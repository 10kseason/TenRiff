#include "app/MenuApp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_set>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#endif

#include "app/GameSession.h"
#include "app/GameplayHudRevisions.h"
#include "app/GraphicsTiming.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "app/MemoryDiagnostics.h"
#include "app/ModeManager.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "app/PersistedRuntimeConfig.h"
#include "app/RuntimeConfigMigration.h"
#include "config/KeycodeMap.h"
#include "gameplay/Replay.h"
#include "render/GameplayMotion.h"
#include "timing/HighResClock.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

constexpr int kSnapshotSongCount = 10;
constexpr int kSongSelectVisibleCardCount = 5;
constexpr int64_t kKeymapCaptureTimeoutNs = 5'000'000'000LL;
constexpr int64_t kSongSelectRepeatInitialDelayNs = 250'000'000LL;
constexpr int64_t kSongSelectRepeatIntervalNs = 45'000'000LL;
constexpr std::size_t kRecentSongSourceLimit = 12;

int detect_active_monitor_refresh_hz(int fallback_hz) {
#ifdef _WIN32
    HMONITOR monitor = nullptr;
    const HWND foreground_window = GetForegroundWindow();
    if (foreground_window && IsWindow(foreground_window)) {
        monitor = MonitorFromWindow(foreground_window, MONITOR_DEFAULTTONEAREST);
    }

    if (!monitor) {
        POINT point{0, 0};
        if (!GetCursorPos(&point)) {
            return clamp_graphics_refresh_hz(fallback_hz);
        }
        monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    }
    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
        return clamp_graphics_refresh_hz(fallback_hz);
    }

    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &mode) &&
        mode.dmDisplayFrequency > 0) {
        return clamp_graphics_refresh_hz(static_cast<int>(mode.dmDisplayFrequency));
    }
#endif
    return clamp_graphics_refresh_hz(fallback_hz);
}

std::filesystem::path path_from_utf8(std::string_view value) {
    try {
        return util::path_from_utf8_lossy(value);
    } catch (...) {
        return {};
    }
}

#ifdef _WIN32
std::optional<std::filesystem::path> executable_directory_path() {
    wchar_t buffer[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
    if (length == 0 || length >= (sizeof(buffer) / sizeof(buffer[0]))) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer).parent_path();
}
#endif

std::string resolve_menu_music_file_path(std::string_view filename) {
    if (filename.empty()) {
        return {};
    }

    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    auto push_root = [&](const fs::path& root) {
        if (root.empty() || std::find(roots.begin(), roots.end(), root) != roots.end()) {
            return;
        }
        roots.push_back(root);
    };

    std::error_code ec;
    const fs::path current = fs::current_path(ec);
    if (!ec && !current.empty()) {
        push_root(current);
        push_root(current.parent_path());
        push_root(current.parent_path().parent_path());
    }

#ifdef _WIN32
    if (auto exe_dir = executable_directory_path(); exe_dir.has_value()) {
        push_root(*exe_dir);
        push_root(exe_dir->parent_path());
        push_root(exe_dir->parent_path().parent_path());
    }
#endif

    for (const auto& root : roots) {
        const fs::path candidate = root / "Mainmusic" / path_from_utf8(filename);
        ec.clear();
        if (!fs::is_regular_file(candidate, ec)) {
            continue;
        }

        const fs::path canonical = fs::weakly_canonical(candidate, ec);
        if (!ec && !canonical.empty()) {
            return canonical.u8string();
        }
        return candidate.lexically_normal().u8string();
    }
    return {};
}

std::string safe_ui_text(std::string_view value, std::string_view fallback = {}) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return std::string(fallback);
}

std::string safe_ui_text_or_placeholder(std::string_view value, std::string_view placeholder) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return value.empty() ? std::string{} : std::string(placeholder);
}

std::string song_title_for_ui(const SongEntry& entry) {
    std::string title = util::sanitize_ui_text(entry.title);
    if (!title.empty()) {
        return title;
    }
    std::string path = util::sanitize_ui_text(entry.path);
    if (!path.empty()) {
        return path;
    }
    return "<invalid title>";
}

std::string song_artist_for_ui(const SongEntry& entry) {
    return safe_ui_text_or_placeholder(entry.artist, "<invalid artist>");
}

std::string song_index_stage_label(SongIndexProgressStage stage) {
    switch (stage) {
    case SongIndexProgressStage::ScanningFiles:
        return "SCANNING FILES";
    case SongIndexProgressStage::BuildingMetadata:
        return "BUILDING METADATA";
    case SongIndexProgressStage::SavingCache:
        return "WRITING CACHE";
    default:
        return "INDEXING";
    }
}

SongIndexLoadResult load_song_index_from_available_cache(const std::string& primary_cache_path,
                                                         const std::string& legacy_cache_path,
                                                         const SongIndexOptions& options) {
    SongIndexLoadResult primary_result = load_song_index(primary_cache_path, options);
    if (primary_result.success() && primary_result.loaded_from_file) {
        return primary_result;
    }

    if (legacy_cache_path.empty() || legacy_cache_path == primary_cache_path) {
        return primary_result;
    }

    SongIndexLoadResult legacy_result = load_song_index(legacy_cache_path, options);
    if (!legacy_result.success() || !legacy_result.loaded_from_file) {
        return primary_result;
    }

    if (!primary_result.error.empty()) {
        legacy_result.warnings.push_back("Primary profile-local song index cache ignored: " + primary_result.error);
    }

    std::string migrate_error;
    if (!save_song_index(primary_cache_path, legacy_result.index, options, &migrate_error)) {
        legacy_result.warnings.push_back(
            "Loaded legacy song index cache but failed to migrate it to profile-local storage: " + migrate_error);
    } else {
        legacy_result.warnings.push_back("Loaded legacy song index cache and migrated it to profile-local storage.");
    }
    return legacy_result;
}

#ifdef _WIN32
std::string browse_for_folder(const std::string& title) {
    std::string result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return result;
    }

    IFileDialog* pFileDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                          IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog));
    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        pFileDialog->GetOptions(&options);
        pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

        std::wstring wide_title;
        if (!title.empty()) {
            int count = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
            if (count > 0) {
                wide_title.resize(static_cast<size_t>(count));
                MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide_title.data(), count);
            }
        }
        if (!wide_title.empty()) {
            pFileDialog->SetTitle(wide_title.c_str());
        }

        hr = pFileDialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszPath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr) && pszPath) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        result.resize(static_cast<size_t>(len));
                        WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, result.data(), len, nullptr, nullptr);
                        if (!result.empty() && result.back() == '\0') {
                            result.pop_back();
                        }
                    }
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pFileDialog->Release();
    }

    CoUninitialize();
    return result;
}
#endif

std::string song_layout_label(const SongEntry& entry) {
    if (!entry.layout_label.empty()) {
        return entry.layout_label;
    }
    return key_mode_label(std::to_string(std::max(1, entry.key_count)) + "k");
}

std::string song_detail_label(const SongEntry& entry) {
    std::string detail = song_layout_label(entry) + " " + format_label(to_lower_ascii(entry.format));
    const std::string chart_name = safe_ui_text(entry.chart_name);
    if (!chart_name.empty()) {
        detail += " / " + chart_name;
    }
    return detail;
}

std::string gauge_type_label(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::Hard: return "HARD";
        case game::GaugeType::Easy: return "EASY";
        case game::GaugeType::Normal:
        default: return "NORMAL";
    }
}

std::string judgement_label(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return "PG";
        case game::Judgement::GR: return "GR";
        case game::Judgement::GD: return "G";
        case game::Judgement::BD: return "BAD";
        case game::Judgement::PR:
        default: return "BAD";
    }
}

std::string format_eta_seconds(int64_t seconds) {
    if (seconds <= 0) {
        return "0s";
    }
    const int64_t hours = seconds / 3600;
    const int64_t minutes = (seconds % 3600) / 60;
    const int64_t secs = seconds % 60;
    if (hours > 0) {
        return std::to_string(hours) + "h" + std::to_string(minutes) + "m";
    }
    if (minutes > 0) {
        return std::to_string(minutes) + "m" + std::to_string(secs) + "s";
    }
    return std::to_string(secs) + "s";
}

