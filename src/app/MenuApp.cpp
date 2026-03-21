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
#include "app/MenuAppSongSelectUtils.h"
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

namespace tenriff::app {

namespace {

using menu_song_select::filename_only;
using menu_song_select::path_from_utf8;
using menu_song_select::safe_ui_text;
using menu_song_select::safe_ui_text_or_placeholder;
using menu_song_select::song_artist_for_ui;
using menu_song_select::song_title_for_ui;

constexpr int kSnapshotSongCount = 10;
constexpr int kSongSelectVisibleCardCount = 5;
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

}  // namespace

MenuApp::MenuApp() = default;

bool MenuApp::ui_uses_korean() const {
    return config::normalize_ui_language_token(config_.ui.language) == "ko";
}

std::string MenuApp::ui_text(std::string_view english, std::string_view korean) const {
    return std::string(ui_uses_korean() ? korean : english);
}

std::string MenuApp::ui_on_off(bool enabled) const {
    return ui_text(enabled ? "On" : "Off", enabled ? "켜짐" : "꺼짐");
}

std::string MenuApp::ui_language_label(std::string_view token) const {
    const std::string normalized = config::normalize_ui_language_token(token);
    if (normalized == "ko") {
        return ui_text("Korean", "한국어");
    }
    return ui_text("English", "영어");
}

std::string MenuApp::ui_display_mode_label(std::string_view token) const {
    const std::string normalized = normalize_display_mode(std::string(token));
    if (normalized == "windowed") {
        return ui_text("Windowed", "창 모드");
    }
    if (normalized == "fullscreen") {
        return ui_text("Fullscreen", "전체 화면");
    }
    return ui_text("Borderless", "테두리 없음");
}

std::string MenuApp::ui_resolution_label(std::string_view token) const {
    const std::string normalized = normalize_resolution_preset(std::string(token));
    if (normalized == "720p") {
        return "1280x720";
    }
    if (normalized == "1080p") {
        return "1920x1080";
    }
    if (normalized == "qhd") {
        return "2560x1440";
    }
    return ui_text("Monitor Native", "모니터 기본");
}

std::string MenuApp::ui_preset_label(std::string_view token) const {
    if (to_lower_ascii(std::string(token)) == "high") {
        return ui_text("High", "고성능");
    }
    return ui_text("Basic", "기본");
}

std::string MenuApp::ui_keysound_policy_label(std::string_view token) const {
    const std::string normalized = to_lower_ascii(std::string(token));
    if (normalized == "autoplay") {
        return ui_text("Autoplay", "자동재생");
    }
    if (normalized == "ignore" || normalized == "off") {
        return ui_text("Off", "끔");
    }
    return ui_text("Follow", "연동");
}

std::string MenuApp::ui_song_index_profile_label(std::string_view token) const {
    if (config::normalize_song_index_profile_token(token) == "fast") {
        return ui_text("Fast", "빠름");
    }
    return ui_text("Safe", "안전");
}

std::string MenuApp::ui_chart_filter_label(std::string_view token) const {
    const std::string normalized = normalize_chart_filter(std::string(token));
    if (normalized == "bms") {
        return "BMS";
    }
    if (normalized == "osu") {
        return "OSU";
    }
    return ui_text("All", "전체");
}

std::string MenuApp::ui_key_mode_label(std::string_view token) const {
    const std::string normalized = normalize_runtime_key_mode(std::string(token));
    if (normalized == "none") {
        return ui_text("None", "원본");
    }
    return key_mode_label(normalized);
}

std::string MenuApp::ui_gauge_label(std::string_view token) const {
    const std::string normalized = to_lower_ascii(std::string(token));
    if (normalized == "hard") {
        return ui_text("Hard", "하드");
    }
    if (normalized == "easy") {
        return ui_text("Easy", "이지");
    }
    return ui_text("Normal", "노말");
}

std::string MenuApp::ui_random_label(std::string_view token) const {
    const std::string normalized = to_lower_ascii(std::string(token));
    if (normalized == "fr") {
        return "FR";
    }
    if (normalized == "sr") {
        return "SR";
    }
    return ui_text("Off", "끔");
}

std::string MenuApp::ui_skin_source_label(std::string_view token) const {
    const std::string normalized = config::normalize_skin_source_token(token);
    if (normalized == "osu") {
        return "osu!mania";
    }
    if (normalized == "lr2") {
        return "LR2";
    }
    return ui_text("Native", "기본");
}

std::string MenuApp::ui_skin_note_shape_label(std::string_view token) const {
    if (config::normalize_skin_note_shape_token(token) == "circle") {
        return ui_text("Circle", "원형");
    }
    return ui_text("Rect", "사각형");
}

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
    input.ghost_visible = state.ghost_visible;
    input.ghost_score = state.ghost_score;
    input.ghost_combo = state.ghost_combo;
    input.ghost_max_combo = state.ghost_max_combo;
    input.ghost_counts = state.ghost_counts;
    input.ghost_gauge = state.ghost_gauge;
    input.ghost_gauge_type = state.ghost_gauge_type;
    input.ghost_has_feedback = state.ghost_has_feedback;
    input.ghost_feedback = state.ghost_feedback;
    input.ghost_feedback_delta_ms = state.ghost_feedback_delta_ms;
    input.ghost_finished = state.ghost_finished;
    input.ghost_game_over = state.ghost_game_over;
    input.ghost_lane_activity_count = state.ghost_lane_activity_count;
    std::copy_n(state.ghost_lane_activity.begin(),
                state.ghost_lane_activity_count,
                input.ghost_lane_activity.begin());
    input.ghost_note_count = state.ghost_note_count;
    for (std::size_t i = 0; i < state.ghost_note_count; ++i) {
        input.ghost_notes[i] = GameplayHudRevisionNote{
            state.ghost_notes[i].lane,
            state.ghost_notes[i].start_sample,
            state.ghost_notes[i].tail_sample,
            state.ghost_notes[i].hold,
            state.ghost_notes[i].head_visible,
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
    const bool migrated_config = config_result.migrated;
    const bool stripped_session_only_mods = strip_session_only_mode_mods(config_);
    first_run_profile_ = config_result.used_defaults;

    if (config_result.used_defaults || migrated_config || stripped_session_only_mods) {
        const config::RuntimeConfig persisted = build_persisted_runtime_config(config_);
        config_loader.save_profile(profile_dir_, persisted);
    }
    config_.graphics.refresh_hz =
        clamp_int(config_.graphics.refresh_hz, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
    config_.graphics.resolution = normalize_resolution_preset(config_.graphics.resolution);
    refresh_song_collection_membership_cache();
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
    key_f9_ = config::KeycodeMap::to_keycode("F9").value_or(0);

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
        if (!input_thread_.start()) {
            std::cerr << "[error] Failed to start input thread." << std::endl;
        }
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
    if (!input_thread_.start()) {
        std::cerr << "[error] Failed to restart input thread." << std::endl;
    }
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

    if (event.state == input::InputState::Pressed &&
        key_f9_ != 0 && event.keycode == key_f9_) {
        menu_window_.request_screenshot();
        return;
    }

    if (screen_ == Screen::Keymap && keymap_capture_active_) {
        if (event.state == input::InputState::Pressed) {
            apply_keymap_capture(event.keycode);
        }
        return;
    }

    if (event.state != input::InputState::Pressed) {
        return;
    }

    const bool help_overlay_supported =
        screen_ != Screen::Gameplay && screen_ != Screen::Result;
    if (help_overlay_supported && key_f1_ != 0 && event.keycode == key_f1_) {
        help_overlay_visible_ = !help_overlay_visible_;
        publish_snapshot();
        return;
    }
    if (help_overlay_supported && help_overlay_visible_) {
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
        case Screen::SettingsCalibration:
            handle_calibration_settings_input(event.keycode);
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
                    case Screen::SettingsCalibration:
                        handle_calibration_settings_input(direction_key);
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
                options_cursor_ = clamp_int(event.index - 1, 0, 7);
                publish_snapshot();
                return;
            }
            options_cursor_ = clamp_int(event.index, 0, 7);
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
                8);
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
            settings_cursor_ = clamp_int(event.index, 0, 7);
            handle_graphics_settings_input(action_key);
            return;
        case Screen::SongBrowser:
            settings_cursor_ = clamp_int(event.index, 0, 5);
            handle_song_browser_input(action_key);
            return;
        case Screen::SettingsSkins:
            settings_cursor_ = clamp_int(event.index, 0,
                                         19 + (config::normalize_skin_source_token(config_.skin.source) == "lr2" ? 1 : 0));
            handle_skins_settings_input(action_key);
            return;
        case Screen::SettingsInput:
            settings_cursor_ = clamp_int(event.index, 0, 2);
            handle_input_settings_input(action_key);
            return;
        case Screen::SettingsCalibration:
            settings_cursor_ = clamp_int(event.index, 0, 4);
            handle_calibration_settings_input(action_key);
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
        std::string new_path = browse_for_folder(ui_text("Select Songs Folder", "곡 폴더 선택"));
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
    constexpr int item_count = 8;
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
                screen_ = Screen::SettingsCalibration;
                settings_cursor_ = 0;
                break;
            case 5:
                submenu_return_screen_ = return_screen;
                screen_ = Screen::ModeSelect;
                settings_cursor_ = 0;
                break;
            case 6:
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
        std::string new_path = browse_for_folder(ui_text("Select Songs Folder", "곡 폴더 선택"));
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
            song_select_nav_cursor_ = clamp_int(song_select_nav_cursor_ - 1, 0, 8);
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
            song_select_nav_cursor_ = clamp_int(song_select_nav_cursor_ + 1, 0, 8);
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
                    if (to_lower_ascii(config_.ui.song_collection_filter) == "favorites") {
                        config_.ui.song_collection_filter = "all";
                    } else {
                        config_.ui.song_collection_filter = "favorites";
                    }
                    persist_runtime_config();
                    song_select_view_ = SongSelectView::Songs;
                    rebuild_visible_song_list();
                    rebuild_current_song_record_indices();
                    song_select_focus_ = SongSelectFocus::SongList;
                    publish_snapshot();
                    return;
                case 5:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::SongBrowser;
                    settings_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 6:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::ModeSelect;
                    settings_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 7:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::OptionsHub;
                    options_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 8:
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
    const int item_count = 9;
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
    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        cycle_song_collection_filter(direction);
        persist_runtime_config();
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
        if (settings_cursor_ == 5) {
            const std::string named_collection = current_named_song_collection();
            bool changed = false;
            if (!named_collection.empty()) {
                changed = toggle_selected_song_in_collection(named_collection);
            } else {
                changed = toggle_selected_song_favorite();
            }
            if (changed) {
                persist_runtime_config();
                if (to_lower_ascii(config_.ui.song_collection_filter) == "favorites" ||
                    !named_collection.empty()) {
                    apply_filter_refresh();
                } else {
                    publish_snapshot();
                }
            }
            return;
        }
        if (settings_cursor_ == 6) {
            create_next_song_collection();
            persist_runtime_config();
            apply_filter_refresh();
            return;
        }
        if (settings_cursor_ == 7) {
            song_search_query_.clear();
            song_key_filter_ = 0;
            song_level_min_filter_ = 0;
            song_level_max_filter_ = 0;
            config_.ui.song_collection_filter = "all";
            persist_runtime_config();
            apply_filter_refresh();
            return;
        }
        if (settings_cursor_ == 8) {
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

void MenuApp::handle_result_input(uint32_t keycode) {
    if (keycode == key_left_) {
        if (!last_chart_path_.empty()) {
            launch_gameplay(last_chart_path_);
        } else {
            launch_selected_song();
        }
        return;
    }
    if (keycode == key_f1_) {
        if (launch_last_result_replay()) {
            return;
        }
    }
    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::SongSelect;
        publish_snapshot();
    }
}


#include "MenuAppTail.inl"