std::string format_int_with_commas(int64_t value) {
    const bool negative = value < 0;
    uint64_t abs_value = 0;
    if (negative) {
        abs_value = static_cast<uint64_t>(-(value + 1)) + 1;
    } else {
        abs_value = static_cast<uint64_t>(value);
    }
    std::string digits = std::to_string(abs_value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count == 3) {
            out.push_back(',');
            count = 0;
        }
        out.push_back(*it);
        ++count;
    }
    if (negative) {
        out.push_back('-');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool song_entry_matches_chart_filter(const SongEntry& entry, std::string_view filter) {
    const std::string normalized_filter = normalize_chart_filter(std::string(filter));
    if (normalized_filter == "auto") {
        return true;
    }
    return to_lower_ascii(entry.format) == normalized_filter;
}

std::string song_sort_title_key(const SongEntry& entry) {
    const std::string display = entry.title.empty() ? entry.path : entry.title;
    return to_lower_ascii(display);
}

bool song_entry_less_by_difficulty_asc(const SongEntry& lhs, const SongEntry& rhs) {
    const int lhs_level = lhs.level > 0 ? lhs.level : 9999;
    const int rhs_level = rhs.level > 0 ? rhs.level : 9999;
    if (lhs_level != rhs_level) {
        return lhs_level < rhs_level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating < rhs.rating;
    }
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_difficulty_desc(const SongEntry& lhs, const SongEntry& rhs) {
    if (lhs.level != rhs.level) {
        return lhs.level > rhs.level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating > rhs.rating;
    }
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_title_asc(const SongEntry& lhs, const SongEntry& rhs) {
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    const int lhs_level = lhs.level > 0 ? lhs.level : 9999;
    const int rhs_level = rhs.level > 0 ? rhs.level : 9999;
    if (lhs_level != rhs_level) {
        return lhs_level < rhs_level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating < rhs.rating;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_title_desc(const SongEntry& lhs, const SongEntry& rhs) {
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title > rhs_title;
    }
    if (lhs.level != rhs.level) {
        return lhs.level > rhs.level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating > rhs.rating;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_matches_search(const SongEntry& entry, std::string_view query) {
    const std::string normalized_query = to_lower_ascii(std::string(query));
    if (normalized_query.empty()) {
        return true;
    }
    const std::string haystacks[] = {
        to_lower_ascii(song_title_for_ui(entry)),
        to_lower_ascii(song_artist_for_ui(entry)),
        to_lower_ascii(entry.path),
    };
    for (const auto& haystack : haystacks) {
        if (haystack.find(normalized_query) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool song_entry_matches_key_filter(const SongEntry& entry, int key_filter) {
    return key_filter <= 0 || entry.key_count == key_filter;
}

bool song_entry_matches_level_filter(const SongEntry& entry, int level_min, int level_max) {
    if (level_min <= 0 && level_max <= 0) {
        return true;
    }
    if (entry.level <= 0) {
        return false;
    }
    if (level_min > 0 && entry.level < level_min) {
        return false;
    }
    if (level_max > 0 && entry.level > level_max) {
        return false;
    }
    return true;
}

std::string key_filter_label(int key_filter) {
    return key_filter <= 0 ? "All Keys" : key_mode_label(std::to_string(key_filter) + "k");
}

int cycle_key_filter_value(int key_filter, int direction) {
    static constexpr int kKeyFilters[] = {0, 4, 5, 6, 7, 8, 9, 10, 16};
    const int option_count = static_cast<int>(sizeof(kKeyFilters) / sizeof(kKeyFilters[0]));
    int index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (key_filter == kKeyFilters[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = option_count - 1;
    } else if (index >= option_count) {
        index = 0;
    }
    return kKeyFilters[index];
}

std::string level_filter_label(int level_min, int level_max) {
    if (level_min <= 0 && level_max <= 0) {
        return "All Levels";
    }
    if (level_min > 0 && level_max > 0) {
        return "LV " + std::to_string(level_min) + "-" + std::to_string(level_max);
    }
    if (level_min > 0) {
        return "LV " + std::to_string(level_min) + "+";
    }
    return "LV <= " + std::to_string(level_max);
}

std::string song_sort_detail_label(MenuApp::SongSortMode mode) {
    switch (mode) {
        case MenuApp::SongSortMode::DifficultyDesc: return "LV DESC";
        case MenuApp::SongSortMode::TitleAsc: return "A-Z";
        case MenuApp::SongSortMode::TitleDesc: return "Z-A";
        case MenuApp::SongSortMode::DifficultyAsc:
        default: return "LV ASC";
    }
}

MenuApp::SongSortMode toggle_difficulty_sort(MenuApp::SongSortMode mode) {
    if (mode == MenuApp::SongSortMode::DifficultyAsc) {
        return MenuApp::SongSortMode::DifficultyDesc;
    }
    if (mode == MenuApp::SongSortMode::DifficultyDesc) {
        return MenuApp::SongSortMode::DifficultyAsc;
    }
    return MenuApp::SongSortMode::DifficultyAsc;
}

MenuApp::SongSortMode toggle_title_sort(MenuApp::SongSortMode mode) {
    if (mode == MenuApp::SongSortMode::TitleAsc) {
        return MenuApp::SongSortMode::TitleDesc;
    }
    if (mode == MenuApp::SongSortMode::TitleDesc) {
        return MenuApp::SongSortMode::TitleAsc;
    }
    return MenuApp::SongSortMode::TitleAsc;
}

std::string browser_summary_label(std::string_view query, int key_filter, int level_min, int level_max) {
    std::vector<std::string> parts;
    if (!query.empty()) {
        parts.push_back("Q " + safe_ui_text(query));
    }
    static_cast<void>(key_filter);
    if (level_min > 0 || level_max > 0) {
        parts.push_back(level_filter_label(level_min, level_max));
    }
    if (parts.empty()) {
        return "NO FILTER";
    }
    std::string joined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            joined += " / ";
        }
        joined += parts[i];
    }
    return joined;
}

std::optional<char> search_character_from_keycode(uint32_t keycode) {
    const std::string name = config::KeycodeMap::to_name(keycode);
    if (name.size() == 1) {
        const char ch = name[0];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }
    if (name == "Space") return ' ';
    if (name == "Minus") return '-';
    if (name == "Period") return '.';
    if (name == "Comma") return ',';
    if (name == "Slash") return '/';
    if (name == "Apostrophe") return '\'';
    if (name == "LBracket") return '[';
    if (name == "RBracket") return ']';
    return std::nullopt;
}

game::GaugeType gauge_type_from_mode_string(std::string_view value) {
    if (value == "hard") {
        return game::GaugeType::Hard;
    }
    if (value == "easy") {
        return game::GaugeType::Easy;
    }
    return game::GaugeType::Normal;
}

std::string short_gauge_type_label(game::GaugeType type) {
    switch (type) {
        case game::GaugeType::Hard: return "H";
        case game::GaugeType::Easy: return "E";
        case game::GaugeType::Normal:
        default: return "N";
    }
}

std::string format_signed_ms(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(2);
    if (value >= 0.0) {
        stream << '+';
    }
    stream << value << "ms";
    return stream.str();
}

std::string filename_only(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    return path_from_utf8(path).filename().u8string();
}

}  // namespace

MenuApp::MenuApp() = default;

GameplayHudRevisionInput MenuApp::gameplay_hud_revision_input(const GameplayHudState& state) {
    GameplayHudRevisionInput input;
    input.active = state.active;
    input.finished = state.finished;
    input.game_over = state.game_over;
    input.user_aborted = state.user_aborted;
    input.loading = state.loading;
    input.countdown_active = state.countdown_active;
    input.countdown_value = state.countdown_value;
    input.loading_percent = state.loading_percent;
    input.loading_stage = state.loading_stage;
    input.lane_count = state.lane_count;
    input.current_sample = state.current_sample;
    input.duration_samples = state.duration_samples;
    input.sample_rate = state.sample_rate;
    input.audio_sample_time_ns = state.audio_sample_time_ns;
    input.audio_buffer_frames = state.audio_buffer_frames;
    input.lookahead_samples = state.lookahead_samples;
    input.past_samples = state.past_samples;
    input.combo = state.combo;
    input.max_combo = state.max_combo;
    input.counts = state.counts;
    input.score = state.score;
    input.gauge = state.gauge;
    input.gauge_type = state.gauge_type;
    input.rate = state.rate;
    input.hispeed = state.hispeed;
    input.has_feedback = state.has_feedback;
    input.feedback = state.feedback;
    input.feedback_delta_ms = state.feedback_delta_ms;
    input.lane_activity_count = state.lane_activity_count;
    std::copy_n(state.lane_activity.begin(), state.lane_activity_count, input.lane_activity.begin());
    input.note_count = state.note_count;
    for (std::size_t i = 0; i < state.note_count; ++i) {
        input.notes[i] = GameplayHudRevisionNote{
            state.notes[i].lane,
            state.notes[i].start_sample,
            state.notes[i].tail_sample,
            state.notes[i].hold,
            state.notes[i].head_visible,
        };
    }
    return input;
}

void MenuApp::advance_gameplay_hud_revisions(GameplayHudState& state, bool motion_changed, bool text_changed) {
    if (motion_changed) {
        ++state.motion_revision;
    }
    if (text_changed) {
        ++state.text_revision;
    }
}

void MenuApp::reset_gameplay_hud_state(GameplayHudState& state, bool preserve_loading) {
    const uint64_t next_motion_revision = state.motion_revision + 1;
    const uint64_t next_text_revision = state.text_revision + 1;
    const bool loading = preserve_loading ? state.loading : false;
    const int loading_percent = preserve_loading ? state.loading_percent : 0;
    const std::string loading_stage = preserve_loading ? state.loading_stage : std::string{};

    state = {};
    state.loading = loading;
    state.loading_percent = loading_percent;
    state.loading_stage = loading_stage;
    state.motion_revision = next_motion_revision;
    state.text_revision = next_text_revision;
}

MenuApp::~MenuApp() {
    shutdown();
}

bool MenuApp::initialize(const CommandLineOptions& options) {
    exit_code_ = 0;
    options_ = options;
    songs_path_ = menu_songs::normalize_song_source_path(options.songs_path);
    if (songs_path_.empty()) {
        songs_path_ = options.songs_path;
    }

    const std::filesystem::path profile_dir = path_from_utf8("profiles") / path_from_utf8(options.profile);
    profile_dir_ = profile_dir.u8string();
    cache_path_ = song_index_cache_path_for_source(profile_dir_, songs_path_);

    config::ConfigLoader config_loader;
    auto config_result = config_loader.load_profile(profile_dir_);
    if (!config_result.success()) {
        return false;
    }
    config_ = config_result.config;
    const bool migrated_config = migrate_bms_first_runtime_config(config_);
    const bool stripped_session_only_mods = strip_session_only_mode_mods(config_);
    first_run_profile_ = config_result.used_defaults;

    if (config_result.used_defaults || migrated_config || stripped_session_only_mods) {
        const config::RuntimeConfig persisted = build_persisted_runtime_config(config_);
        config_loader.save_profile(profile_dir_, persisted);
    }
    config_.graphics.refresh_hz =
        clamp_int(config_.graphics.refresh_hz, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
    config_.graphics.resolution = normalize_resolution_preset(config_.graphics.resolution);
    refresh_available_osu_skins();
    refresh_available_lr2_skins();

    config::KeymapManager keymap_manager;
    auto keymap_result = keymap_manager.load_profile(profile_dir_);
    if (!keymap_result.success()) {
        return false;
    }
    keymap_ = keymap_result.keymap;

    if (keymap_result.used_defaults) {
        keymap_manager.save_profile(profile_dir_, keymap_);
    }
    first_run_profile_ = first_run_profile_ || keymap_result.used_defaults;

    key_up_ = config::KeycodeMap::to_keycode("Up").value_or(0);
    key_down_ = config::KeycodeMap::to_keycode("Down").value_or(0);
    key_left_ = config::KeycodeMap::to_keycode("Left").value_or(0);
    key_right_ = config::KeycodeMap::to_keycode("Right").value_or(0);
    key_page_up_ = config::KeycodeMap::to_keycode("PageUp").value_or(0);
    key_page_down_ = config::KeycodeMap::to_keycode("PageDown").value_or(0);
    key_enter_ = config::KeycodeMap::to_keycode("Enter").value_or(0);
    key_escape_ = config::KeycodeMap::to_keycode("Esc").value_or(0);
    key_backspace_ = config::KeycodeMap::to_keycode("Backspace").value_or(0);
    key_delete_ = config::KeycodeMap::to_keycode("Delete").value_or(0);
    key_a_ = config::KeycodeMap::to_keycode("A").value_or(0);
    key_g_ = config::KeycodeMap::to_keycode("G").value_or(0);
    key_i_ = config::KeycodeMap::to_keycode("I").value_or(0);
    key_m_ = config::KeycodeMap::to_keycode("M").value_or(0);
    key_k_ = config::KeycodeMap::to_keycode("K").value_or(0);
    key_r_ = config::KeycodeMap::to_keycode("R").value_or(0);
    key_f1_ = config::KeycodeMap::to_keycode("F1").value_or(0);
    key_f2_ = config::KeycodeMap::to_keycode("F2").value_or(0);
    key_f5_ = config::KeycodeMap::to_keycode("F5").value_or(0);

    {
        config::KeymapManager keymap_manager;
        keymap_edit_mode_ = keymap_manager.normalize_mode_token(config_.mode.key_mode);
    }
    refresh_keymap_lane_list();

    std::string initial_song_source = songs_path_;
    if (!config_.ui.active_song_source.empty()) {
        std::error_code ec;
        const auto active_path = path_from_utf8(config_.ui.active_song_source);
        if (std::filesystem::is_directory(active_path, ec)) {
            initial_song_source = menu_songs::normalize_song_source_path(config_.ui.active_song_source);
        }
    } else {
        for (const auto& recent_source : config_.ui.recent_song_sources) {
            if (recent_source.empty()) {
                continue;
            }
            std::error_code ec;
            const auto recent_path = path_from_utf8(recent_source);
            if (std::filesystem::is_directory(recent_path, ec)) {
                initial_song_source = menu_songs::normalize_song_source_path(recent_source);
                break;
            }
        }
    }

    switch_song_source(initial_song_source, false);
    if (first_run_profile_) {
        screen_ = Screen::QuickSetup;
        settings_cursor_ = 0;
    }

    start_menu_threads();
    publish_snapshot();

    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(3);
    while (!menu_window_.init_done()) {
        if (menu_window_.had_fatal_error() || menu_window_.should_close()) {
            break;
        }
        if (std::chrono::steady_clock::now() - start > timeout) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (menu_window_.had_fatal_error() || menu_window_.should_close()) {
        std::cerr << "[error] Menu renderer failed to initialize." << std::endl;
        exit_code_ = 13;
        return false;
    }

    if (!menu_window_.init_done()) {
        std::cerr << "[error] Menu renderer initialization timed out." << std::endl;
        exit_code_ = 13;
        return false;
    }

    if (!menu_window_.init_success()) {
        std::cerr << "[error] Menu renderer failed to initialize." << std::endl;
        exit_code_ = 13;
        return false;
    }

    return true;
}

void MenuApp::run() {
    while (!exit_requested_.load(std::memory_order_acquire)) {
        if (menu_window_.should_close()) {
            exit_requested_.store(true, std::memory_order_release);
            break;
        }
        while (true) {
            auto event = input_thread_.queue().pop();
            if (!event.has_value()) {
                break;
            }
            handle_input_event(event.value());
        }
        while (true) {
            auto click = menu_window_.poll_click_event();
            if (!click.has_value()) {
                break;
            }
            handle_menu_click(click.value());
        }

        SongIndex updated;
        std::vector<std::string> warnings;
        if (song_indexer_.poll_result(updated, warnings)) {
            update_song_list(std::move(updated));
            for (const auto& warning : warnings) {
                std::cerr << "[warn] " << warning << std::endl;
            }
            publish_snapshot();
        }

        update_keymap_capture_timeout();
        update_song_select_repeat();

        if (screen_ == Screen::SongSelect && song_indexer_.is_running()) {
            const int64_t now_ns = timing::HighResClock::now_ns();
            if (now_ns - last_indexer_snapshot_ns_ >= 200'000'000LL) {
                last_indexer_snapshot_ns_ = now_ns;
                publish_snapshot();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (exit_code_ == 0 && menu_window_.had_fatal_error()) {
        exit_code_ = 13;
    }
}

void MenuApp::shutdown() {
    exit_requested_.store(true, std::memory_order_release);
    stop_menu_threads();
    song_indexer_.stop();
}

void MenuApp::start_menu_threads() {
    input::InputThreadConfig input_config;
    input_config.backend = config_.input.rawinput ? input::InputBackend::RawInput
                                                  : input::InputBackend::Polling;
    input_config.raw_input.register_keyboard = config_.input.rawinput;
    input_config.raw_input.input_sink = true;
    input_config.raw_input.no_legacy = false;
    input_config.polling_hz = config_.input.polling_hz;
    input_config.key_state.debounce_window_ns =
        std::max<int64_t>(0, static_cast<int64_t>(std::llround(config_.input.debounce_ms * 1'000'000.0)));

    if (!input_thread_.initialize(input_config)) {
        std::cerr << "[error] Failed to initialize input thread." << std::endl;
    } else {
        (void)input_thread_.start();
    }

    const render::RenderConfig render_config = current_render_config();
    menu_window_.set_config(current_window_config());
    if (!render_thread_.initialize(render_config, [this]() {
        render_tick();
    }, [this]() {
        menu_window_.shutdown();
    })) {
        std::cerr << "[error] Failed to initialize render thread." << std::endl;
        return;
    }
    (void)render_thread_.start();
}

void MenuApp::stop_menu_threads() {
    render_thread_.stop();
    menu_music_.stop();
    audio_thread_.shutdown();
    input_thread_.stop();
}

void MenuApp::restart_input_thread() {
    input_thread_.shutdown();
    input::InputThreadConfig input_config;
    input_config.backend = config_.input.rawinput ? input::InputBackend::RawInput
                                                  : input::InputBackend::Polling;
    input_config.raw_input.register_keyboard = config_.input.rawinput;
    input_config.raw_input.input_sink = true;
    input_config.raw_input.no_legacy = false;
    input_config.polling_hz = config_.input.polling_hz;
    input_config.key_state.debounce_window_ns =
        std::max<int64_t>(0, static_cast<int64_t>(std::llround(config_.input.debounce_ms * 1'000'000.0)));
    if (!input_thread_.initialize(input_config)) {
        std::cerr << "[error] Failed to reinitialize input thread." << std::endl;
        return;
    }
    (void)input_thread_.start();
}

void MenuApp::restart_audio_thread() {
    audio_thread_.shutdown();
}

void MenuApp::restart_render_thread() {
    render_thread_.shutdown();
    const render::RenderConfig render_config = current_render_config();
    menu_window_.set_config(current_window_config());
    if (!render_thread_.initialize(render_config, [this]() {
        render_tick();
    }, [this]() {
        menu_window_.shutdown();
    })) {
        std::cerr << "[error] Failed to reinitialize render thread." << std::endl;
        return;
    }
    (void)render_thread_.start();
}

render::RenderConfig MenuApp::current_render_config() const {
    render::RenderConfig render_config;
    render_config.vsync = config_.graphics.vsync;
    render_config.fps_limit = effective_render_fps_limit();
    return render_config;
}

int MenuApp::effective_refresh_hz() const {
    return effective_configured_refresh_hz(config_.graphics.refresh_hz,
                                           screen_ == Screen::Gameplay);
}

int MenuApp::effective_present_refresh_hz() const {
    const int detected_monitor_refresh_hz =
        detect_active_monitor_refresh_hz(config_.graphics.refresh_hz);
    return ::tenriff::app::effective_present_refresh_hz(config_.graphics.vsync,
                                                        config_.graphics.refresh_hz,
                                                        detected_monitor_refresh_hz,
                                                        screen_ == Screen::Gameplay);
}

int MenuApp::effective_render_fps_limit() const {
    const int detected_monitor_refresh_hz =
        detect_active_monitor_refresh_hz(config_.graphics.refresh_hz);
    return ::tenriff::app::effective_render_fps_limit(config_.graphics.vsync,
                                                      config_.graphics.refresh_hz,
                                                      detected_monitor_refresh_hz,
                                                      screen_ == Screen::Gameplay);
}

render::MenuWindowConfig MenuApp::current_window_config() const {
    render::MenuWindowConfig window_config;
    const auto [width, height] = resolution_dimensions(config_.graphics.resolution);
    window_config.title = "TenRiff";
    window_config.display_mode = config_.graphics.display_mode;
    window_config.vsync = config_.graphics.vsync;
    window_config.refresh_hz = effective_present_refresh_hz();
    window_config.width = width;
    window_config.height = height;
    return window_config;
}

void MenuApp::apply_runtime_graphics_config() {
    menu_window_.set_config(current_window_config());
    render_thread_.update_config(current_render_config());
}

bool MenuApp::remember_song_source(const std::string& source_path) {
    const std::string normalized = menu_songs::normalize_song_source_path(source_path);
    if (normalized.empty()) {
        return false;
    }

    const std::string normalized_key = menu_songs::normalize_path_key(path_from_utf8(normalized));
    std::vector<std::string> updated_sources;
    updated_sources.reserve(std::min<std::size_t>(config_.ui.recent_song_sources.size() + 1, kRecentSongSourceLimit));
    updated_sources.push_back(normalized);

    for (const auto& existing : config_.ui.recent_song_sources) {
        const std::string normalized_existing = menu_songs::normalize_song_source_path(existing);
        if (normalized_existing.empty()) {
            continue;
        }
        if (menu_songs::normalize_path_key(path_from_utf8(normalized_existing)) == normalized_key) {
            continue;
        }
        updated_sources.push_back(normalized_existing);
        if (updated_sources.size() >= kRecentSongSourceLimit) {
            break;
        }
    }

    bool changed = (config_.ui.active_song_source != normalized) ||
                   (config_.ui.recent_song_sources.size() != updated_sources.size());
    if (!changed) {
        for (std::size_t i = 0; i < updated_sources.size(); ++i) {
            if (config_.ui.recent_song_sources[i] != updated_sources[i]) {
                changed = true;
                break;
            }
        }
    }

    config_.ui.active_song_source = normalized;
    config_.ui.recent_song_sources = std::move(updated_sources);
    selected_source_ = 0;
    return changed;
}

void MenuApp::persist_runtime_config() {
    config::ConfigLoader loader;
    std::string error;
    const config::RuntimeConfig persisted = build_persisted_runtime_config(config_);
    if (!loader.save_profile(profile_dir_, persisted, &error)) {
        std::cerr << "[error] " << error << std::endl;
    }
}

void MenuApp::refresh_keymap_lane_list() {
    config::KeymapManager manager;
    keymap_edit_mode_ = manager.normalize_mode_token(keymap_edit_mode_);
    keymap_lanes_ = manager.lane_ids_for_mode(keymap_edit_mode_);
    const int max_cursor = static_cast<int>(keymap_lanes_.size());
    keymap_cursor_ = clamp_int(keymap_cursor_, 0, max_cursor);
}

void MenuApp::refresh_song_source(bool force_reindex) {
    switch_song_source(songs_path_, force_reindex);
}

void MenuApp::switch_song_source(const std::string& new_songs_path, bool force_reindex) {
    if (new_songs_path.empty()) {
        return;
    }

    const std::string normalized_source = menu_songs::normalize_song_source_path(new_songs_path);
    songs_path_ = normalized_source.empty() ? new_songs_path : normalized_source;
    song_select_view_ = SongSelectView::Songs;
    const bool source_history_changed = remember_song_source(songs_path_);
    cache_path_ = song_index_cache_path_for_source(profile_dir_, songs_path_);
    const std::string legacy_cache_path = legacy_song_index_cache_path_for_source(songs_path_);
    last_indexer_snapshot_ns_ = 0;
    song_indexer_.stop();
    SongIndexOptions index_options;
    index_options.include_osu = config_.mode.enable_osu_charts;
    index_options.profile = (config::normalize_song_index_profile_token(config_.mode.song_index_profile) == "fast")
                                ? SongIndexProfile::Fast
                                : SongIndexProfile::Safe;

    log_memory_phase("MenuApp",
                     "cache-load-before",
                     query_process_memory_snapshot(),
                     "source=" + safe_ui_text_or_placeholder(songs_path_, "<invalid path>"));
    auto cache_result = load_song_index_from_available_cache(cache_path_, legacy_cache_path, index_options);
    if (cache_result.success() && cache_result.loaded_from_file) {
        const int cached_count = static_cast<int>(cache_result.index.entries.size());
        update_song_list(std::move(cache_result.index));
        source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] = cached_count;
    } else {
        update_song_list(SongIndex{});
        source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] = 0;
        if (!cache_result.error.empty()) {
            std::cerr << "[warn] " << cache_result.error << std::endl;
        }
    }
    log_memory_phase("MenuApp",
                     "cache-load-after",
                     query_process_memory_snapshot(),
                     "entries=" + std::to_string(indexed_songs_.size()) +
                         " visible=" + std::to_string(visible_song_count()));

    for (const auto& warning : cache_result.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    reload_chart_best_results();

    if (force_reindex || !cache_result.success() || !cache_result.loaded_from_file) {
        std::cerr << "[info] Indexing song source: " << songs_path_ << std::endl;
        (void)song_indexer_.start(songs_path_, cache_path_, index_options);
    }

    if (source_history_changed) {
        persist_runtime_config();
    }

    sync_song_select_state();
}

void MenuApp::handle_input_event(const input::InputEvent& event) {
    update_pressed_keys(event);

    if (screen_ == Screen::Keymap && keymap_capture_active_) {
        if (event.state == input::InputState::Pressed) {
            apply_keymap_capture(event.keycode);
        }
        return;
    }

    if (event.state != input::InputState::Pressed) {
        return;
    }

    if (screen_ != Screen::Gameplay && key_f1_ != 0 && event.keycode == key_f1_) {
        help_overlay_visible_ = !help_overlay_visible_;
        publish_snapshot();
        return;
    }
    if (help_overlay_visible_) {
        if (event.keycode == key_escape_ || event.keycode == key_backspace_) {
            help_overlay_visible_ = false;
            publish_snapshot();
        }
        return;
    }

    switch (screen_) {
        case Screen::QuickSetup:
            handle_quick_setup_input(event.keycode);
            break;
        case Screen::Title:
            handle_title_input(event.keycode);
            break;
        case Screen::OptionsHub:
            handle_options_hub_input(event.keycode);
            break;
        case Screen::EditStub:
            handle_edit_stub_input(event.keycode);
            break;
        case Screen::SongSelect:
            handle_song_select_input(event.keycode);
            break;
        case Screen::SongBrowser:
            handle_song_browser_input(event.keycode);
            break;
        case Screen::Gameplay:
            break;
        case Screen::SettingsAudio:
            handle_audio_settings_input(event.keycode);
            break;
        case Screen::SettingsGraphics:
            handle_graphics_settings_input(event.keycode);
            break;
        case Screen::SettingsSkins:
            handle_skins_settings_input(event.keycode);
            break;
        case Screen::SettingsInput:
            handle_input_settings_input(event.keycode);
            break;
        case Screen::ModeSelect:
            handle_mode_settings_input(event.keycode);
            break;
        case Screen::ModeMods:
            handle_mode_mods_input(event.keycode);
            break;
        case Screen::Keymap:
            handle_keymap_input(event.keycode);
            break;
        case Screen::KeymapConfirm:
            handle_keymap_confirm_input(event.keycode);
            break;
        case Screen::KeymapTest:
            handle_keymap_test_input(event.keycode);
            break;
        case Screen::Result:
            handle_result_input(event.keycode);
            break;
    }
}

void MenuApp::handle_menu_click(const render::MenuClickEvent& event) {
    if (screen_ == Screen::Gameplay) {
        return;
    }
    if (help_overlay_visible_) {
        help_overlay_visible_ = false;
        publish_snapshot();
        return;
    }
    if (screen_ == Screen::Keymap && keymap_capture_active_) {
        return;
    }
    if (screen_ == Screen::SongSelect) {
        sync_song_select_state();
    }
    if (event.kind == render::MenuHitTargetKind::MouseWheel) {
        if (screen_ == Screen::SongSelect && event.wheel_steps != 0) {
            song_select_focus_ = SongSelectFocus::SongList;
            if (move_song_select_selection(-event.wheel_steps)) {
                publish_snapshot();
            }
        } else if (event.wheel_steps != 0) {
            const uint32_t direction_key = event.wheel_steps > 0 ? key_up_ : key_down_;
            const int steps = std::abs(event.wheel_steps);
            for (int i = 0; i < steps; ++i) {
                switch (screen_) {
                    case Screen::QuickSetup:
                        handle_quick_setup_input(direction_key);
                        break;
                    case Screen::OptionsHub:
                        handle_options_hub_input(direction_key);
                        break;
                    case Screen::SongBrowser:
                        handle_song_browser_input(direction_key);
                        break;
                    case Screen::SettingsAudio:
                        handle_audio_settings_input(direction_key);
                        break;
                    case Screen::SettingsGraphics:
                        handle_graphics_settings_input(direction_key);
                        break;
                    case Screen::SettingsSkins:
                        handle_skins_settings_input(direction_key);
                        break;
                    case Screen::SettingsInput:
                        handle_input_settings_input(direction_key);
                        break;
                    case Screen::ModeSelect:
                        handle_mode_settings_input(direction_key);
                        break;
                    case Screen::ModeMods:
                        handle_mode_mods_input(direction_key);
                        break;
                    default:
                        break;
                }
            }
        }
        return;
    }
    if (event.kind == render::MenuHitTargetKind::SongScrollbar) {
        if (screen_ != Screen::SongSelect) {
            return;
        }
        song_select_focus_ = SongSelectFocus::SongList;
        if (song_select_view_ == SongSelectView::Sources) {
            if (!config_.ui.recent_song_sources.empty()) {
                selected_source_ = clamp_int(event.index, 0, static_cast<int>(config_.ui.recent_song_sources.size() - 1));
                publish_snapshot();
            }
            return;
        }
        if (song_select_view_ == SongSelectView::Records) {
            rebuild_current_song_record_indices();
            if (!current_song_record_indices_.empty()) {
                selected_record_ =
                    clamp_int(event.index, 0, static_cast<int>(current_song_record_indices_.size() - 1));
                publish_snapshot();
            }
            return;
        }
        if (visible_song_count() > 0) {
            selected_song_ = clamp_int(event.index, 0, static_cast<int>(visible_song_count() - 1));
            publish_snapshot();
        }
        return;
    }
    if (event.kind == render::MenuHitTargetKind::FileDrop) {
        if (screen_ == Screen::SettingsSkins) {
            if (!import_skin_path_auto(event.path)) {
                std::cerr << "[warn] Ignored dropped path (expected an osu!mania or LR2 skin folder, or a folder containing skins): "
                          << event.path << std::endl;
                return;
            }
            publish_snapshot();
            return;
        }
        auto dropped_source = menu_songs::normalize_dropped_song_source(event.path);
        if (!dropped_source.has_value()) {
            std::cerr << "[warn] Ignored dropped path (expected a folder or supported chart file): " << event.path
                      << std::endl;
            return;
        }
        switch_song_source(dropped_source.value(), false);
        if (screen_ == Screen::QuickSetup) {
            publish_snapshot();
            return;
        }
        screen_ = Screen::SongSelect;
        song_select_focus_ = SongSelectFocus::SongList;
        song_select_view_ = SongSelectView::Songs;
        publish_snapshot();
        return;
    }
    if (event.kind == render::MenuHitTargetKind::None || event.index < 0) {
        return;
    }

    switch (event.kind) {
        case render::MenuHitTargetKind::TitleButton:
            if (screen_ != Screen::Title) {
                return;
            }
            if (event.part == render::MenuHitPart::Decrement) {
                title_cursor_ = clamp_int(event.index - 1, 0, 3);
                publish_snapshot();
                return;
            }
            title_cursor_ = clamp_int(event.index, 0, 3);
            handle_title_input(key_enter_);
            return;
        case render::MenuHitTargetKind::OptionsItem:
            if (screen_ != Screen::OptionsHub) {
                return;
            }
            if (event.part == render::MenuHitPart::Decrement) {
                options_cursor_ = clamp_int(event.index - 1, 0, 6);
                publish_snapshot();
                return;
            }
            options_cursor_ = clamp_int(event.index, 0, 6);
            handle_options_hub_input(key_enter_);
            return;
        case render::MenuHitTargetKind::SongNavButton:
            if (screen_ != Screen::SongSelect) {
                return;
            }
            song_select_focus_ = SongSelectFocus::LeftNav;
            song_select_nav_cursor_ = clamp_int(
                (event.part == render::MenuHitPart::Decrement) ? (event.index - 1) : event.index,
                0,
                7);
            publish_snapshot();
            if (event.part == render::MenuHitPart::Activate) {
                handle_song_select_input(key_enter_);
            }
            return;
        case render::MenuHitTargetKind::SongCard:
            if (screen_ != Screen::SongSelect) {
                return;
            }
            song_select_focus_ = SongSelectFocus::SongList;
            if (song_select_view_ == SongSelectView::Sources) {
                if (config_.ui.recent_song_sources.empty()) {
                    return;
                }
                selected_source_ = clamp_int(event.index, 0, static_cast<int>(config_.ui.recent_song_sources.size() - 1));
                publish_snapshot();
                if (event.double_click) {
                    switch_song_source(config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)], false);
                    publish_snapshot();
                }
                return;
            }
            if (visible_song_count() == 0) {
                if (song_select_view_ != SongSelectView::Records) {
                    return;
                }
            }
            if (song_select_view_ == SongSelectView::Records) {
                rebuild_current_song_record_indices();
                if (current_song_record_indices_.empty()) {
                    return;
                }
                selected_record_ = clamp_int(event.index, 0, static_cast<int>(current_song_record_indices_.size() - 1));
                publish_snapshot();
                if (event.double_click) {
                    (void)open_selected_record_result();
                    publish_snapshot();
                }
                return;
            }
            selected_song_ = clamp_int(event.index, 0, static_cast<int>(visible_song_count() - 1));
            publish_snapshot();
            if (event.double_click) {
                launch_selected_song();
            }
            return;
        case render::MenuHitTargetKind::SettingsRow:
            break;
        case render::MenuHitTargetKind::KeymapButton:
            if (screen_ != Screen::Keymap) {
                return;
            }
            switch (event.index) {
                case 0:
                    apply_keymap_save();
                    publish_snapshot();
                    break;
                case 1:
                    apply_keymap_reset();
                    publish_snapshot();
                    break;
                case 2:
                    screen_ = Screen::KeymapTest;
                    publish_snapshot();
                    break;
                case 3:
                    exit_keymap_screen();
                    break;
                default:
                    break;
            }
            return;
        default:
            return;
    }

    if (event.kind != render::MenuHitTargetKind::SettingsRow) {
        return;
    }

    if (screen_ == Screen::Result) {
        if (event.index == 1) {
            (void)launch_last_result_replay();
        } else {
            screen_ = Screen::SongSelect;
            publish_snapshot();
        }
        return;
    }

    const uint32_t action_key = (event.part == render::MenuHitPart::Increment)
                                    ? key_right_
                                    : (event.part == render::MenuHitPart::Decrement ? key_left_ : key_enter_);

    switch (screen_) {
        case Screen::QuickSetup:
            settings_cursor_ = clamp_int(event.index, 0, 6);
            handle_quick_setup_input(action_key);
            return;
        case Screen::SettingsAudio:
            settings_cursor_ = clamp_int(event.index, 0, 5);
            handle_audio_settings_input(action_key);
            return;
        case Screen::SettingsGraphics:
            settings_cursor_ = clamp_int(event.index, 0, 6);
            handle_graphics_settings_input(action_key);
            return;
        case Screen::SongBrowser:
            settings_cursor_ = clamp_int(event.index, 0, 5);
            handle_song_browser_input(action_key);
            return;
        case Screen::SettingsSkins:
            settings_cursor_ = clamp_int(event.index, 0, 19);
            handle_skins_settings_input(action_key);
            return;
        case Screen::SettingsInput:
            settings_cursor_ = clamp_int(event.index, 0, 1);
            handle_input_settings_input(action_key);
            return;
        case Screen::ModeSelect:
            settings_cursor_ = clamp_int(event.index, 0, 10);
            handle_mode_settings_input(action_key);
            return;
        case Screen::ModeMods:
            settings_cursor_ = clamp_int(event.index, 0, static_cast<int>(mode_mod_categories().size()));
            handle_mode_mods_input(action_key);
            return;
        case Screen::EditStub:
            handle_edit_stub_input(key_enter_);
            return;
        case Screen::Result:
            handle_result_input(key_enter_);
            return;
        case Screen::KeymapConfirm:
            if (event.index == 0) {
                handle_keymap_confirm_input(key_enter_);
            } else if (event.index == 1) {
                handle_keymap_confirm_input(key_escape_);
            }
            return;
        case Screen::KeymapTest:
            handle_keymap_test_input(key_escape_);
            return;
        default:
            return;
    }
}

void MenuApp::open_keymap_screen(Screen return_screen) {
    submenu_return_screen_ = return_screen;
    working_keymap_ = keymap_;
    {
        config::KeymapManager keymap_manager;
        keymap_edit_mode_ = keymap_manager.normalize_mode_token(config_.mode.key_mode);
    }
    refresh_keymap_lane_list();
    keymap_cursor_ = 0;
    keymap_dirty_ = false;
    keymap_capture_active_ = false;
    keymap_pending_lane_.clear();
    keymap_pending_key_.clear();
    keymap_duplicate_lane_.clear();
    screen_ = Screen::Keymap;
}

bool MenuApp::handle_settings_shortcut(uint32_t keycode, Screen return_screen) {
    auto open_settings = [&](Screen target) {
        submenu_return_screen_ = return_screen;
        screen_ = target;
        settings_cursor_ = 0;
    };

    if (keycode == key_a_) {
        open_settings(Screen::SettingsAudio);
    } else if (keycode == key_g_) {
        open_settings(Screen::SettingsGraphics);
    } else if (keycode == key_i_) {
        open_settings(Screen::SettingsInput);
    } else if (keycode == key_m_) {
        open_settings(Screen::ModeSelect);
    } else if (keycode == key_k_) {
        open_keymap_screen(return_screen);
    } else {
        return false;
    }

    publish_snapshot();
    return true;
}

void MenuApp::handle_title_input(uint32_t keycode) {
    if (keycode == key_f5_) {
        refresh_song_source(true);
        publish_snapshot();
        return;
    }
    if (handle_settings_shortcut(keycode, Screen::Title)) {
        return;
    }
    if (keycode == key_up_) {
        title_cursor_ = clamp_int(title_cursor_ - 1, 0, 3);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        title_cursor_ = clamp_int(title_cursor_ + 1, 0, 3);
        publish_snapshot();
        return;
    }
    if (keycode == key_enter_) {
        if (title_cursor_ == 0) {
            screen_ = Screen::SongSelect;
            song_select_focus_ = SongSelectFocus::SongList;
            publish_snapshot();
            return;
        }
        if (title_cursor_ == 1) {
            screen_ = Screen::EditStub;
            publish_snapshot();
            return;
        }
        if (title_cursor_ == 2) {
            submenu_return_screen_ = Screen::Title;
            screen_ = Screen::OptionsHub;
            options_cursor_ = 0;
            publish_snapshot();
            return;
        }
        exit_requested_.store(true, std::memory_order_release);
        return;
    }
    if (keycode == key_escape_) {
        exit_requested_.store(true, std::memory_order_release);
    }
}

void MenuApp::handle_quick_setup_input(uint32_t keycode) {
    constexpr int item_count = 7;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

#ifdef _WIN32
    if ((keycode == key_enter_ && settings_cursor_ == 0) || keycode == key_f2_) {
        std::string new_path = browse_for_folder("Select Songs Folder");
        if (!new_path.empty()) {
            switch_song_source(new_path, false);
            publish_snapshot();
        }
        return;
    }
#endif

    if (keycode == key_left_ || keycode == key_right_) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        if (settings_cursor_ == 1) {
            if (config_.mode.gauge == "normal") {
                config_.mode.gauge = (direction > 0) ? "hard" : "easy";
            } else if (config_.mode.gauge == "hard") {
                config_.mode.gauge = (direction > 0) ? "easy" : "normal";
            } else {
                config_.mode.gauge = (direction > 0) ? "normal" : "hard";
            }
            persist_runtime_config();
            publish_snapshot();
            return;
        }
        if (settings_cursor_ == 2) {
            config_.speed.rate = clamp_step_value(config_.speed.rate + static_cast<double>(direction) * kRateStep,
                                                  kRateMin, kRateMax, kRateStep);
            persist_runtime_config();
            publish_snapshot();
            return;
        }
        if (settings_cursor_ == 3) {
            config_.visual_offset_ms = clamp_step_value(
                config_.visual_offset_ms + static_cast<double>(direction) * kVisualOffsetStep,
                kVisualOffsetMin,
                kVisualOffsetMax,
                kVisualOffsetStep);
            apply_runtime_graphics_config();
            persist_runtime_config();
            publish_snapshot();
            return;
        }
        if (settings_cursor_ == 4) {
            config_.audio_ui.bms_keysound_policy =
                cycle_bms_keysound_policy(config_.audio_ui.bms_keysound_policy, direction);
            persist_runtime_config();
            publish_snapshot();
            return;
        }
    }

    if (keycode == key_enter_) {
        if (settings_cursor_ == 5) {
            first_run_profile_ = false;
            screen_ = Screen::SongSelect;
            song_select_focus_ = SongSelectFocus::SongList;
            persist_runtime_config();
            publish_snapshot();
            return;
        }
        if (settings_cursor_ == 6) {
            first_run_profile_ = false;
            screen_ = Screen::Title;
            persist_runtime_config();
            publish_snapshot();
            return;
        }
    }

    if (keycode == key_escape_ || keycode == key_backspace_) {
        first_run_profile_ = false;
        screen_ = Screen::Title;
        persist_runtime_config();
        publish_snapshot();
    }
}

void MenuApp::handle_options_hub_input(uint32_t keycode) {
    constexpr int item_count = 7;
    const Screen return_screen = submenu_return_screen_;
    if (keycode == key_up_) {
        options_cursor_ = clamp_int(options_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        options_cursor_ = clamp_int(options_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_) {
        switch (options_cursor_) {
            case 0:
                submenu_return_screen_ = return_screen;
                screen_ = Screen::SettingsAudio;
                settings_cursor_ = 0;
                break;
            case 1:
                submenu_return_screen_ = return_screen;
                screen_ = Screen::SettingsGraphics;
                settings_cursor_ = 0;
                break;
            case 2:
                submenu_return_screen_ = return_screen;
                screen_ = Screen::SettingsSkins;
                settings_cursor_ = 0;
                skin_dirty_ = false;
                skin_edit_mode_ = normalize_skin_edit_mode(config_.mode.key_mode);
                skin_edit_lane_ = 0;
                refresh_available_osu_skins();
                refresh_available_lr2_skins();
                break;
            case 3:
                submenu_return_screen_ = return_screen;
                screen_ = Screen::SettingsInput;
                settings_cursor_ = 0;
                break;
            case 4:
                submenu_return_screen_ = return_screen;
                screen_ = Screen::ModeSelect;
                settings_cursor_ = 0;
                break;
            case 5:
                open_keymap_screen(return_screen);
                break;
            default:
                screen_ = return_screen;
                break;
        }
        publish_snapshot();
        return;
    }

    if (keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = return_screen;
        publish_snapshot();
    }
}

void MenuApp::handle_edit_stub_input(uint32_t keycode) {
    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::Title;
        publish_snapshot();
    }
}

void MenuApp::handle_song_select_input(uint32_t keycode) {
    sync_song_select_state();
    rebuild_current_song_record_indices();

    if (keycode == key_f5_) {
        refresh_song_source(true);
        publish_snapshot();
        return;
    }
#ifdef _WIN32
    if (keycode == key_f2_) {
        std::string new_path = browse_for_folder("Select Songs Folder");
        if (!new_path.empty()) {
            switch_song_source(new_path, false);
            song_select_view_ = SongSelectView::Songs;
            publish_snapshot();
        }
        return;
    }
#endif
    if (handle_settings_shortcut(keycode, Screen::SongSelect)) {
        return;
    }
    if (keycode == key_left_) {
        song_select_focus_ = SongSelectFocus::LeftNav;
        publish_snapshot();
        return;
    }
    if (keycode == key_right_) {
        song_select_focus_ = SongSelectFocus::SongList;
        publish_snapshot();
        return;
    }
    if (keycode == key_page_up_ || keycode == key_page_down_) {
        song_select_focus_ = SongSelectFocus::SongList;
        const int delta = (keycode == key_page_up_) ? -kSongSelectVisibleCardCount : kSongSelectVisibleCardCount;
        if (move_song_select_selection(delta)) {
            publish_snapshot();
        }
        return;
    }
    if (keycode == key_up_) {
        if (song_select_focus_ == SongSelectFocus::LeftNav) {
            song_select_nav_cursor_ = clamp_int(song_select_nav_cursor_ - 1, 0, 7);
            publish_snapshot();
            return;
        }
        if (move_song_select_selection(-1)) {
            publish_snapshot();
        }
        return;
    }
    if (keycode == key_down_) {
        if (song_select_focus_ == SongSelectFocus::LeftNav) {
            song_select_nav_cursor_ = clamp_int(song_select_nav_cursor_ + 1, 0, 7);
            publish_snapshot();
            return;
        }
        if (move_song_select_selection(1)) {
            publish_snapshot();
        }
        return;
    }
    if (keycode == key_r_ && song_select_view_ == SongSelectView::Records) {
        if (launch_selected_record_replay()) {
            return;
        }
    }
    if (keycode == key_enter_) {
        if (song_select_focus_ == SongSelectFocus::LeftNav) {
            switch (song_select_nav_cursor_) {
                case 0:
                    apply_song_sort(toggle_difficulty_sort(song_sort_mode_));
                    song_select_view_ = SongSelectView::Songs;
                    publish_snapshot();
                    return;
                case 1:
                    apply_song_sort(toggle_title_sort(song_sort_mode_));
                    song_select_view_ = SongSelectView::Songs;
                    publish_snapshot();
                    return;
                case 2:
                    song_select_view_ = SongSelectView::Sources;
                    selected_source_ = 0;
                    song_select_focus_ = SongSelectFocus::SongList;
                    publish_snapshot();
                    return;
                case 3:
                    song_key_filter_ = cycle_key_filter_value(song_key_filter_, 1);
                    song_select_view_ = SongSelectView::Songs;
                    rebuild_visible_song_list();
                    rebuild_current_song_record_indices();
                    song_select_focus_ = SongSelectFocus::SongList;
                    publish_snapshot();
                    return;
                case 4:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::SongBrowser;
                    settings_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 5:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::ModeSelect;
                    settings_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 6:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::OptionsHub;
                    options_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 7:
                    song_select_view_ = SongSelectView::Records;
                    selected_record_ = 0;
                    song_select_focus_ = SongSelectFocus::SongList;
                    rebuild_current_song_record_indices();
                    publish_snapshot();
                    return;
                default:
                    return;
            }
        }

        if (song_select_view_ == SongSelectView::Sources) {
            if (!config_.ui.recent_song_sources.empty() &&
                selected_source_ >= 0 &&
                selected_source_ < static_cast<int>(config_.ui.recent_song_sources.size())) {
                switch_song_source(config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)], false);
                publish_snapshot();
            }
        } else if (song_select_view_ == SongSelectView::Records) {
            if (open_selected_record_result()) {
                publish_snapshot();
            }
        } else if (visible_song_count() > 0) {
            launch_selected_song();
        }
        return;
    }
    if (keycode == key_backspace_) {
        if (song_select_view_ == SongSelectView::Records) {
            song_select_view_ = SongSelectView::Songs;
            song_select_focus_ = SongSelectFocus::SongList;
            publish_snapshot();
            return;
        }
        if (song_select_view_ != SongSelectView::Sources && !config_.ui.recent_song_sources.empty()) {
            song_select_view_ = SongSelectView::Sources;
            selected_source_ = 0;
            song_select_focus_ = SongSelectFocus::SongList;
            publish_snapshot();
            return;
        }
        screen_ = Screen::Title;
        publish_snapshot();
        return;
    }
    if (keycode == key_escape_) {
        screen_ = Screen::Title;
        publish_snapshot();
    }
}

void MenuApp::handle_song_browser_input(uint32_t keycode) {
    const int item_count = 6;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    auto apply_filter_refresh = [this]() {
        rebuild_visible_song_list();
        rebuild_current_song_record_indices();
        publish_snapshot();
    };

    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        song_key_filter_ = cycle_key_filter_value(song_key_filter_, direction);
        apply_filter_refresh();
        return;
    }
    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        song_level_min_filter_ = clamp_int(song_level_min_filter_ + direction, 0, 50);
        if (song_level_max_filter_ > 0 && song_level_min_filter_ > song_level_max_filter_) {
            song_level_max_filter_ = song_level_min_filter_;
        }
        apply_filter_refresh();
        return;
    }
    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        song_level_max_filter_ = clamp_int(song_level_max_filter_ + direction, 0, 50);
        if (song_level_max_filter_ > 0 && song_level_min_filter_ > song_level_max_filter_) {
            song_level_min_filter_ = song_level_max_filter_;
        }
        apply_filter_refresh();
        return;
    }

    if (settings_cursor_ == 0) {
        if (keycode == key_backspace_) {
            if (!song_search_query_.empty()) {
                song_search_query_.pop_back();
                apply_filter_refresh();
                return;
            }
        }
        if (key_delete_ != 0 && keycode == key_delete_) {
            if (!song_search_query_.empty()) {
                song_search_query_.clear();
                apply_filter_refresh();
                return;
            }
        }
        if (auto ch = search_character_from_keycode(keycode)) {
            song_search_query_.push_back(*ch);
            apply_filter_refresh();
            return;
        }
    }

    if (keycode == key_enter_) {
        if (settings_cursor_ == 4) {
            song_search_query_.clear();
            song_key_filter_ = 0;
            song_level_min_filter_ = 0;
            song_level_max_filter_ = 0;
            apply_filter_refresh();
            return;
        }
        if (settings_cursor_ == 5) {
            screen_ = submenu_return_screen_;
            settings_cursor_ = 0;
            publish_snapshot();
            return;
        }
    }

    if (keycode == key_escape_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        publish_snapshot();
        return;
    }

    if (keycode == key_backspace_ && settings_cursor_ != 0) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        publish_snapshot();
    }
}

bool MenuApp::move_song_select_selection(int delta) {
    if (delta == 0) {
        return false;
    }

    if (song_select_view_ == SongSelectView::Sources) {
        if (config_.ui.recent_song_sources.empty()) {
            return false;
        }
        const int previous = selected_source_;
        selected_source_ = clamp_int(selected_source_ + delta, 0,
                                     static_cast<int>(config_.ui.recent_song_sources.size() - 1));
        return selected_source_ != previous;
    }

    if (song_select_view_ == SongSelectView::Records) {
        rebuild_current_song_record_indices();
        if (current_song_record_indices_.empty()) {
            return false;
        }
        const int previous = selected_record_;
        selected_record_ = clamp_int(selected_record_ + delta, 0,
                                     static_cast<int>(current_song_record_indices_.size() - 1));
        return selected_record_ != previous;
    }

    if (visible_song_count() == 0) {
        return false;
    }

    const int previous = selected_song_;
    selected_song_ = clamp_int(selected_song_ + delta, 0, static_cast<int>(visible_song_count() - 1));
    return selected_song_ != previous;
}

void MenuApp::handle_keymap_input(uint32_t keycode) {
    const int row_count = static_cast<int>(keymap_lanes_.size()) + 1;
    if (keycode == key_up_) {
        keymap_cursor_ = clamp_int(keymap_cursor_ - 1, 0, row_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        keymap_cursor_ = clamp_int(keymap_cursor_ + 1, 0, row_count - 1);
        publish_snapshot();
        return;
    }
    if (keymap_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config::KeymapManager manager;
        const auto modes = manager.supported_mode_tokens();
        int current_index = 0;
        for (std::size_t i = 0; i < modes.size(); ++i) {
            if (modes[i] == manager.normalize_mode_token(keymap_edit_mode_)) {
                current_index = static_cast<int>(i);
                break;
            }
        }
        current_index += (keycode == key_left_) ? -1 : 1;
        if (current_index < 0) {
            current_index = static_cast<int>(modes.size() - 1);
        } else if (current_index >= static_cast<int>(modes.size())) {
            current_index = 0;
        }
        keymap_edit_mode_ = modes[static_cast<std::size_t>(current_index)];
        refresh_keymap_lane_list();
        publish_snapshot();
        return;
    }
    if (keycode == key_enter_) {
        start_keymap_capture();
        publish_snapshot();
        return;
    }
    if (keycode == key_a_) {
        apply_keymap_save();
        publish_snapshot();
        return;
    }
    if (keycode == key_r_) {
        apply_keymap_reset();
        publish_snapshot();
        return;
    }
    if (keycode == key_f2_) {
        screen_ = Screen::KeymapTest;
        publish_snapshot();
        return;
    }
    if (keycode == key_escape_ || keycode == key_backspace_) {
        exit_keymap_screen();
        return;
    }
}

void MenuApp::handle_keymap_confirm_input(uint32_t keycode) {
    if (keycode == key_enter_) {
        if (!keymap_duplicate_lane_.empty()) {
            working_keymap_.bindings[keymap_duplicate_lane_] = "";
        }
        if (!keymap_pending_lane_.empty()) {
            working_keymap_.bindings[keymap_pending_lane_] = keymap_pending_key_;
            keymap_dirty_ = true;
        }
        keymap_pending_lane_.clear();
        keymap_pending_key_.clear();
        keymap_duplicate_lane_.clear();
        screen_ = Screen::Keymap;
        publish_snapshot();
        return;
    }
    if (keycode == key_escape_ || keycode == key_backspace_) {
        keymap_pending_lane_.clear();
        keymap_pending_key_.clear();
        keymap_duplicate_lane_.clear();
        screen_ = Screen::Keymap;
        publish_snapshot();
    }
}

void MenuApp::handle_keymap_test_input(uint32_t keycode) {
    if (keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::Keymap;
        publish_snapshot();
    }
}

void MenuApp::handle_result_input(uint32_t keycode) {
    if (keycode == key_r_) {
        if (launch_last_result_replay()) {
            return;
        }
    }
    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::SongSelect;
        publish_snapshot();
    }
}

void MenuApp::populate_gameplay_render_data(render::GameplayHudData& target,
                                            uint64_t* out_motion_revision,
                                            uint64_t* out_text_revision) {
    std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);

    target.motion_revision = gameplay_hud_.motion_revision;
    target.text_revision = gameplay_hud_.text_revision;
    target.active = gameplay_hud_.active;
    target.loading = gameplay_hud_.loading;
    target.countdown_active = gameplay_hud_.countdown_active;
    target.countdown_value = gameplay_hud_.countdown_value;
    target.loading_percent = gameplay_hud_.loading_percent;
    target.loading_stage = gameplay_hud_.loading_stage;
    target.lane_count = clamp_int(gameplay_hud_.lane_count, 1, static_cast<int>(kGameplayHudMaxLanes));
    target.current_sample = gameplay_hud_.current_sample;
    target.duration_samples = gameplay_hud_.duration_samples;
    target.sample_rate = gameplay_hud_.sample_rate;
    target.audio_sample_time_ns = gameplay_hud_.audio_sample_time_ns;
    target.audio_buffer_frames = gameplay_hud_.audio_buffer_frames;
    target.lookahead_samples = gameplay_hud_.lookahead_samples;
    target.past_samples = gameplay_hud_.past_samples;
    target.judgement_line_position = std::clamp(
        config_.skin.judgement_line_position,
        config::kJudgementLinePositionMin,
        config::kJudgementLinePositionMax);
    target.combo_position = std::clamp(
        config_.skin.combo_position,
        config::kComboPositionMin,
        config::kComboPositionMax);
    const std::string skin_mode = std::to_string(target.lane_count) + "k";
    target.note_width_scale = std::clamp(
        config::resolved_skin_note_width_scale(config_.skin, skin_mode),
        config::kNoteWidthScaleMin,
        config::kNoteWidthScaleMax);
    target.note_height_scale = std::clamp(
        config::resolved_skin_note_height_scale(config_.skin, skin_mode),
        config::kNoteHeightScaleMin,
        config::kNoteHeightScaleMax);
    target.lane_divider_width_scale = std::clamp(
        config::resolved_skin_lane_divider_width_scale(config_.skin, skin_mode),
        config::kLaneDividerWidthScaleMin,
        config::kLaneDividerWidthScaleMax);
    target.hold_body_width_scale = std::clamp(
        config_.skin.hold_body_width_scale,
        config::kHoldBodyWidthScaleMin,
        config::kHoldBodyWidthScaleMax);
    target.show_lane_dividers = config_.skin.show_lane_dividers;
    target.show_judgement_line = config_.skin.show_judgement_line;
    target.show_gear_boundary_line = config_.skin.show_gear_boundary_line;
    target.hold_tail_taper_enabled = config_.skin.hold_tail_taper_enabled;
    target.note_border_enabled = config_.skin.note_border_enabled;
    target.note_shape = config::normalize_skin_note_shape_token(config_.skin.note_shape);
    target.preserve_note_image_aspect_ratio = config_.skin.preserve_note_image_aspect_ratio;
    target.skin_source = config::normalize_skin_source_token(config_.skin.source);
    target.external_skin_root = active_external_skin_root();
    target.external_skin_name = active_external_skin_name();
    target.visual_offset_ms = std::clamp(config_.visual_offset_ms, kVisualOffsetMin, kVisualOffsetMax);
    target.rate = gameplay_hud_.rate;
    target.hispeed = gameplay_hud_.hispeed;
    target.combo = gameplay_hud_.combo;
    target.max_combo = gameplay_hud_.max_combo;
    target.pg = gameplay_hud_.counts.pg;
    target.gr = gameplay_hud_.counts.gr;
    target.gd = gameplay_hud_.counts.gd;
    target.bd = gameplay_hud_.counts.bd;
    target.total_notes = gameplay_hud_.counts.pg + gameplay_hud_.counts.gr +
                         gameplay_hud_.counts.gd + gameplay_hud_.counts.bd;
    target.gauge = gameplay_hud_.gauge;
    target.gauge_label = gauge_type_label(gameplay_hud_.gauge_type);
    target.has_feedback = gameplay_hud_.has_feedback;
    target.feedback = judgement_label(gameplay_hud_.feedback);
    target.feedback_delta_ms = gameplay_hud_.feedback_delta_ms;
    target.finished = gameplay_hud_.finished;
    target.game_over = gameplay_hud_.game_over;

    target.lane_activity_count = gameplay_hud_.lane_activity_count;
    target.lane_activity.fill(0.0f);
    std::copy_n(gameplay_hud_.lane_activity.begin(), gameplay_hud_.lane_activity_count, target.lane_activity.begin());

    target.lane_color_count = 0;
    target.lane_colors.fill(0);
    const auto lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_mode);
    target.lane_color_count = std::min<std::size_t>(lane_colors.size(), static_cast<std::size_t>(target.lane_count));
    for (std::size_t i = 0; i < target.lane_color_count; ++i) {
        target.lane_colors[i] = config::skin_color_rgb(lane_colors[i]);
    }

    target.note_count = gameplay_hud_.note_count;
    for (std::size_t i = 0; i < gameplay_hud_.note_count; ++i) {
        const auto& note = gameplay_hud_.notes[i];
        render::GameplayNoteData out_note;
        out_note.lane = note.lane;
        out_note.start_sample = note.start_sample;
        out_note.tail_sample = note.tail_sample;
        out_note.hold = note.hold;
        out_note.head_visible = note.head_visible;
        target.notes[i] = out_note;
    }
    target.score = gameplay_hud_.score;

    const int judged_total = gameplay_hud_.counts.pg + gameplay_hud_.counts.gr + gameplay_hud_.counts.gd +
                             gameplay_hud_.counts.bd;
    if (judged_total > 0) {
        const double weighted = gameplay_hud_.counts.pg * 1.0 +
                                gameplay_hud_.counts.gr * 0.80 +
                                gameplay_hud_.counts.gd * 0.50 +
                                gameplay_hud_.counts.bd * 0.20;
        target.accuracy = std::clamp(weighted / static_cast<double>(judged_total) * 100.0, 0.0, 100.0);
    } else {
        target.accuracy = 0.0;
    }

    if (out_motion_revision) {
        *out_motion_revision = gameplay_hud_.motion_revision;
    }
    if (out_text_revision) {
        *out_text_revision = gameplay_hud_.text_revision;
    }
}

void MenuApp::update_gameplay_loading_state(int percent, std::string_view stage) {
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        const GameplayHudRevisionInput previous = gameplay_hud_revision_input(gameplay_hud_);
        GameplayHudRevisionInput next = previous;
        next.loading = true;
        next.loading_percent = clamp_int(percent, 0, 100);
        next.loading_stage = std::string(stage);
        const GameplayHudRevisionFlags diff = diff_gameplay_hud_revisions(previous, next);

        gameplay_hud_.loading = next.loading;
        gameplay_hud_.loading_percent = next.loading_percent;
        gameplay_hud_.loading_stage = next.loading_stage;
        advance_gameplay_hud_revisions(gameplay_hud_, diff.motion_changed, false);
    }
    publish_snapshot();
}

std::string MenuApp::current_track_label() const {
    if (screen_ == Screen::SongSelect && song_select_view_ == SongSelectView::Sources &&
        !config_.ui.recent_song_sources.empty() &&
        selected_source_ >= 0 &&
        selected_source_ < static_cast<int>(config_.ui.recent_song_sources.size())) {
        return menu_songs::song_source_display_name(
            config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)]);
    }
    if (selected_song_ >= 0 && selected_song_ < static_cast<int>(visible_song_count())) {
        if (const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_))) {
            return song_title_for_ui(*entry);
        }
    }
    if (!last_chart_title_.empty()) {
        return safe_ui_text(last_chart_title_, "-");
    }
    return "-";
}

void MenuApp::sync_menu_music() {
    if (screen_ == Screen::Gameplay) {
        menu_music_.stop();
        return;
    }

    const bool song_select_context =
        screen_ == Screen::SongSelect ||
        screen_ == Screen::SongBrowser ||
        screen_ == Screen::Result ||
        ((screen_ == Screen::OptionsHub ||
          screen_ == Screen::SettingsAudio ||
          screen_ == Screen::SettingsGraphics ||
          screen_ == Screen::SettingsSkins ||
          screen_ == Screen::SettingsInput ||
          screen_ == Screen::ModeSelect ||
          screen_ == Screen::ModeMods ||
          screen_ == Screen::Keymap ||
          screen_ == Screen::KeymapConfirm ||
          screen_ == Screen::KeymapTest) &&
         submenu_return_screen_ == Screen::SongSelect);

    std::string music_path = resolve_menu_music_file_path(song_select_context ? "Song Selecte.mp3" : "Main Menu.mp3");
    if (music_path.empty() && song_select_context) {
        music_path = resolve_menu_music_file_path("Song Select.mp3");
    }
    if (music_path.empty()) {
        menu_music_.stop();
        return;
    }

    const double gain = std::clamp(config_.audio_ui.master_volume * config_.audio_ui.bgm_volume, 0.0, 1.0);
    menu_music_.play_looping_file(music_path, gain);
}

void MenuApp::populate_quick_setup_render_data(render::MenuRenderData& render) {
    render.kind = render::MenuScreenKind::GenericList;
    render.generic.heading = "Quick Setup";

    append_menu_row(render.generic,
                    "Songs Folder",
                    safe_ui_text(menu_songs::song_source_display_name(songs_path_), "Choose Folder"),
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Gauge",
                    gauge_label(config_.mode.gauge),
                    settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Rate",
                    format_multiplier(config_.speed.rate),
                    settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow,
                    2,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Display Offset",
                    format_signed_offset_ms(config_.visual_offset_ms),
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    false,
                    true);
    append_menu_row(render.generic,
                    "BMS Keysound",
                    keysound_policy_label(config_.audio_ui.bms_keysound_policy),
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Continue to Song Select",
                    "",
                    settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow,
                    5,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Skip to Title",
                    "",
                    settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow,
                    6,
                    true,
                    false);

    render.generic.notes.push_back("First launch detected. TenRiff already created a default profile and default keymap.");
    render.generic.notes.push_back("Recommended start: Gauge Normal, Rate 1.00x, Display Offset 0ms, BMS Keysound Follow.");
    render.generic.notes.push_back("Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.");
    render.generic.notes.push_back("These values stay editable later from Song Select: A=Audio  G=Graphics  I=Input  M=Mode  K=Keymap.");
}

void MenuApp::populate_title_render_data(render::MenuRenderData& render,
                                         const std::string& current_track,
                                         const MenuApp::BestResultRecord& current_best) {
    render.kind = render::MenuScreenKind::TitleMenu;
    render.title.profile = options_.profile;
    render.title.track = current_track;
    render.title.high_score = current_best.has_value ? current_best.best_score : 0;
    render.title.buttons = {
        render::MenuButtonData{"PLAY", u8"▶", title_cursor_ == 0},
        render::MenuButtonData{"EDIT", u8"✎", title_cursor_ == 1},
        render::MenuButtonData{"OPTIONS", u8"⚙", title_cursor_ == 2},
        render::MenuButtonData{"EXIT", u8"⏻", title_cursor_ == 3},
    };
    render.title.guides = {
        "UP / DOWN or mouse to move",
        "ENTER or double-click to open",
        "F5 refreshes the current song source",
        "A/G/I/M/K jumps straight to Audio, Graphics, Input, Mode, Keymap",
        "F1 opens the control help overlay",
        "ESC exits from the title menu",
    };
}

void MenuApp::populate_song_select_render_data(render::MenuRenderData& render,
                                               const std::string& current_track,
                                               const MenuApp::BestResultRecord& current_best,
                                               const MenuApp::LocalPlayRecord* selected_record) {
    render.kind = render::MenuScreenKind::SongSelect;
    render.song_select.profile = options_.profile;
    render.song_select.track = current_track;
    render.song_select.song_count = static_cast<int>(visible_song_count());
    render.song_select.source_count = static_cast<int>(config_.ui.recent_song_sources.size());
    render.song_select.record_count = static_cast<int>(current_song_record_indices_.size());
    render.song_select.showing_sources = (song_select_view_ == SongSelectView::Sources);
    render.song_select.showing_records = (song_select_view_ == SongSelectView::Records);
    render.song_select.high_score =
        render.song_select.showing_sources ? 0 :
        (render.song_select.showing_records && selected_record ? selected_record->score :
         (current_best.has_value ? current_best.best_score : 0));
    render.song_select.current_source_name =
        safe_ui_text(menu_songs::song_source_display_name(songs_path_), "Songs");
    render.song_select.current_source_path = safe_ui_text_or_placeholder(songs_path_, "<invalid path>");
    render.song_select.index_profile_label = song_index_profile_label(config_.mode.song_index_profile);
    render.song_select.browser_summary = browser_summary_label(song_search_query_,
                                                               song_key_filter_,
                                                               song_level_min_filter_,
                                                               song_level_max_filter_);
    render.song_select.sort_summary = song_sort_detail_label(song_sort_mode_);
    render.song_select.primary_hint =
        render.song_select.showing_sources
            ? "UP/DOWN or wheel  MOVE     ENTER / dbl-click  OPEN SOURCE     PGUP/PGDN  PAGE"
            : (render.song_select.showing_records
                   ? "UP/DOWN or wheel  MOVE     ENTER / dbl-click  OPEN RESULT     PGUP/PGDN  PAGE"
                   : "UP/DOWN or wheel  MOVE     ENTER / dbl-click  PLAY     PGUP/PGDN  PAGE");
    render.song_select.secondary_hint =
        render.song_select.showing_sources
            ? "LEFT/RIGHT  NAV FOCUS     BACKSPACE  SONGS     F2  BROWSE     F5  REINDEX     F1  HELP"
            : (render.song_select.showing_records
                   ? "LEFT/RIGHT  NAV FOCUS     BACKSPACE  SONGS     ESC  TITLE     F1  HELP"
                   : "LEFT/RIGHT  NAV FOCUS     BACKSPACE  SOURCES     A/G/I/M/K  SETTINGS     F5  REINDEX     F1  HELP");

    const std::string source_detail =
        std::to_string(render.song_select.source_count) + " ROOT" +
        (render.song_select.source_count == 1 ? "" : "S");
    const std::string browser_detail = render.song_select.browser_summary;
    const std::string records_detail =
        (render.song_select.record_count > 0)
            ? (std::to_string(render.song_select.record_count) + " PLAYS")
            : std::string("NO PLAYS");

    render.song_select.indexing = song_indexer_.is_running();
    if (render.song_select.indexing) {
        const auto progress = song_indexer_.progress();
        render.song_select.indexing_stage = song_index_stage_label(progress.stage);
        render.song_select.indexing_processed = std::max(0, progress.processed);
        render.song_select.indexing_total = progress.total;
        if (progress.total > 0) {
            const int processed = std::max(0, std::min(progress.processed, progress.total));
            const int percent = static_cast<int>(std::llround(
                100.0 * static_cast<double>(processed) / static_cast<double>(progress.total)));
            render.song_select.indexing_percent = percent;

            const int remaining = progress.total - processed;
            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t elapsed_ns = (progress.started_ns > 0 && now_ns > progress.started_ns)
                                           ? (now_ns - progress.started_ns)
                                           : 0;
            if (remaining > 0 && processed > 0 && elapsed_ns > 0) {
                const double elapsed_sec = static_cast<double>(elapsed_ns) / 1'000'000'000.0;
                const double eta_sec =
                    elapsed_sec * static_cast<double>(remaining) / static_cast<double>(processed);
                render.song_select.indexing_eta =
                    format_eta_seconds(static_cast<int64_t>(std::llround(eta_sec)));
            }
        } else if (progress.total == 0) {
            render.song_select.indexing_percent = 0;
            render.song_select.indexing_eta = "0s";
        } else {
            render.song_select.indexing_percent = -1;
            render.song_select.indexing_eta.clear();
        }
    }

    render.song_select.left_nav = {
        render::MenuButtonData{"LEVEL", "L", song_select_nav_cursor_ == 0,
                               (song_sort_mode_ == SongSortMode::DifficultyAsc ||
                                song_sort_mode_ == SongSortMode::DifficultyDesc)
                                   ? song_sort_detail_label(song_sort_mode_)
                                   : "LV ASC"},
        render::MenuButtonData{"TITLE", "T", song_select_nav_cursor_ == 1,
                               (song_sort_mode_ == SongSortMode::TitleAsc ||
                                song_sort_mode_ == SongSortMode::TitleDesc)
                                   ? song_sort_detail_label(song_sort_mode_)
                                   : "A-Z"},
        render::MenuButtonData{"SOURCES", "D", song_select_nav_cursor_ == 2,
                               render.song_select.showing_sources ? "ACTIVE" : source_detail},
        render::MenuButtonData{"KEY", "K", song_select_nav_cursor_ == 3, key_filter_label(song_key_filter_)},
        render::MenuButtonData{"BROWSE", "F", song_select_nav_cursor_ == 4, browser_detail},
        render::MenuButtonData{"MOD", "M", song_select_nav_cursor_ == 5,
                               format_multiplier(config_.speed.rate) + " / HS " +
                                   format_decimal(config_.speed.hi_speed)},
        render::MenuButtonData{"OPTIONS", "O", song_select_nav_cursor_ == 6, "AUDIO / GFX"},
        render::MenuButtonData{"RECORDS", "R", song_select_nav_cursor_ == 7,
                               render.song_select.showing_records ? "ACTIVE" : records_detail},
    };

    if (render.song_select.showing_sources) {
        const int total_sources = static_cast<int>(config_.ui.recent_song_sources.size());
        if (total_sources > 0) {
            selected_source_ = clamp_int(selected_source_, 0, total_sources - 1);
            constexpr int visible = kSongSelectVisibleCardCount;
            int start = std::max(0, selected_source_ - (visible / 2));
            const int max_start = std::max(0, total_sources - visible);
            start = std::min(start, max_start);
            const int end = std::min(total_sources, start + visible);
            render.song_select.list_total_count = total_sources;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_source_;

            for (int i = start; i < end; ++i) {
                const std::string& source_path = config_.ui.recent_song_sources[static_cast<std::size_t>(i)];
                render::SongCardData card;
                card.title = safe_ui_text(menu_songs::song_source_display_name(source_path), "<invalid title>");
                card.artist = safe_ui_text_or_placeholder(source_path, "<invalid artist>");
                const auto count_it =
                    source_song_counts_.find(menu_songs::normalize_path_key(path_from_utf8(source_path)));
                card.level = (count_it != source_song_counts_.end()) ? count_it->second : 0;
                card.song_index = i;
                card.selected = (i == selected_source_);
                card.detail = safe_ui_text((menu_songs::normalize_path_key(path_from_utf8(source_path)) ==
                                            menu_songs::normalize_path_key(path_from_utf8(songs_path_)))
                                               ? "CURRENT SOURCE"
                                               : "RECENT SOURCE");
                render.song_select.songs.push_back(std::move(card));
            }

            const std::string& selected_source =
                config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)];
            render.song_select.selected_source_name =
                safe_ui_text(menu_songs::song_source_display_name(selected_source), "Songs");
            render.song_select.selected_source_path =
                safe_ui_text_or_placeholder(selected_source, "<invalid path>");
            const auto count_it =
                source_song_counts_.find(menu_songs::normalize_path_key(path_from_utf8(selected_source)));
            render.song_select.selected_source_song_count =
                (count_it != source_song_counts_.end()) ? count_it->second : -1;
            render.song_select.selected_source_active =
                menu_songs::normalize_path_key(path_from_utf8(selected_source)) ==
                menu_songs::normalize_path_key(path_from_utf8(songs_path_));
        }
        if (total_sources == 0) {
            render.song_select.empty_title = "NO SONG SOURCES";
            render.song_select.empty_message =
                "Press F2 to choose a folder, or drag and drop one onto the window.";
        }
    } else if (render.song_select.showing_records) {
        const int total = static_cast<int>(current_song_record_indices_.size());
        if (total > 0) {
            constexpr int visible = kSongSelectVisibleCardCount;
            selected_record_ = clamp_int(selected_record_, 0, total - 1);
            int start = std::max(0, selected_record_ - (visible / 2));
            const int max_start = std::max(0, total - visible);
            start = std::min(start, max_start);
            const int end = std::min(total, start + visible);
            render.song_select.list_total_count = total;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_record_;

            for (int i = start; i < end; ++i) {
                const LocalPlayRecord& record =
                    local_play_records_[current_song_record_indices_[static_cast<std::size_t>(i)]];
                render::SongCardData card;
                card.title = menu_records::compact_timestamp_label(record.created_utc);
                card.artist = record.clear_status + "  " + record.rank + "  SCORE " +
                              format_int_with_commas(record.score);
                card.detail = record.replay_path.empty()
                                  ? "RESULT ONLY"
                                  : "REPLAY " + filename_only(record.replay_path);
                card.song_index = i;
                card.selected = (i == selected_record_);
                render.song_select.songs.push_back(std::move(card));
            }
        }
        if (total == 0) {
            render.song_select.empty_title = "NO LOCAL RECORDS";
            render.song_select.empty_message =
                "Play a chart first. Saved results and replays will appear here.";
        }
    } else {
        const int total = static_cast<int>(visible_song_count());
        if (total > 0) {
            constexpr int visible = kSongSelectVisibleCardCount;
            int start = std::max(0, selected_song_ - (visible / 2));
            const int max_start = std::max(0, total - visible);
            start = std::min(start, max_start);
            const int end = std::min(total, start + visible);
            render.song_select.list_total_count = total;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_song_;

            for (int i = start; i < end; ++i) {
                const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(i));
                if (!entry) {
                    continue;
                }
                render::SongCardData card;
                card.title = song_title_for_ui(*entry);
                card.artist = song_artist_for_ui(*entry);
                card.detail = safe_ui_text_or_placeholder(song_detail_label(*entry), "<invalid detail>");
                card.background_path = song_background_preview_path_for_entry(*entry);
                card.level = entry->level;
                card.rating = entry->rating;
                card.song_index = i;
                card.selected = (i == selected_song_);
                render.song_select.songs.push_back(std::move(card));
            }
        }
        if (total == 0) {
            if (!song_search_query_.empty() || song_key_filter_ > 0 || song_level_min_filter_ > 0 ||
                song_level_max_filter_ > 0) {
                render.song_select.empty_title = "NO CHARTS MATCH";
                render.song_select.empty_message =
                    "Clear filters in Browse or switch the active source to see more charts.";
            } else if (render.song_select.indexing) {
                render.song_select.empty_title = "BUILDING LIBRARY";
                render.song_select.empty_message =
                    "The current source is still indexing. You can keep browsing while scan progress updates above.";
            } else {
                render.song_select.empty_title = "NO CHARTS INDEXED";
                render.song_select.empty_message =
                    "Press F5 to scan the current source, or use F2 / drag-and-drop to point TenRiff at a songs folder.";
            }
        }
    }

    if (!render.song_select.showing_sources) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            render.song_select.selected_song_title = song_title_for_ui(*entry);
            render.song_select.selected_song_artist = song_artist_for_ui(*entry);
            render.song_select.selected_song_detail = safe_ui_text_or_placeholder(song_detail_label(*entry), "-");
            render.song_select.selected_song_background_path = selected_song_background_preview_path();
        }
    }

    if (render.song_select.showing_records) {
        if (selected_record) {
            render.song_select.rank = selected_record->rank;
            render.song_select.best_score = selected_record->score;
            render.song_select.max_combo = selected_record->max_combo;
            render.song_select.perfect = selected_record->perfect;
            render.song_select.great = selected_record->great;
            render.song_select.good = selected_record->good;
            render.song_select.bad = selected_record->bad;
            render.song_select.miss = selected_record->miss;
            render.song_select.accuracy = selected_record->accuracy;
            render.song_select.selected_record_created_utc =
                menu_records::compact_timestamp_label(selected_record->created_utc);
            render.song_select.selected_record_status = selected_record->clear_status;
            render.song_select.selected_record_replay_file = filename_only(selected_record->replay_path);
            if (const ReplaySummary* replay = replay_summary_for_path(selected_record->replay_path)) {
                render.song_select.selected_record_replay_lane_count = replay->lane_count;
                render.song_select.selected_record_replay_event_count = replay->event_count;
                if (!replay->exists) {
                    render.song_select.selected_record_replay_detail = "Replay missing";
                } else if (!replay->error.empty()) {
                    render.song_select.selected_record_replay_detail = "Replay parse warning";
                } else {
                    render.song_select.selected_record_replay_detail =
                        format_multiplier(replay->rate) + " / " +
                        format_signed_offset_ms(replay->input_offset_ms);
                }
            } else {
                render.song_select.selected_record_replay_detail = "No replay";
            }
        }
    } else if (!render.song_select.showing_sources) {
        render.song_select.rank = current_best.has_value ? current_best.rank : "--";
        render.song_select.best_score = current_best.has_value ? current_best.best_score : 0;
        render.song_select.max_combo = current_best.has_value ? current_best.max_combo : 0;
        render.song_select.perfect = current_best.has_value ? current_best.perfect : 0;
        render.song_select.great = current_best.has_value ? current_best.great : 0;
        render.song_select.good = current_best.has_value ? current_best.good : 0;
        render.song_select.bad = current_best.has_value ? current_best.bad : 0;
        render.song_select.miss = current_best.has_value ? current_best.miss : 0;
    }
}

void MenuApp::populate_song_browser_render_data(render::MenuRenderData& render) {
    render.kind = render::MenuScreenKind::GenericList;

    append_menu_row(render.generic,
                    "Search",
                    song_search_query_.empty() ? std::string("<type to search>") : song_search_query_,
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Key Filter",
                    key_filter_label(song_key_filter_),
                    settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Difficulty Min",
                    song_level_min_filter_ > 0 ? std::to_string(song_level_min_filter_) : "Any",
                    settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow,
                    2,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Difficulty Max",
                    song_level_max_filter_ > 0 ? std::to_string(song_level_max_filter_) : "Any",
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Clear Filters",
                    "",
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Back",
                    "",
                    settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow,
                    5,
                    true,
                    false);

    render.generic.notes.push_back("Search matches title, artist, and chart path.");
    render.generic.notes.push_back("Use letters/numbers/space on the Search row. Backspace deletes, Delete clears.");
    render.generic.notes.push_back("Key Filter supports All plus 4K through 10K and 16K. Song Select also has a quick KEY toggle.");
    render.generic.notes.push_back("Difficulty filters apply to indexed LV values only.");
    render.generic.notes.push_back("Sort order stays available from Song Select: LEVEL toggles ASC/DESC, TITLE toggles A-Z/Z-A.");
}

void MenuApp::populate_result_render_data(render::MenuRenderData& render, const std::string& current_track) {
    render.kind = render::MenuScreenKind::ResultScreen;
    render.result.profile = options_.profile;
    render.result.track = last_chart_title_.empty() ? current_track : last_chart_title_;
    render.result.title = last_chart_title_.empty() ? "Unknown Chart" : last_chart_title_;
    render.result.artist = last_chart_artist_;

    if (!has_result_) {
        render.result.notes.push_back("No result data is available for this run.");
        render.result.notes.push_back("Press Enter or Esc to return to Song Select.");
        return;
    }

    const int judged = menu_records::judged_total(last_result_.counts);
    const int total_notes = (last_result_.total_notes > 0) ? last_result_.total_notes : judged;
    const double accuracy = menu_records::calculate_accuracy(last_result_);

    game::GaugeType final_gauge_type = gauge_type_from_mode_string(config_.mode.gauge);
    if (!last_result_.shifts.empty()) {
        final_gauge_type = last_result_.shifts.back().to;
    }

    render.result.rank = menu_records::calculate_rank(last_result_, last_game_over_);
    render.result.status = last_game_over_ ? "GAME OVER" : "CLEAR";
    render.result.gauge_label = gauge_type_label(final_gauge_type);
    render.result.score = last_result_final_score_;
    render.result.accuracy = accuracy;
    render.result.gauge_value =
        last_result_.gauge_history.empty() ? 0.0 : last_result_.gauge_history.back().value;
    render.result.max_combo = last_result_.max_combo;
    render.result.total_notes = total_notes;
    render.result.judged_notes = judged;
    render.result.perfect = last_result_.counts.pg;
    render.result.great = last_result_.counts.gr;
    render.result.good = last_result_.counts.gd;
    render.result.bad = last_result_.counts.bd;
    render.result.miss = 0;
    render.result.mean_delta_ms = last_result_.mean_delta_ms;
    render.result.stddev_delta_ms = last_result_.stddev_delta_ms();
    render.result.shift_count = static_cast<int>(last_result_.shifts.size());
    render.result.export_warning_count = static_cast<int>(last_export_warnings_.size());
    render.result.replay_file = filename_only(last_replay_path_);
    render.result.replay_available = false;
    render.result.result_file = filename_only(last_result_path_);

    if (!last_replay_path_.empty()) {
        std::error_code ec;
        render.result.replay_available = std::filesystem::exists(path_from_utf8(last_replay_path_), ec) && !ec;
    }

    if (!render.result.replay_file.empty()) {
        render.result.notes.push_back("Replay: " + render.result.replay_file);
    }
    if (!render.result.result_file.empty()) {
        render.result.notes.push_back("Result: " + render.result.result_file);
    }
    if (render.result.replay_available) {
        render.result.notes.push_back("R or click Replay to watch the saved input trace.");
    } else if (!render.result.replay_file.empty()) {
        render.result.notes.push_back("Replay file is missing or unavailable.");
    }
    render.result.notes.push_back("Score x" + format_decimal(last_result_score_multiplier_));
    if (last_result_rate_multiplier_ != 1.0) {
        render.result.notes.push_back("Rate score x" + format_decimal(last_result_rate_multiplier_));
    }
    render.result.notes.push_back("Mods: " + mode_mod_summary(last_result_mods_));
    render.result.notes.push_back("Timing center " + format_signed_ms(last_result_.mean_delta_ms) +
                                  "  spread " + format_decimal(render.result.stddev_delta_ms) + "ms");
    if (last_session_replay_playback_) {
        render.result.notes.push_back("Replay playback session: no new replay/result export was written.");
    }
    if (!last_export_warnings_.empty()) {
        render.result.notes.push_back("Export warnings: " + std::to_string(last_export_warnings_.size()));
        const std::size_t preview_count = std::min<std::size_t>(2, last_export_warnings_.size());
        for (std::size_t i = 0; i < preview_count; ++i) {
            render.result.notes.push_back(last_export_warnings_[i]);
        }
    }

    int64_t graph_end_sample = 1;
    for (const auto& sample : last_result_.gauge_history) {
        graph_end_sample = std::max(graph_end_sample, sample.sample);
    }
    for (const auto& shift : last_result_.shifts) {
        graph_end_sample = std::max(graph_end_sample, shift.sample);
    }

    const std::size_t gauge_count = last_result_.gauge_history.size();
    std::size_t gauge_stride = 1;
    if (gauge_count > 240) {
        gauge_stride = (gauge_count + 239) / 240;
    }

    for (std::size_t i = 0; i < gauge_count; i += gauge_stride) {
        const auto& sample = last_result_.gauge_history[i];
        const float position = (gauge_count == 1)
                                   ? 1.0f
                                   : static_cast<float>(
                                         std::clamp(static_cast<double>(sample.sample) /
                                                        static_cast<double>(graph_end_sample),
                                                    0.0, 1.0));
        const float value = static_cast<float>(std::clamp(sample.value / 100.0, 0.0, 1.0));
        render.result.gauge_points.push_back(render::ResultGaugePoint{position, value});
    }
    if (gauge_count > 1) {
        const auto& last_sample = last_result_.gauge_history.back();
        const float last_position = static_cast<float>(
            std::clamp(static_cast<double>(last_sample.sample) / static_cast<double>(graph_end_sample),
                       0.0, 1.0));
        const float last_value = static_cast<float>(std::clamp(last_sample.value / 100.0, 0.0, 1.0));
        if (render.result.gauge_points.empty() ||
            render.result.gauge_points.back().position != last_position) {
            render.result.gauge_points.push_back(render::ResultGaugePoint{last_position, last_value});
        }
    }

            for (const auto& shift : last_result_.shifts) {
                const double shift_ratio = static_cast<double>(shift.sample) / static_cast<double>(graph_end_sample);
                const double clamped_shift_ratio =
                    shift_ratio < 0.0 ? 0.0 : (shift_ratio > 1.0 ? 1.0 : shift_ratio);
                const float position = static_cast<float>(clamped_shift_ratio);
                render.result.gauge_shifts.push_back(
                    render::ResultShiftMarker{position,
                                              short_gauge_type_label(shift.from) + "->" +
                                          short_gauge_type_label(shift.to)});
    }
    std::stable_sort(render.result.gauge_points.begin(),
                     render.result.gauge_points.end(),
                     [](const render::ResultGaugePoint& lhs, const render::ResultGaugePoint& rhs) {
                         return lhs.position < rhs.position;
                     });
    std::stable_sort(render.result.gauge_shifts.begin(),
                     render.result.gauge_shifts.end(),
                     [](const render::ResultShiftMarker& lhs, const render::ResultShiftMarker& rhs) {
                         return lhs.position < rhs.position;
                     });
}

void MenuApp::populate_generic_screen_render_data(render::MenuRenderData& render) {
    render.kind = render::MenuScreenKind::GenericList;
    render.generic.heading = screen_title();

    if (screen_ == Screen::OptionsHub) {
        append_menu_row(render.generic, "Audio", "", options_cursor_ == 0, render::MenuHitTargetKind::OptionsItem, 0, true, false);
        append_menu_row(render.generic, "Graphics", "", options_cursor_ == 1, render::MenuHitTargetKind::OptionsItem, 1, true, false);
        append_menu_row(render.generic, "Skins", "", options_cursor_ == 2, render::MenuHitTargetKind::OptionsItem, 2, true, false);
        append_menu_row(render.generic, "Input", "", options_cursor_ == 3, render::MenuHitTargetKind::OptionsItem, 3, true, false);
        append_menu_row(render.generic, "Mode", "", options_cursor_ == 4, render::MenuHitTargetKind::OptionsItem, 4, true, false);
        append_menu_row(render.generic, "Keymap", "", options_cursor_ == 5, render::MenuHitTargetKind::OptionsItem, 5, true, false);
        append_menu_row(render.generic, "Back", "", options_cursor_ == 6, render::MenuHitTargetKind::OptionsItem, 6, true, false);
        render.generic.notes.push_back("Up/Down to move, Enter to select, Esc to return.");
        render.generic.notes.push_back("A/G/I/M/K also jumps directly into Audio, Graphics, Input, Mode, and Keymap.");
    } else if (screen_ == Screen::EditStub) {
        render.generic.notes.push_back("Editor is not implemented yet.");
        append_menu_row(render.generic, "Back", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
    } else if (screen_ == Screen::SettingsAudio) {
        populate_audio_settings_render_data(render);
    } else if (screen_ == Screen::SettingsGraphics) {
        populate_graphics_settings_render_data(render);
    } else if (screen_ == Screen::SettingsSkins) {
        populate_skin_settings_render_data(render);
    } else if (screen_ == Screen::SettingsInput) {
        populate_input_settings_render_data(render);
    } else if (screen_ == Screen::ModeSelect) {
        populate_mode_settings_render_data(render);
    } else if (screen_ == Screen::ModeMods) {
        populate_mode_mods_render_data(render);
    } else if (screen_ == Screen::Keymap) {
        config::KeymapManager keymap_manager;
        const auto current_bindings = keymap_manager.bindings_for_mode(working_keymap_, keymap_edit_mode_);
        append_menu_row(render.generic, "Key Mode", key_mode_label(keymap_edit_mode_), keymap_cursor_ == 0,
                        render::MenuHitTargetKind::None, 0, false, false);
        for (std::size_t i = 0; i < keymap_lanes_.size(); ++i) {
            const std::string& lane = keymap_lanes_[i];
            const auto it = current_bindings.find(lane);
            std::string key_name = (it == current_bindings.end() || it->second.empty())
                                       ? "Unassigned"
                                       : it->second;
            if (keymap_capture_active_ && static_cast<int>(i) + 1 == keymap_cursor_) {
                key_name += " [waiting]";
            }
            append_menu_row(render.generic, lane, key_name, static_cast<int>(i) + 1 == keymap_cursor_,
                            render::MenuHitTargetKind::None, static_cast<int>(i) + 1, false, false);
        }
        if (keymap_capture_active_) {
            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t remaining_ns = std::max<int64_t>(0, keymap_capture_deadline_ns_ - now_ns);
            const int remaining_ms = static_cast<int>(remaining_ns / 1'000'000);
            render.generic.notes.push_back("Capture timeout: " + std::to_string(remaining_ms) + "ms");
            render.generic.notes.push_back("Press any keyboard key. Delete cancels capture.");
            render.generic.notes.push_back("Duplicate lane bindings are allowed.");
        }
        append_menu_row(render.generic, "Save", "", false, render::MenuHitTargetKind::KeymapButton, 0, !keymap_capture_active_, false);
        append_menu_row(render.generic, "Reset", "", false, render::MenuHitTargetKind::KeymapButton, 1, !keymap_capture_active_, false);
        append_menu_row(render.generic, "NKRO Test", "", false, render::MenuHitTargetKind::KeymapButton, 2, !keymap_capture_active_, false);
        append_menu_row(render.generic, "Back", "", false, render::MenuHitTargetKind::KeymapButton, 3, !keymap_capture_active_, false);
        render.generic.notes.push_back("Left/Right on Key Mode selects which 4K-10K or 16K layout you are editing.");
        render.generic.notes.push_back("Enter binds the selected lane. A=Save  R=Reset  F2=NKRO Test  Esc=Back");
    } else if (screen_ == Screen::KeymapConfirm) {
        render.generic.notes.push_back("Duplicate binding detected.");
        render.generic.notes.push_back("Key: " + keymap_pending_key_);
        render.generic.notes.push_back("Already used by: " + keymap_duplicate_lane_);
        append_menu_row(render.generic, "Replace Existing Binding", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
        append_menu_row(render.generic, "Cancel", "", false, render::MenuHitTargetKind::SettingsRow, 1, true, false);
    } else if (screen_ == Screen::KeymapTest) {
        render.generic.notes.push_back("NKRO Test (press multiple keys)");
        config::KeymapManager keymap_manager;
        const auto current_bindings = keymap_manager.bindings_for_mode(working_keymap_, keymap_edit_mode_);
        for (std::size_t i = 0; i < keymap_lanes_.size(); ++i) {
            const std::string& lane = keymap_lanes_[i];
            const auto it = current_bindings.find(lane);
            const std::string key_name = (it == current_bindings.end() || it->second.empty())
                                             ? "Unassigned"
                                             : it->second;
            bool down = false;
            if (!key_name.empty()) {
                auto keycode = config::KeycodeMap::to_keycode(key_name);
                if (keycode.has_value()) {
                    down = (pressed_keys_.find(keycode.value()) != pressed_keys_.end());
                }
            }
            append_menu_row(render.generic, lane, key_name + (down ? " [DOWN]" : ""), down,
                            render::MenuHitTargetKind::None, static_cast<int>(i), false, false);
        }
        append_menu_row(render.generic, "Back", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
    }
}

void MenuApp::publish_snapshot() {
    if (screen_ == Screen::SongSelect) {
        sync_song_select_state();
    }
    sync_menu_music();

    MenuSnapshot snapshot;
    render::MenuRenderData render;
    render.screen_title = screen_title();
    render.performance.visible = config_.graphics.performance_overlay;

    const std::string current_track = current_track_label();
    const BestResultRecord current_best = current_song_best_result();
    rebuild_current_song_record_indices();
    const LocalPlayRecord* selected_record = current_selected_record();

    if (screen_ == Screen::QuickSetup) {
        populate_quick_setup_render_data(render);
    } else if (screen_ == Screen::Title) {
        populate_title_render_data(render, current_track, current_best);
    } else if (screen_ == Screen::SongSelect) {
        populate_song_select_render_data(render, current_track, current_best, selected_record);
    } else if (screen_ == Screen::SongBrowser) {
        populate_song_browser_render_data(render);
    } else if (screen_ == Screen::Gameplay) {
        render.kind = render::MenuScreenKind::GameplayHud;
        render.gameplay.title = last_chart_title_;
        render.gameplay.artist = last_chart_artist_;
        render.gameplay.bpm = last_chart_bpm_;
        populate_gameplay_render_data(render.gameplay);
        if (last_chart_bpm_ > 0.0 && render.gameplay.rate > 0.0) {
            render.gameplay.scroll_speed = (last_chart_bpm_ * render.gameplay.hispeed) / render.gameplay.rate;
        } else {
            render.gameplay.scroll_speed = 0.0;
        }
    } else if (screen_ == Screen::Result) {
        populate_result_render_data(render, current_track);
    } else {
        populate_generic_screen_render_data(render);
    }

    populate_help_overlay(render.help_overlay);
    snapshot.render = std::move(render);
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        snapshot_ = std::move(snapshot);
        ++snapshot_version_;
    }
}

void MenuApp::render_tick() {
    const bool show_performance_overlay = config_.graphics.performance_overlay;
    render::RenderPerformanceSnapshot perf_snapshot =
        show_performance_overlay ? render_thread_.performance_snapshot() : render::RenderPerformanceSnapshot{};
    bool snapshot_changed = false;

    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        if (!render_cache_ready_ || rendered_snapshot_version_ != snapshot_version_) {
            render_cache_ = snapshot_.render;
            rendered_snapshot_version_ = snapshot_version_;
            render_cache_ready_ = true;
            snapshot_changed = true;
        }
        render_cache_.performance.visible =
            show_performance_overlay && render_cache_.kind != render::MenuScreenKind::ResultScreen;
    }

    if (render_cache_.kind == render::MenuScreenKind::GameplayHud) {
        uint64_t gameplay_motion_revision = 0;
        uint64_t gameplay_text_revision = 0;
        int64_t gameplay_hud_publish_time_ns = 0;
        bool gameplay_perf_active = false;
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            gameplay_motion_revision = gameplay_hud_.motion_revision;
            gameplay_text_revision = gameplay_hud_.text_revision;
            gameplay_hud_publish_time_ns = gameplay_hud_.hud_publish_time_ns;
            gameplay_perf_active = gameplay_hud_.active && !gameplay_hud_.loading;
        }
        if (gameplay_perf_active) {
            if (!gameplay_performance_active_) {
                gameplay_performance_tracker_.reset();
                gameplay_performance_last_motion_revision_ = 0;
                gameplay_performance_active_ = true;
            }
            if (gameplay_hud_publish_time_ns > 0 && gameplay_motion_revision != 0 &&
                gameplay_motion_revision != gameplay_performance_last_motion_revision_) {
                gameplay_performance_tracker_.record_frame_start_ns(gameplay_hud_publish_time_ns);
                gameplay_performance_last_motion_revision_ = gameplay_motion_revision;
            }
            perf_snapshot = gameplay_performance_tracker_.snapshot();
        } else if (gameplay_performance_active_) {
            gameplay_performance_tracker_.reset();
            gameplay_performance_last_motion_revision_ = 0;
            gameplay_performance_active_ = false;
        }
        if (snapshot_changed ||
            rendered_gameplay_motion_version_ != gameplay_motion_revision ||
            rendered_gameplay_text_version_ != gameplay_text_revision) {
            populate_gameplay_render_data(render_cache_.gameplay,
                                          &gameplay_motion_revision,
                                          &gameplay_text_revision);
            if (render_cache_.gameplay.bpm > 0.0 && render_cache_.gameplay.rate > 0.0) {
                render_cache_.gameplay.scroll_speed =
                    (render_cache_.gameplay.bpm * render_cache_.gameplay.hispeed) / render_cache_.gameplay.rate;
            } else {
                render_cache_.gameplay.scroll_speed = 0.0;
            }
            rendered_gameplay_motion_version_ = gameplay_motion_revision;
            rendered_gameplay_text_version_ = gameplay_text_revision;
        }

        const bool had_gameplay_metrics_visible = render_cache_.performance.gameplay_metrics_visible;
        render_cache_.performance.gameplay_metrics_visible = gameplay_perf_active;
        if (gameplay_perf_active) {
            const uint64_t gameplay_metrics_revision = (perf_snapshot.metrics_revision != 0) ? perf_snapshot.metrics_revision : 1;
            if (!had_gameplay_metrics_visible ||
                render_cache_.performance.gameplay_metrics_revision != gameplay_metrics_revision) {
                const auto diagnostics = render::compute_gameplay_motion_diagnostics(
                    render::GameplayMotionState{
                        render_cache_.gameplay.current_sample,
                        render_cache_.gameplay.duration_samples,
                        render_cache_.gameplay.sample_rate,
                        render_cache_.gameplay.audio_sample_time_ns,
                        gameplay_hud_publish_time_ns,
                        render_cache_.gameplay.audio_buffer_frames,
                        render_cache_.gameplay.visual_offset_ms,
                        render_cache_.gameplay.finished,
                        render_cache_.gameplay.game_over,
                    },
                    timing::HighResClock::now_ns());
                render_cache_.performance.gameplay_audio_age_ms = diagnostics.audio_age_ms;
                render_cache_.performance.gameplay_hud_delta_ms = diagnostics.hud_delta_ms;
                render_cache_.performance.gameplay_extrapolated_ms = diagnostics.extrapolated_ms;
                render_cache_.performance.gameplay_buffer_ms = diagnostics.buffer_ms;
                render_cache_.performance.gameplay_metrics_revision = gameplay_metrics_revision;
            }
        } else {
            render_cache_.performance.gameplay_metrics_revision = 0;
            render_cache_.performance.gameplay_audio_age_ms = 0.0;
            render_cache_.performance.gameplay_hud_delta_ms = 0.0;
            render_cache_.performance.gameplay_extrapolated_ms = 0.0;
            render_cache_.performance.gameplay_buffer_ms = 0.0;
        }
    } else {
        if (gameplay_performance_active_) {
            gameplay_performance_tracker_.reset();
            gameplay_performance_last_motion_revision_ = 0;
            gameplay_performance_active_ = false;
        }
        render_cache_.performance.gameplay_metrics_visible = false;
        render_cache_.performance.gameplay_metrics_revision = 0;
        render_cache_.performance.gameplay_audio_age_ms = 0.0;
        render_cache_.performance.gameplay_hud_delta_ms = 0.0;
        render_cache_.performance.gameplay_extrapolated_ms = 0.0;
        render_cache_.performance.gameplay_buffer_ms = 0.0;
        rendered_gameplay_motion_version_ = 0;
        rendered_gameplay_text_version_ = 0;
    }

    render_cache_.performance.valid = perf_snapshot.valid;
    render_cache_.performance.sample_count = perf_snapshot.sample_count;
    render_cache_.performance.graph_sample_count = perf_snapshot.graph_sample_count;
    render_cache_.performance.graph_revision = perf_snapshot.graph_revision;
    render_cache_.performance.metrics_revision = perf_snapshot.metrics_revision;
    render_cache_.performance.average_frame_ms = perf_snapshot.average_frame_ms;
    render_cache_.performance.average_fps = perf_snapshot.average_fps;
    render_cache_.performance.max_fps = perf_snapshot.max_fps;
    render_cache_.performance.fps_0_1_low = perf_snapshot.fps_0_1_low;
    render_cache_.performance.fps_0_01_low = perf_snapshot.fps_0_01_low;
    render_cache_.performance.frame_times_ms = perf_snapshot.frame_times_ms;
    menu_window_.render(render_cache_);
}

void MenuApp::render_snapshot(const MenuSnapshot& snapshot) {
    menu_window_.render(snapshot.render);
}

void MenuApp::update_pressed_keys(const input::InputEvent& event) {
    if (event.state == input::InputState::Pressed) {
        pressed_keys_.insert(event.keycode);
        if (screen_ == Screen::SongSelect && is_song_select_repeat_key(event.keycode)) {
            song_select_repeat_key_ = event.keycode;
            song_select_repeat_next_ns_ =
                timing::HighResClock::now_ns() + kSongSelectRepeatInitialDelayNs;
        }
    } else {
        pressed_keys_.erase(event.keycode);
        if (event.keycode == song_select_repeat_key_) {
            reset_song_select_repeat();
        }
    }
    if (screen_ == Screen::KeymapTest) {
        publish_snapshot();
    }
}

void MenuApp::update_song_select_repeat() {
    if (screen_ != Screen::SongSelect) {
        reset_song_select_repeat();
        return;
    }
    if (song_select_repeat_key_ == 0 || !is_song_select_repeat_key(song_select_repeat_key_)) {
        reset_song_select_repeat();
        return;
    }
    if (pressed_keys_.find(song_select_repeat_key_) == pressed_keys_.end()) {
        reset_song_select_repeat();
        return;
    }

    const int64_t now_ns = timing::HighResClock::now_ns();
    if (now_ns < song_select_repeat_next_ns_) {
        return;
    }

    handle_song_select_input(song_select_repeat_key_);
    song_select_repeat_next_ns_ = now_ns + kSongSelectRepeatIntervalNs;
}

void MenuApp::reset_song_select_repeat() {
    song_select_repeat_key_ = 0;
    song_select_repeat_next_ns_ = 0;
}

bool MenuApp::is_song_select_repeat_key(uint32_t keycode) const {
    return keycode == key_up_ || keycode == key_down_ ||
           keycode == key_page_up_ || keycode == key_page_down_;
}

void MenuApp::update_keymap_capture_timeout() {
    if (screen_ != Screen::Keymap || !keymap_capture_active_) {
        return;
    }
    const int64_t now_ns = timing::HighResClock::now_ns();
    if (now_ns >= keymap_capture_deadline_ns_) {
        keymap_capture_active_ = false;
        publish_snapshot();
    }
}

void MenuApp::start_keymap_capture() {
    const int lane_index = keymap_cursor_ - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(keymap_lanes_.size())) {
        return;
    }
    keymap_capture_active_ = true;
    keymap_capture_deadline_ns_ = timing::HighResClock::now_ns() + kKeymapCaptureTimeoutNs;
}

void MenuApp::apply_keymap_capture(uint32_t keycode) {
    const int lane_index = keymap_cursor_ - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(keymap_lanes_.size())) {
        keymap_capture_active_ = false;
        return;
    }
    if (keycode == static_cast<uint32_t>(VK_DELETE)) {
        keymap_capture_active_ = false;
        publish_snapshot();
        return;
    }

    const std::string key_name = config::KeycodeMap::to_name(keycode);
    if (key_name == "Unknown") {
        keymap_capture_active_ = false;
        publish_snapshot();
        return;
    }

    const std::string& lane = keymap_lanes_[static_cast<std::size_t>(lane_index)];
    working_keymap_.mode_bindings[keymap_edit_mode_][lane] = key_name;
    if (keymap_edit_mode_ == "10k") {
        working_keymap_.bindings = working_keymap_.mode_bindings[keymap_edit_mode_];
    }
    keymap_dirty_ = true;
    keymap_capture_active_ = false;
    keymap_pending_lane_.clear();
    keymap_pending_key_.clear();
    keymap_duplicate_lane_.clear();
    publish_snapshot();
}

void MenuApp::apply_keymap_reset() {
    config::KeymapManager manager;
    manager.reset_mode_bindings(working_keymap_, keymap_edit_mode_);
    keymap_dirty_ = true;
}

void MenuApp::apply_keymap_save() {
    config::KeymapManager manager;
    std::string error;
    if (!manager.save_profile(profile_dir_, working_keymap_, &error)) {
        std::cerr << "[error] " << error << std::endl;
        return;
    }
    keymap_ = working_keymap_;
    keymap_dirty_ = false;
}

void MenuApp::exit_keymap_screen() {
    working_keymap_ = keymap_;
    keymap_dirty_ = false;
    keymap_capture_active_ = false;
    keymap_pending_lane_.clear();
    keymap_pending_key_.clear();
    keymap_duplicate_lane_.clear();
    screen_ = submenu_return_screen_;
    publish_snapshot();
}

void MenuApp::apply_song_sort(SongSortMode mode) {
    song_sort_mode_ = mode;
    sort_song_list_preserving_selection();
}

void MenuApp::sort_song_list_preserving_selection() {
    rebuild_visible_song_list();
}

void MenuApp::sync_song_select_state() {
    const bool preserve_records = (song_select_view_ == SongSelectView::Records);
    app::SongSelectState state;
    state.selected_song = selected_song_;
    state.selected_source = selected_source_;
    state.showing_sources = (song_select_view_ == SongSelectView::Sources);
    app::sync_song_select_state(state, visible_song_count(), config_.ui.recent_song_sources.size());
    selected_song_ = state.selected_song;
    selected_source_ = state.selected_source;
    if (state.showing_sources) {
        song_select_view_ = SongSelectView::Sources;
    } else if (preserve_records) {
        song_select_view_ = SongSelectView::Records;
    } else {
        song_select_view_ = SongSelectView::Songs;
    }
}

void MenuApp::reload_chart_best_results() {
    chart_best_results_.clear();
    local_play_records_.clear();
    chart_play_record_indices_.clear();
    current_song_record_indices_.clear();
    replay_summary_cache_.clear();

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path results_dir = path_from_utf8(profile_dir_) / "results";
    if (!fs::exists(results_dir, ec) || !fs::is_directory(results_dir, ec)) {
        return;
    }

    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(results_dir, options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        const std::string ext = to_lower_ascii(entry.path().extension().u8string());
        if (ext != ".json") {
            it.increment(ec);
            continue;
        }

        std::string parse_error;
        auto parsed = menu_records::parse_result_file(entry.path(), &parse_error);
        if (!parsed.has_value()) {
            if (!parse_error.empty()) {
                std::cerr << "[warn] Failed to parse result file " << entry.path().u8string()
                          << ": " << parse_error << std::endl;
            }
            it.increment(ec);
            continue;
        }

        BestResultRecord candidate;
        candidate.has_value = true;
        candidate.rank = menu_records::calculate_rank(parsed->stats, parsed->game_over);
        candidate.best_score = parsed->final_score;
        candidate.clear_status = parsed->clear_status;
        candidate.final_gauge = parsed->final_gauge;
        candidate.game_over = parsed->game_over;
        candidate.max_combo = parsed->stats.max_combo;
        candidate.perfect = parsed->stats.counts.pg;
        candidate.great = parsed->stats.counts.gr;
        candidate.good = parsed->stats.counts.gd;
        candidate.bad = parsed->stats.counts.bd;
        candidate.miss = 0;
        candidate.created_utc = parsed->created_utc;
        const int candidate_judged = menu_records::judged_total(parsed->stats.counts);
        const int candidate_clear_priority =
            menu_records::clear_status_priority(parsed->clear_status, parsed->game_over, parsed->final_gauge);

        LocalPlayRecord record;
        record.chart_path = parsed->chart_path;
        record.chart_format = parsed->chart_format;
        record.created_utc = parsed->created_utc;
        record.result_path = entry.path().u8string();
        record.replay_path = parsed->replay_path;
        record.rank = candidate.rank;
        record.clear_status = parsed->clear_status;
        record.final_gauge = parsed->final_gauge;
        record.game_over = parsed->game_over;
        record.mods = parsed->mods;
        record.rate_multiplier = parsed->rate_multiplier;
        record.score_multiplier = parsed->score_multiplier;
        record.raw_score = parsed->stats.raw_score;
        record.score = candidate.best_score;
        record.accuracy = menu_records::calculate_accuracy(parsed->stats);
        record.max_combo = parsed->stats.max_combo;
        record.total_notes = parsed->stats.total_notes;
        record.judged_notes = candidate_judged;
        record.perfect = parsed->stats.counts.pg;
        record.great = parsed->stats.counts.gr;
        record.good = parsed->stats.counts.gd;
        record.bad = parsed->stats.counts.bd;
        record.miss = 0;
        record.mean_delta_ms = parsed->stats.mean_delta_ms;
        record.stddev_delta_ms = parsed->stats.stddev_delta_ms();
        const std::size_t record_index = local_play_records_.size();
        local_play_records_.push_back(record);

        for (const auto& key : menu_songs::build_chart_path_keys(parsed->chart_path, songs_path_)) {
            chart_play_record_indices_[key].push_back(record_index);
            auto existing = chart_best_results_.find(key);
            if (existing == chart_best_results_.end()) {
                chart_best_results_.emplace(key, candidate);
                continue;
            }

            const int existing_judged = existing->second.perfect + existing->second.great + existing->second.good +
                                        existing->second.bad + existing->second.miss;
            const int existing_clear_priority =
                menu_records::clear_status_priority(existing->second.clear_status,
                                                    existing->second.game_over,
                                                    existing->second.final_gauge);
            if (menu_records::is_better_record(candidate.best_score,
                                               candidate_clear_priority,
                                               candidate.max_combo,
                                               candidate_judged,
                                               candidate.created_utc,
                                               existing->second.best_score,
                                               existing_clear_priority,
                                               existing->second.max_combo,
                                               existing_judged,
                                               existing->second.created_utc)) {
                existing->second = candidate;
            }
        }

        it.increment(ec);
    }

    rebuild_current_song_record_indices();
}

void MenuApp::rebuild_current_song_record_indices() {
    current_song_record_indices_.clear();

    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    if (!entry) {
        selected_record_ = 0;
        return;
    }

    std::unordered_set<std::size_t> seen;
    for (const auto& key : menu_songs::build_chart_path_keys(entry->path, songs_path_)) {
        auto found = chart_play_record_indices_.find(key);
        if (found == chart_play_record_indices_.end()) {
            continue;
        }
        for (std::size_t index : found->second) {
            if (index < local_play_records_.size() && seen.insert(index).second) {
                current_song_record_indices_.push_back(index);
            }
        }
    }

    std::stable_sort(current_song_record_indices_.begin(), current_song_record_indices_.end(),
                     [this](std::size_t lhs_index, std::size_t rhs_index) {
                         const auto& lhs = local_play_records_[lhs_index];
                         const auto& rhs = local_play_records_[rhs_index];
                         const int lhs_clear =
                             menu_records::clear_status_priority(lhs.clear_status, lhs.game_over, lhs.final_gauge);
                         const int rhs_clear =
                             menu_records::clear_status_priority(rhs.clear_status, rhs.game_over, rhs.final_gauge);
                         if (lhs_clear != rhs_clear) {
                             return lhs_clear > rhs_clear;
                         }
                         if (lhs.score != rhs.score) {
                             return lhs.score > rhs.score;
                         }
                         if (lhs.max_combo != rhs.max_combo) {
                             return lhs.max_combo > rhs.max_combo;
                         }
                         return lhs.created_utc > rhs.created_utc;
                     });

    if (current_song_record_indices_.empty()) {
        selected_record_ = 0;
    } else {
        selected_record_ = clamp_int(selected_record_, 0, static_cast<int>(current_song_record_indices_.size() - 1));
    }
}

const MenuApp::LocalPlayRecord* MenuApp::current_selected_record() const {
    if (current_song_record_indices_.empty()) {
        return nullptr;
    }
    if (selected_record_ < 0 || selected_record_ >= static_cast<int>(current_song_record_indices_.size())) {
        return nullptr;
    }
    const std::size_t record_index = current_song_record_indices_[static_cast<std::size_t>(selected_record_)];
    if (record_index >= local_play_records_.size()) {
        return nullptr;
    }
    return &local_play_records_[record_index];
}

const MenuApp::ReplaySummary* MenuApp::replay_summary_for_path(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    auto found = replay_summary_cache_.find(path);
    if (found != replay_summary_cache_.end()) {
        return &found->second;
    }

    ReplaySummary summary;
    summary.loaded = true;
    std::error_code ec;
    summary.exists = std::filesystem::exists(path_from_utf8(path), ec) && !ec;
    if (summary.exists) {
        std::string parse_error;
        auto parsed = menu_records::parse_replay_file(path_from_utf8(path), &parse_error);
        if (parsed.has_value()) {
            summary.sample_rate = parsed->sample_rate;
            summary.lane_count = parsed->lane_count;
            summary.event_count = parsed->event_count;
            summary.duration_samples = parsed->duration_samples;
            summary.rate = parsed->rate;
            summary.input_offset_ms = parsed->input_offset_ms;
            summary.mods = parsed->mods;
            summary.rate_multiplier = parsed->rate_multiplier;
            summary.score_multiplier = parsed->score_multiplier;
            summary.final_score = parsed->final_score;
        } else {
            summary.error = parse_error;
        }
    } else if (ec) {
        summary.error = ec.message();
    } else {
        summary.error = "Replay file not found.";
    }

    auto [it, inserted] = replay_summary_cache_.emplace(path, std::move(summary));
    (void)inserted;
    return &it->second;
}

MenuApp::BestResultRecord MenuApp::current_song_best_result() const {
    BestResultRecord best;
    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    if (!entry) {
        return best;
    }

    try {
        for (const auto& key : menu_songs::build_chart_path_keys(entry->path, songs_path_)) {
            auto found = chart_best_results_.find(key);
            if (found == chart_best_results_.end()) {
                continue;
            }
            if (!best.has_value) {
                best = found->second;
                continue;
            }

            const int best_judged = best.perfect + best.great + best.good + best.bad + best.miss;
            const int found_judged = found->second.perfect + found->second.great + found->second.good +
                                     found->second.bad + found->second.miss;
            if (menu_records::is_better_record(found->second.best_score,
                                               menu_records::clear_status_priority(found->second.clear_status,
                                                                                   found->second.game_over,
                                                                                   found->second.final_gauge),
                                               found->second.max_combo,
                                               found_judged,
                                               found->second.created_utc,
                                               best.best_score,
                                               menu_records::clear_status_priority(best.clear_status,
                                                                                   best.game_over,
                                                                                   best.final_gauge),
                                               best.max_combo,
                                               best_judged,
                                               best.created_utc)) {
                best = found->second;
            }
        }
    } catch (...) {
    }

    return best;
}

bool MenuApp::open_selected_record_result() {
    rebuild_current_song_record_indices();
    const LocalPlayRecord* record = current_selected_record();
    if (!record || record->result_path.empty()) {
        return false;
    }

    std::string parse_error;
    auto parsed = menu_records::parse_result_file(path_from_utf8(record->result_path), &parse_error);
    if (!parsed.has_value()) {
        if (!parse_error.empty()) {
            std::cerr << "[warn] Failed to open saved result " << record->result_path
                      << ": " << parse_error << std::endl;
        }
        return false;
    }

    last_result_ = parsed->stats;
    last_game_over_ = parsed->game_over;
    has_result_ = true;
    last_result_mods_ = parsed->mods;
    last_result_rate_multiplier_ = parsed->rate_multiplier;
    last_result_score_multiplier_ = parsed->score_multiplier;
    last_result_final_score_ = parsed->final_score;
    last_replay_path_ = record->replay_path;
    last_result_path_ = record->result_path;
    last_export_warnings_.clear();
    last_session_replay_playback_ = false;
    if (const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                       : nullptr) {
        last_chart_title_ = entry->title.empty() ? entry->path : entry->title;
        last_chart_artist_ = entry->artist;
        last_chart_bpm_ = entry->bpm;
    } else {
        last_chart_title_ = filename_only(parsed->chart_path);
        last_chart_artist_.clear();
        last_chart_bpm_ = 0.0;
    }
    screen_ = Screen::Result;
    return true;
}

bool MenuApp::launch_replay_from_path(const std::string& replay_path, const std::string& fallback_chart_path) {
    if (replay_path.empty()) {
        return false;
    }

    auto replay_load = gameplay::load_replay_json(replay_path);
    if (!replay_load.success()) {
        std::cerr << "[warn] Failed to load replay for playback " << replay_path
                  << ": " << replay_load.error << std::endl;
        return false;
    }
    for (const auto& warning : replay_load.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }

    std::string chart_path = replay_load.replay->chart_path;
    if (chart_path.empty()) {
        chart_path = fallback_chart_path;
    }
    if (chart_path.empty()) {
        std::cerr << "[warn] Replay file does not contain a chart path: " << replay_path << std::endl;
        return false;
    }

    launch_gameplay(chart_path, replay_path);
    return true;
}

bool MenuApp::launch_last_result_replay() {
    return launch_replay_from_path(last_replay_path_);
}

bool MenuApp::launch_selected_record_replay() {
    rebuild_current_song_record_indices();
    const LocalPlayRecord* record = current_selected_record();
    if (!record) {
        return false;
    }
    return launch_replay_from_path(record->replay_path, record->chart_path);
}

std::string MenuApp::selected_song_absolute_path() const {
    if (selected_song_ < 0) {
        return {};
    }
    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    if (!entry) {
        return {};
    }

    return song_absolute_path(*entry);
}

std::string MenuApp::song_absolute_path(const SongEntry& entry) const {
    namespace fs = std::filesystem;
    fs::path candidate = path_from_utf8(entry.path);
    if (!candidate.is_absolute()) {
        fs::path rooted = path_from_utf8(songs_path_) / candidate;
        std::error_code ec;
        if (fs::exists(rooted, ec) && !ec) {
            candidate = std::move(rooted);
        }
    }

    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(candidate, ec);
    if (!ec && !canonical.empty()) {
        candidate = canonical;
    } else {
        ec.clear();
        candidate = candidate.lexically_normal();
    }
    return candidate.u8string();
}

std::string MenuApp::song_background_preview_path_for_entry(const SongEntry& entry) {
    const std::string chart_path = song_absolute_path(entry);
    if (chart_path.empty()) {
        return {};
    }

    const std::string cache_key = menu_songs::normalize_path_key(path_from_utf8(chart_path));
    if (!cache_key.empty()) {
        auto cached = song_background_preview_cache_.find(cache_key);
        if (cached != song_background_preview_cache_.end()) {
            return cached->second;
        }
    }

    const std::string resolved_path = menu_songs::resolve_song_background_preview_path(chart_path);

    if (!cache_key.empty()) {
        song_background_preview_cache_[cache_key] = resolved_path;
    }
    return resolved_path;
}

std::string MenuApp::selected_song_background_preview_path() {
    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    if (!entry) {
        return {};
    }
    return song_background_preview_path_for_entry(*entry);
}

void MenuApp::launch_selected_song() {
    const std::string chart_path = selected_song_absolute_path();
    if (chart_path.empty()) {
        return;
    }
    launch_gameplay(chart_path);
}

void MenuApp::launch_gameplay(const std::string& chart_path, const std::string& replay_path) {
    const bool replay_playback = !replay_path.empty();
    const std::string preserved_result_path = last_result_path_;
    if (selected_song_ >= 0 && selected_song_ < static_cast<int>(visible_song_count())) {
        if (const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_))) {
            last_chart_title_ = entry->title.empty() ? entry->path : entry->title;
            last_chart_artist_ = entry->artist;
            last_chart_bpm_ = entry->bpm;
        }
    } else {
        last_chart_title_.clear();
        last_chart_artist_.clear();
        last_chart_bpm_ = 0.0;
    }

    screen_ = Screen::Gameplay;
    apply_runtime_graphics_config();
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        reset_gameplay_hud_state(gameplay_hud_);
        gameplay_hud_.loading = true;
        gameplay_hud_.loading_percent = 0;
        gameplay_hud_.loading_stage = "Preparing gameplay";
    }
    publish_snapshot();

    input_thread_.stop();
    audio_thread_.shutdown();

    GameSession session;
    session.set_loading_progress_callback([this](const GameSession::LoadingProgress& progress) {
        update_gameplay_loading_state(progress.percent, progress.stage);
    });
#ifdef _WIN32
    auto escape_was_down = std::make_shared<std::atomic<bool>>((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0);
    session.set_loading_cancel_callback([escape_was_down]() {
        const bool escape_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool fresh_press = escape_down && !escape_was_down->exchange(escape_down, std::memory_order_acq_rel);
        return fresh_press;
    });
#else
    session.set_loading_cancel_callback([]() { return false; });
#endif
    session.set_hud_callback([this](const GameSession::HudSnapshot& hud) {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        const GameplayHudRevisionInput previous = gameplay_hud_revision_input(gameplay_hud_);
        GameplayHudRevisionInput next = previous;
        next.loading = false;
        next.loading_percent = 100;
        next.loading_stage = "Ready";
        next.active = hud.active;
        next.finished = hud.finished;
        next.game_over = hud.game_over;
        next.user_aborted = hud.user_aborted;
        next.countdown_active = hud.countdown_active;
        next.countdown_value = hud.countdown_value;
        next.lane_count = hud.lane_count;
        next.current_sample = hud.current_sample;
        next.duration_samples = hud.duration_samples;
        next.sample_rate = hud.sample_rate;
        next.audio_sample_time_ns = hud.audio_sample_time_ns;
        next.audio_buffer_frames = hud.audio_buffer_frames;
        next.lookahead_samples = hud.lookahead_samples;
        next.past_samples = hud.past_samples;
        next.combo = hud.combo;
        next.max_combo = hud.max_combo;
        next.counts = hud.counts;
        next.score = hud.score;
        next.gauge = hud.gauge;
        next.gauge_type = hud.gauge_type;
        next.rate = hud.rate;
        next.hispeed = hud.hispeed;
        next.has_feedback = hud.has_feedback;
        next.feedback = hud.feedback_judgement;
        next.feedback_delta_ms = hud.feedback_delta_ms;
        next.lane_activity_count = hud.lane_activity_count;
        next.lane_activity.fill(0.0f);
        std::copy_n(hud.lane_activity.begin(), hud.lane_activity_count, next.lane_activity.begin());
        next.note_count = hud.note_count;
        for (std::size_t i = 0; i < hud.note_count; ++i) {
            next.notes[i] = GameplayHudRevisionNote{
                hud.notes[i].lane,
                hud.notes[i].start_sample,
                hud.notes[i].tail_sample,
                hud.notes[i].hold,
                hud.notes[i].head_visible,
            };
        }
        const GameplayHudRevisionFlags diff = diff_gameplay_hud_revisions(previous, next);

        gameplay_hud_.loading = false;
        gameplay_hud_.loading_percent = 100;
        gameplay_hud_.loading_stage = "Ready";
        gameplay_hud_.active = hud.active;
        gameplay_hud_.finished = hud.finished;
        gameplay_hud_.game_over = hud.game_over;
        gameplay_hud_.user_aborted = hud.user_aborted;
        gameplay_hud_.countdown_active = hud.countdown_active;
        gameplay_hud_.countdown_value = hud.countdown_value;
        gameplay_hud_.lane_count = hud.lane_count;
        gameplay_hud_.current_sample = hud.current_sample;
        gameplay_hud_.duration_samples = hud.duration_samples;
        gameplay_hud_.sample_rate = hud.sample_rate;
        gameplay_hud_.audio_sample_time_ns = hud.audio_sample_time_ns;
        gameplay_hud_.hud_publish_time_ns = hud.hud_publish_time_ns;
        gameplay_hud_.audio_buffer_frames = hud.audio_buffer_frames;
        gameplay_hud_.lookahead_samples = hud.lookahead_samples;
        gameplay_hud_.past_samples = hud.past_samples;
        gameplay_hud_.combo = hud.combo;
        gameplay_hud_.max_combo = hud.max_combo;
        gameplay_hud_.counts = hud.counts;
        gameplay_hud_.score = hud.score;
        gameplay_hud_.gauge = hud.gauge;
        gameplay_hud_.gauge_type = hud.gauge_type;
        gameplay_hud_.rate = hud.rate;
        gameplay_hud_.hispeed = hud.hispeed;
        gameplay_hud_.has_feedback = hud.has_feedback;
        gameplay_hud_.feedback = hud.feedback_judgement;
        gameplay_hud_.feedback_delta_ms = hud.feedback_delta_ms;
        gameplay_hud_.lane_activity_count = hud.lane_activity_count;
        gameplay_hud_.lane_activity.fill(0.0f);
        std::copy_n(hud.lane_activity.begin(), hud.lane_activity_count, gameplay_hud_.lane_activity.begin());

        gameplay_hud_.note_count = hud.note_count;
        for (std::size_t i = 0; i < hud.note_count; ++i) {
            GameplayHudState::Note out;
            out.lane = hud.notes[i].lane;
            out.start_sample = hud.notes[i].start_sample;
            out.tail_sample = hud.notes[i].tail_sample;
            out.hold = hud.notes[i].hold;
            out.head_visible = hud.notes[i].head_visible;
            gameplay_hud_.notes[i] = out;
        }
        advance_gameplay_hud_revisions(gameplay_hud_, diff.motion_changed, diff.text_changed);
    });

    CommandLineOptions play_options = options_;
    play_options.chart_path = chart_path;
    play_options.replay_path = replay_path;
    if (!session.initialize(play_options)) {
        const bool loading_canceled = session.was_user_aborted();
        session.shutdown();
        if (!loading_canceled) {
            std::cerr << "[error] Failed to initialize gameplay session." << std::endl;
        }
        restart_input_thread();
        restart_audio_thread();
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            reset_gameplay_hud_state(gameplay_hud_);
        }
        screen_ = Screen::SongSelect;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }

    session.run();
    session.shutdown();

    const double session_hispeed = session.final_hispeed();
    if (std::abs(session_hispeed - config_.speed.hi_speed) > 0.0001) {
        config_.speed.hi_speed = session_hispeed;
        persist_runtime_config();
    }

    restart_input_thread();
    restart_audio_thread();
    const auto& result = session.result();
    if (result.has_value) {
        last_result_ = result.stats;
        last_game_over_ = result.game_over;
        has_result_ = true;
        last_result_mods_ = result.mods;
        last_result_rate_multiplier_ = result.rate_multiplier;
        last_result_score_multiplier_ = result.score_multiplier;
        last_result_final_score_ = result.final_score;
        last_replay_path_ = result.replay_path;
        last_result_path_ = (!result.result_path.empty() || !replay_playback) ? result.result_path : preserved_result_path;
        last_export_warnings_ = result.export_warnings;
        last_session_replay_playback_ = replay_playback;
        reload_chart_best_results();
        screen_ = Screen::Result;
    } else {
        has_result_ = false;
        last_result_mods_.clear();
        last_result_rate_multiplier_ = 1.0;
        last_result_score_multiplier_ = 1.0;
        last_result_final_score_ = 0;
        last_replay_path_.clear();
        last_result_path_.clear();
        last_export_warnings_.clear();
        last_session_replay_playback_ = false;
        screen_ = Screen::SongSelect;
    }
    apply_runtime_graphics_config();
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        reset_gameplay_hud_state(gameplay_hud_);
    }
    publish_snapshot();
}

std::string MenuApp::screen_title() const {
    switch (screen_) {
        case Screen::QuickSetup: return "Quick Setup";
        case Screen::Title: return "Title";
        case Screen::OptionsHub: return "Options";
        case Screen::EditStub: return "Edit";
        case Screen::SongSelect:
            if (song_select_view_ == SongSelectView::Sources) {
                return "Song Sources";
            }
            if (song_select_view_ == SongSelectView::Records) {
                return "Local Records";
            }
            return "Song Select";
        case Screen::SongBrowser: return "Song Browser";
        case Screen::Gameplay: return "Gameplay";
        case Screen::SettingsAudio: return "Audio Settings";
        case Screen::SettingsGraphics: return "Graphics Settings";
        case Screen::SettingsSkins: return "Skin Settings";
        case Screen::SettingsInput: return "Input Settings";
        case Screen::ModeSelect: return "Mode Select";
        case Screen::ModeMods: return "Mod Manager";
        case Screen::Keymap: return "Keymap";
        case Screen::KeymapConfirm: return "Keymap Confirm";
        case Screen::KeymapTest: return "NKRO Test";
        case Screen::Result: return "Result";
        default: return "Menu";
    }
}

void MenuApp::populate_help_overlay(render::HelpOverlayData& target) const {
    target.visible = help_overlay_visible_ && screen_ != Screen::Gameplay;
    if (!target.visible) {
        return;
    }

    switch (screen_) {
        case Screen::QuickSetup:
            target.title = "Quick Setup";
            target.lines = {
                "TenRiff already created a default profile and default keymap for this first launch.",
                "Songs Folder opens a picker on Enter or F2. You can also drag and drop a folder later.",
                "Recommended starting values are Gauge Normal, Rate 1.00x, Display Offset 0ms, and BMS Keysound Follow.",
                "Left / Right changes the highlighted setting. Continue saves the current values and opens Song Select.",
            };
            target.footer = "Esc or Backspace skips to the title screen. Press F1 again to close help.";
            return;
        case Screen::Title:
            target.title = "Title Controls";
            target.lines = {
                "Up / Down or the mouse selects PLAY, EDIT, OPTIONS, or EXIT.",
                "Enter or double-click opens the selected button.",
                "F5 refreshes the current song source before you enter Song Select.",
                "A / G / I / M / K jumps straight to Audio, Graphics, Input, Mode, and Keymap.",
            };
            target.footer = "Esc exits TenRiff. Press F1 again to close help.";
            return;
        case Screen::SongSelect:
            target.title = "Song Select Controls";
            target.lines = {
                "Up / Down or the mouse wheel moves through the current list. PgUp / PgDn jumps by a page.",
                "Left / Right switches focus between the left navigation rail and the song list.",
                "Enter selects the focused item. Double-click on a song launches it immediately.",
                "Backspace jumps to Sources or back to Songs. Esc returns to the title screen.",
                "F2 chooses a new songs folder. F5 refreshes the active source.",
                "Safe indexing lowers RAM use for very large libraries. Fast rescans quicker on high-memory PCs.",
                "A / G / I / M / K opens Audio, Graphics, Input, Mode, and Keymap from Song Select.",
            };
            target.footer = "Current source, filter, sort, and indexing profile stay visible on the Song Select screen.";
            return;
        case Screen::SongBrowser:
            target.title = "Browse Help";
            target.lines = {
                "Search matches title, artist, and chart path.",
                "Up / Down or the mouse wheel moves the selection. Long lists now show a scrollbar on the right.",
                "Type while Search is selected. Backspace deletes one character. Delete clears the whole query.",
                "Left / Right adjusts key and difficulty filters. Enter activates Clear Filters or Back.",
            };
            target.footer = "Esc or Backspace returns to Song Select. Press F1 again to close help.";
            return;
        case Screen::SettingsAudio:
            target.title = "Audio Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                "Left / Right or the +/- buttons changes the current value.",
                "Follow keeps note keysounds tied to your hits. Autoplay mixes them into background audio instead.",
                "Esc or Backspace saves the current values and returns.",
            };
            target.footer = "A later return to Song Select keeps these values live.";
            return;
        case Screen::SettingsGraphics:
            target.title = "Graphics Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                "Display, Resolution, Refresh Hz, and VSync apply live while you adjust them.",
                "Display Offset shifts visuals only. Positive values draw notes earlier without moving judgement.",
                "Esc or Backspace saves and returns.",
            };
            target.footer = "Use this screen when notes feel visually early or late on your display.";
            return;
        case Screen::SettingsSkins:
            target.title = "Skin Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long skin lists now scroll with a right-side scrollbar.",
                "Skin Source swaps between the native vector skin and imported osu!mania assets.",
                "Import OSU Skin opens a folder picker. Drag-and-drop also works on this screen.",
                "Image Aspect keeps imported note heads and tails from stretching to the gameplay note box.",
                "The right preview shows the native fallback lane colors and sizing per layout.",
            };
            target.footer = "Esc or Backspace saves and returns.";
            return;
        case Screen::SettingsInput:
            target.title = "Input Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row.",
                "Polling Hz changes how often keyboard state is sampled.",
                "Debounce filters switch chatter before gameplay receives duplicate presses.",
                "Esc or Backspace saves and returns.",
            };
            target.footer = "Input changes apply after the input thread restarts when you leave the screen.";
            return;
        case Screen::ModeSelect:
            target.title = "Mode Settings";
            target.lines = {
                "Up / Down or the mouse wheel selects a row. Long lists show a scrollbar on the right.",
                "Gauge, Random, Mods, Rate, and Hi-Speed change the play feel for the next song.",
                "Indexing Safe minimizes RAM for huge libraries. Fast spends more RAM to speed up rescans.",
                "Chart Filter decides whether Song Select shows BMS, OSU, or both when OSU indexing is enabled.",
                "Enter on Mods opens the registry-backed Mod Manager for score-multiplier presets.",
            };
            target.footer = "Esc or Backspace saves and refreshes the library if needed.";
            return;
        case Screen::ModeMods:
            target.title = "Mod Manager";
            target.lines = {
                "Up / Down selects a mod category. Left / Right cycles Off and the registered preset for that category.",
                "Judge Window, Note Structure, and Hold Rule normalize to one active preset per category.",
                "Final score uses the lowest multiplier between the current Rate and all active mods.",
            };
            target.footer = "Enter, Esc, or Backspace returns to Mode Settings.";
            return;
        case Screen::Keymap:
            target.title = "Keymap Help";
            target.lines = {
                "Up / Down selects a lane. Enter starts key capture for that lane.",
                "Left / Right on Key Mode switches which 4K-10K or 16K layout you are editing.",
                "A saves, R resets, F2 opens NKRO Test, and Esc returns.",
            };
            target.footer = "Duplicate lane bindings are allowed.";
            return;
        case Screen::KeymapTest:
            target.title = "NKRO Test";
            target.lines = {
                "Press multiple keys together to verify rollover and ghosting behavior.",
                "Highlighted lanes show which mapped keys TenRiff is currently receiving.",
            };
            target.footer = "Esc or Backspace returns to Keymap.";
            return;
        case Screen::Result:
            target.title = "Result Screen";
            target.lines = {
                "This panel shows the saved score, timing spread, gauge trace, and export file names for the run.",
                "R or the Replay button starts playback from the saved replay file when one is available.",
                "Enter, Esc, and Backspace all return to Song Select.",
            };
            target.footer = "Back and confirm are intentionally mirrored here for faster recovery after each song.";
            return;
        case Screen::OptionsHub:
            target.title = "Options Hub";
            target.lines = {
                "Enter opens Audio, Graphics, Skins, Input, Mode, or Keymap.",
                "A / G / I / M / K jumps directly to the most common settings screens.",
            };
            target.footer = "Esc or Backspace returns to the previous screen.";
            return;
        default:
            target.title = "Controls";
            target.lines = {
                "Up / Down selects items. Left / Right changes adjustable values.",
                "Enter confirms the focused action. Esc or Backspace returns.",
            };
            target.footer = "Press F1 again to close help.";
            return;
    }
}

const SongEntry* MenuApp::visible_song_entry(std::size_t visible_index) const {
    if (visible_index >= visible_song_indices_.size()) {
        return nullptr;
    }
    const std::size_t song_index = visible_song_indices_[visible_index];
    if (song_index >= indexed_songs_.size()) {
        return nullptr;
    }
    return &indexed_songs_[song_index];
}

std::string MenuApp::selected_song_path() const {
    if (selected_song_ < 0) {
        return {};
    }
    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    return entry ? entry->path : std::string{};
}

std::string MenuApp::format_song_line(std::size_t index) const {
    const SongEntry* entry = visible_song_entry(index);
    if (!entry) {
        return "";
    }
    std::string label = std::to_string(index + 1) + ". ";
    label += song_title_for_ui(*entry);
    const std::string artist = song_artist_for_ui(*entry);
    if (!artist.empty()) {
        label += " - " + artist;
    }
    if (!entry->format.empty()) {
        label += " [" + safe_ui_text_or_placeholder(entry->format, "<invalid format>");
        if (entry->key_count > 0) {
            label += " " + std::to_string(entry->key_count) + "K";
        }
        label += "]";
    }
    if (entry->bpm > 0.0) {
        label += " BPM " + std::to_string(static_cast<int>(entry->bpm));
    }
    return label;
}

void MenuApp::update_song_list(SongIndex index) {
    std::string selected_path = selected_song_path();
    song_background_preview_cache_.clear();
    indexed_songs_ = std::move(index.entries);
    for (auto& entry : indexed_songs_) {
        entry.title = safe_ui_text(entry.title);
        entry.artist = safe_ui_text(entry.artist);
        entry.format = safe_ui_text(entry.format);
    }
    rebuild_visible_song_list(selected_path.empty() ? nullptr : &selected_path);
    sync_song_select_state();
}

void MenuApp::rebuild_visible_song_list(const std::string* selected_path) {
    std::string preserved_path;
    if (selected_path) {
        preserved_path = *selected_path;
    } else {
        preserved_path = selected_song_path();
    }

    visible_song_indices_.clear();
    visible_song_indices_.reserve(indexed_songs_.size());
    const std::string chart_filter =
        config_.mode.enable_osu_charts ? normalize_chart_filter(config_.mode.format) : std::string("bms");
    for (std::size_t index = 0; index < indexed_songs_.size(); ++index) {
        const SongEntry& entry = indexed_songs_[index];
        if (song_entry_matches_chart_filter(entry, chart_filter) &&
            song_entry_matches_search(entry, song_search_query_) &&
            song_entry_matches_key_filter(entry, song_key_filter_) &&
            song_entry_matches_level_filter(entry, song_level_min_filter_, song_level_max_filter_)) {
            visible_song_indices_.push_back(index);
        }
    }

    if (song_sort_mode_ == SongSortMode::DifficultyAsc) {
        std::stable_sort(visible_song_indices_.begin(), visible_song_indices_.end(), [this](std::size_t lhs,
                                                                                            std::size_t rhs) {
            return song_entry_less_by_difficulty_asc(indexed_songs_[lhs], indexed_songs_[rhs]);
        });
    } else if (song_sort_mode_ == SongSortMode::DifficultyDesc) {
        std::stable_sort(visible_song_indices_.begin(), visible_song_indices_.end(), [this](std::size_t lhs,
                                                                                            std::size_t rhs) {
            return song_entry_less_by_difficulty_desc(indexed_songs_[lhs], indexed_songs_[rhs]);
        });
    } else if (song_sort_mode_ == SongSortMode::TitleAsc) {
        std::stable_sort(visible_song_indices_.begin(), visible_song_indices_.end(), [this](std::size_t lhs,
                                                                                             std::size_t rhs) {
            return song_entry_less_by_title_asc(indexed_songs_[lhs], indexed_songs_[rhs]);
        });
    } else {
        std::stable_sort(visible_song_indices_.begin(), visible_song_indices_.end(), [this](std::size_t lhs,
                                                                                             std::size_t rhs) {
            return song_entry_less_by_title_desc(indexed_songs_[lhs], indexed_songs_[rhs]);
        });
    }

    if (visible_song_indices_.empty()) {
        selected_song_ = 0;
        sync_song_select_state();
        if (!songs_path_.empty()) {
            source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] = 0;
        }
        return;
    }

    std::vector<SongEntry> selection_view;
    selection_view.reserve(visible_song_indices_.size());
    for (std::size_t song_index : visible_song_indices_) {
        selection_view.push_back(indexed_songs_[song_index]);
    }
    selected_song_ = resolve_selected_song_index(selection_view,
                                                 selected_song_,
                                                 preserved_path.empty() ? nullptr : &preserved_path);
    sync_song_select_state();
    rebuild_current_song_record_indices();

    source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] =
        static_cast<int>(visible_song_count());
    log_memory_phase("MenuApp",
                     "visible-list-rebuilt",
                     query_process_memory_snapshot(),
                     "entries=" + std::to_string(indexed_songs_.size()) +
                         " visible=" + std::to_string(visible_song_count()));
}

}  // namespace tenriff::app
