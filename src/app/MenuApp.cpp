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
#endif

#include "app/GameSession.h"
#include "app/GraphicsTiming.h"
#include "app/MemoryDiagnostics.h"
#include "app/RuntimeConfigMigration.h"
#include "chart/BmsParser.h"
#include "chart/OsuManiaLoader.h"
#include "config/SimpleJson.h"
#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

constexpr int kSnapshotSongCount = 10;
constexpr int kSongSelectVisibleCardCount = 5;
constexpr int kRefreshHzStep = 10;
constexpr double kVisualOffsetMin = -500.0;
constexpr double kVisualOffsetMax = 500.0;
constexpr double kVisualOffsetStep = 5.0;
constexpr double kJudgementLinePositionStep = 0.01;
constexpr double kNoteSizeScaleStep = 0.05;
constexpr double kVolumeMin = 0.0;
constexpr double kVolumeMax = 1.0;
constexpr double kVolumeStep = 0.05;
constexpr double kChartMixVolumeMin = 0.0;
constexpr double kChartMixVolumeMax = 2.0;
constexpr double kChartMixVolumeStep = 0.05;
constexpr double kRateMin = 0.5;
constexpr double kRateMax = 2.0;
constexpr double kRateStep = 0.05;
constexpr double kHiSpeedMin = 0.5;
constexpr double kHiSpeedMax = 50.0;
constexpr double kHiSpeedStep = 0.25;
constexpr int kSeedMin = 0;
constexpr int kSeedMax = 9999;
constexpr int64_t kKeymapCaptureTimeoutNs = 5'000'000'000LL;
constexpr int64_t kSongSelectRepeatInitialDelayNs = 250'000'000LL;
constexpr int64_t kSongSelectRepeatIntervalNs = 45'000'000LL;
constexpr std::size_t kRecentSongSourceLimit = 12;

const int kPollingOptions[] = {1000, 2000, 4000, 8000};

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

std::string on_off(bool value) {
    return value ? "On" : "Off";
}

int clamp_int(int value, int min_value, int max_value) {
    return std::max(min_value, std::min(max_value, value));
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

int next_option_index(const int* options, int count, int current, int direction) {
    int index = 0;
    for (int i = 0; i < count; ++i) {
        if (options[i] == current) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = count - 1;
    } else if (index >= count) {
        index = 0;
    }
    return index;
}

std::string preset_label(const std::string& preset) {
    if (preset == "high") {
        return "High";
    }
    return "Basic";
}

void apply_audio_preset(config::RuntimeConfig& config) {
    if (config.audio_ui.preset == "basic") {
        config.audio.frames_per_buffer = 256;
        config.audio.periods = 3;
    } else {
        config.audio.frames_per_buffer = 320;
        config.audio.periods = 3;
    }
}

double clamp_step_value(double value, double min_value, double max_value, double step) {
    if (!std::isfinite(value)) {
        return min_value;
    }
    const double clamped = std::clamp(value, min_value, max_value);
    const double snapped = std::round(clamped / step) * step;
    return std::clamp(snapped, min_value, max_value);
}

std::string format_percent(double value) {
    const int percent = static_cast<int>(std::lround(std::clamp(value, 0.0, 2.0) * 100.0));
    return std::to_string(percent) + "%";
}

std::string format_multiplier(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(2);
    stream << value << "x";
    return stream.str();
}

std::string format_decimal(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(2);
    stream << value;
    return stream.str();
}

std::string format_signed_offset_ms(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(1);
    if (value >= 0.0) {
        stream << '+';
    }
    stream << value << " ms";
    return stream.str();
}

std::string display_label(const std::string& mode) {
    if (mode == "fullscreen") {
        return "Fullscreen";
    }
    return "Borderless";
}

std::string normalize_resolution_preset(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "720p" || value == "1080p" || value == "qhd") {
        return value;
    }
    return "native";
}

std::string cycle_resolution_preset(std::string current, int direction) {
    static constexpr const char* kResolutionPresets[] = {"native", "720p", "1080p", "qhd"};
    const int option_count = static_cast<int>(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]));
    current = normalize_resolution_preset(std::move(current));
    int current_index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (current == kResolutionPresets[i]) {
            current_index = i;
            break;
        }
    }
    current_index += direction;
    if (current_index < 0) {
        current_index = option_count - 1;
    } else if (current_index >= option_count) {
        current_index = 0;
    }
    return kResolutionPresets[current_index];
}

std::pair<int, int> resolution_dimensions(std::string_view preset) {
    if (preset == "720p") {
        return {1280, 720};
    }
    if (preset == "1080p") {
        return {1920, 1080};
    }
    if (preset == "qhd") {
        return {2560, 1440};
    }
    return {0, 0};
}

std::string resolution_label(std::string_view preset) {
    if (preset == "720p") {
        return "1280x720";
    }
    if (preset == "1080p") {
        return "1920x1080";
    }
    if (preset == "qhd") {
        return "2560x1440";
    }
    return "Monitor Native";
}

std::string to_lower_ascii(std::string value);

std::string format_label(const std::string& value) {
    if (value == "bms") {
        return "BMS";
    }
    if (value == "osu") {
        return "OSU";
    }
    return "All";
}

std::string key_mode_label(const std::string& value) {
    if (value == "4k") {
        return "4K";
    }
    if (value == "5k") {
        return "5K";
    }
    if (value == "6k") {
        return "6K";
    }
    if (value == "7k") {
        return "7K";
    }
    if (value == "8k") {
        return "8K";
    }
    if (value == "9k") {
        return "9K";
    }
    if (value == "10k") {
        return "10K";
    }
    return "Auto";
}

std::string normalize_runtime_key_mode(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "4k" || value == "5k" || value == "6k" || value == "7k" || value == "8k" ||
        value == "9k" || value == "10k") {
        return value;
    }
    return "auto";
}

std::string cycle_runtime_key_mode(std::string_view current, int direction, bool allow_auto) {
    static constexpr const char* kAutoModes[] = {"auto", "4k", "5k", "6k", "7k", "8k", "9k", "10k"};
    static constexpr const char* kConcreteModes[] = {"4k", "5k", "6k", "7k", "8k", "9k", "10k"};
    const auto* options = allow_auto ? kAutoModes : kConcreteModes;
    const int option_count = allow_auto ? static_cast<int>(sizeof(kAutoModes) / sizeof(kAutoModes[0]))
                                        : static_cast<int>(sizeof(kConcreteModes) / sizeof(kConcreteModes[0]));
    std::string normalized = normalize_runtime_key_mode(std::string(current));
    if (!allow_auto && normalized == "auto") {
        normalized = "10k";
    }

    int index = 0;
    for (int i = 0; i < option_count; ++i) {
        if (normalized == options[i]) {
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
    return options[index];
}

std::string normalize_skin_edit_mode(std::string value) {
    value = config::normalize_skin_mode_token(value);
    if (value == "4k" || value == "5k" || value == "6k" || value == "7k" || value == "8k" ||
        value == "9k" || value == "10k") {
        return value;
    }
    return "10k";
}

std::string cycle_skin_edit_mode(std::string_view current, int direction) {
    static constexpr const char* kSkinModes[] = {"4k", "5k", "6k", "7k", "8k", "9k", "10k"};
    const int option_count = static_cast<int>(sizeof(kSkinModes) / sizeof(kSkinModes[0]));
    std::string normalized = normalize_skin_edit_mode(std::string(current));
    int index = option_count - 1;
    for (int i = 0; i < option_count; ++i) {
        if (normalized == kSkinModes[i]) {
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
    return kSkinModes[index];
}

int lane_count_for_skin_mode(std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    if (normalized == "5k") {
        return 5;
    }
    if (normalized == "6k") {
        return 6;
    }
    if (normalized == "7k") {
        return 7;
    }
    if (normalized == "8k") {
        return 8;
    }
    if (normalized == "9k") {
        return 9;
    }
    if (normalized == "4k") {
        return 4;
    }
    return 10;
}

std::string lane_display_label(int lane_index) {
    return "Lane " + std::to_string(std::max(1, lane_index + 1));
}

std::vector<std::string>& editable_skin_lane_colors(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& colors = skin.lane_colors[normalized];
    colors = config::resolved_skin_lane_colors(skin, normalized);
    return colors;
}

std::string gauge_label(const std::string& value) {
    if (value == "hard") {
        return "Hard";
    }
    if (value == "easy") {
        return "Easy";
    }
    return "Normal";
}

std::string random_label(const std::string& value) {
    if (value == "fr") {
        return "FR";
    }
    if (value == "sr") {
        return "SR";
    }
    return "Off";
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
        case game::Judgement::GD: return "GD";
        case game::Judgement::BD: return "BD";
        case game::Judgement::PR:
        default: return "PR";
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

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string to_upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('a') && ch <= static_cast<unsigned char>('z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string keysound_policy_label(std::string_view policy) {
    const std::string normalized = to_lower_ascii(std::string(policy));
    if (normalized == "autoplay") {
        return "Autoplay";
    }
    if (normalized == "ignore" || normalized == "off") {
        return "Off";
    }
    return "Follow";
}

std::string cycle_bms_keysound_policy(std::string_view current, int direction) {
    static constexpr const char* kPolicies[] = {"follow", "autoplay", "ignore"};
    int index = 0;
    const std::string normalized = to_lower_ascii(std::string(current));
    for (int i = 0; i < 3; ++i) {
        if (normalized == kPolicies[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = 2;
    } else if (index >= 3) {
        index = 0;
    }
    return kPolicies[index];
}

std::string normalize_chart_filter(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "bms" || value == "osu") {
        return value;
    }
    return "auto";
}

std::string cycle_chart_filter(std::string_view current, int direction) {
    static constexpr const char* kFilters[] = {"auto", "bms", "osu"};
    int index = 0;
    const std::string normalized = normalize_chart_filter(std::string(current));
    for (int i = 0; i < 3; ++i) {
        if (normalized == kFilters[i]) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = 2;
    } else if (index >= 3) {
        index = 0;
    }
    return kFilters[index];
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

int clear_status_priority(std::string_view clear_status, bool game_over, std::string_view final_gauge) {
    const std::string status = to_lower_ascii(std::string(clear_status));
    if (game_over || status.find("fail") != std::string::npos) {
        return 0;
    }
    const std::string gauge = to_lower_ascii(std::string(final_gauge));
    if (status.find("hard") != std::string::npos || gauge == "hard") {
        return 3;
    }
    if (status.find("easy") != std::string::npos || gauge == "easy") {
        return 1;
    }
    return 2;
}

std::string normalized_clear_status(std::string_view clear_status, bool game_over, std::string_view final_gauge) {
    const std::string status = safe_ui_text(clear_status);
    if (!status.empty()) {
        return status;
    }
    if (game_over) {
        return "FAILED";
    }
    const std::string gauge = to_lower_ascii(std::string(final_gauge));
    if (gauge == "hard") {
        return "HARD CLEAR";
    }
    if (gauge == "easy") {
        return "EASY CLEAR";
    }
    return "CLEAR";
}

std::string compact_timestamp_label(std::string_view created_utc) {
    std::string value(created_utc);
    if (value.size() >= 16 && value[8] == '_' && value.back() == 'Z') {
        return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2) + " " +
               value.substr(9, 2) + ":" + value.substr(11, 2) + ":" + value.substr(13, 2) + " UTC";
    }
    return safe_ui_text_or_placeholder(created_utc, "-");
}

std::string browser_summary_label(std::string_view query, int key_filter, int level_min, int level_max) {
    std::vector<std::string> parts;
    if (!query.empty()) {
        parts.push_back("Q " + safe_ui_text(query));
    }
    if (key_filter > 0) {
        parts.push_back(key_mode_label(std::to_string(key_filter) + "k"));
    }
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

bool is_bms_chart_extension(std::string_view ext) {
    return ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms";
}

bool is_supported_chart_extension(std::string_view ext) {
    return is_bms_chart_extension(ext) || ext == ".osu";
}

std::optional<std::string> normalize_dropped_song_source(const std::string& raw_path) {
    namespace fs = std::filesystem;
    if (raw_path.empty()) {
        return std::nullopt;
    }

    fs::path candidate = path_from_utf8(raw_path);
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(candidate, ec);
    if (!ec && !canonical.empty()) {
        candidate = canonical;
    } else {
        ec.clear();
        candidate = candidate.lexically_normal();
    }

    if (fs::is_directory(candidate, ec)) {
        return candidate.u8string();
    }
    ec.clear();

    if (fs::is_regular_file(candidate, ec) &&
        is_supported_chart_extension(to_lower_ascii(candidate.extension().u8string()))) {
        const fs::path parent = candidate.parent_path();
        if (!parent.empty()) {
            return parent.u8string();
        }
    }

    return std::nullopt;
}

std::string normalize_song_source_path(const std::string& raw_path) {
    namespace fs = std::filesystem;
    if (raw_path.empty()) {
        return {};
    }

    fs::path candidate = path_from_utf8(raw_path);
    std::error_code ec;
    if (!candidate.is_absolute()) {
        const fs::path absolute = fs::absolute(candidate, ec);
        if (!ec && !absolute.empty()) {
            candidate = absolute;
        } else {
            ec.clear();
        }
    }

    const fs::path canonical = fs::weakly_canonical(candidate, ec);
    if (!ec && !canonical.empty()) {
        candidate = canonical;
    } else {
        ec.clear();
        candidate = candidate.lexically_normal();
    }
    return candidate.u8string();
}

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string normalize_asset_reference(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = trim_copy(std::string_view(value).substr(1, value.size() - 2));
        }
    }
    return value;
}

bool is_preview_image_extension(std::string_view ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
           ext == ".gif" || ext == ".tif" || ext == ".tiff" || ext == ".webp";
}

std::string normalize_asset_lookup_key(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return to_lower_ascii(std::move(value));
}

std::filesystem::path normalize_resolved_preview_path(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path normalized = path;
    if (!normalized.is_absolute()) {
        const fs::path absolute = fs::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        } else {
            ec.clear();
        }
    }

    const fs::path canonical = fs::weakly_canonical(normalized, ec);
    if (!ec && !canonical.empty()) {
        return canonical;
    }
    return normalized.lexically_normal();
}

struct PreviewAssetLookupIndex {
    bool built = false;
    std::unordered_map<std::string, std::filesystem::path> by_relative;
    std::unordered_map<std::string, std::filesystem::path> by_filename;
};

void build_preview_asset_lookup(const std::filesystem::path& chart_path, PreviewAssetLookupIndex& lookup) {
    if (lookup.built) {
        return;
    }
    lookup.built = true;
    lookup.by_relative.clear();
    lookup.by_filename.clear();

    namespace fs = std::filesystem;
    const fs::path root = chart_path.parent_path();
    if (root.empty()) {
        return;
    }

    std::error_code ec;
    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root, options, ec);
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
        if (!is_preview_image_extension(ext)) {
            it.increment(ec);
            continue;
        }

        const fs::path full = normalize_resolved_preview_path(entry.path());
        fs::path relative = fs::relative(full, root, ec);
        if (ec || relative.empty()) {
            ec.clear();
            relative = full.lexically_relative(root);
        }
        if (!relative.empty()) {
            lookup.by_relative.emplace(normalize_asset_lookup_key(relative.generic_u8string()), full);
        }
        lookup.by_filename.emplace(normalize_asset_lookup_key(full.filename().u8string()), full);
        it.increment(ec);
    }
}

std::optional<std::filesystem::path> lookup_preview_asset_candidate(const std::filesystem::path& chart_path,
                                                                    const std::filesystem::path& ref_path,
                                                                    PreviewAssetLookupIndex& lookup) {
    namespace fs = std::filesystem;
    const fs::path direct = ref_path.is_absolute()
                                ? ref_path.lexically_normal()
                                : (chart_path.parent_path() / ref_path).lexically_normal();
    std::error_code ec;
    if (!direct.empty() && fs::exists(direct, ec) && !ec &&
        is_preview_image_extension(to_lower_ascii(direct.extension().u8string()))) {
        return normalize_resolved_preview_path(direct);
    }

    build_preview_asset_lookup(chart_path, lookup);

    const std::string relative_key = normalize_asset_lookup_key(ref_path.generic_u8string());
    auto relative_it = lookup.by_relative.find(relative_key);
    if (relative_it != lookup.by_relative.end()) {
        return relative_it->second;
    }

    const std::string file_key = normalize_asset_lookup_key(ref_path.filename().u8string());
    auto file_it = lookup.by_filename.find(file_key);
    if (file_it != lookup.by_filename.end()) {
        return file_it->second;
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> build_preview_reference_candidates(const std::string& reference) {
    namespace fs = std::filesystem;
    static constexpr std::string_view kPreviewExts[] = {
        ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff", ".webp",
    };

    const std::string normalized = normalize_asset_reference(reference);
#ifdef _WIN32
    fs::path ref_path = fs::u8path(normalized);
#else
    fs::path ref_path(normalized);
#endif

    std::vector<fs::path> candidates;
    std::unordered_set<std::string> seen;
    auto push_candidate = [&](const fs::path& candidate) {
        const std::string key = normalize_asset_lookup_key(candidate.generic_u8string());
        if (seen.emplace(key).second) {
            candidates.push_back(candidate);
        }
    };

    push_candidate(ref_path);

    const std::string ext = to_lower_ascii(ref_path.extension().u8string());
    if (ext.empty()) {
        for (std::string_view preview_ext : kPreviewExts) {
            fs::path candidate = ref_path;
            candidate += preview_ext;
            push_candidate(candidate);
        }
    } else if (is_preview_image_extension(ext)) {
        for (std::string_view preview_ext : kPreviewExts) {
            if (preview_ext == ext) {
                continue;
            }
            fs::path candidate = ref_path;
            candidate.replace_extension(preview_ext);
            push_candidate(candidate);
        }
    }

    return candidates;
}

std::optional<std::filesystem::path> resolve_preview_asset_path(const std::filesystem::path& chart_path,
                                                                const std::string& reference) {
    PreviewAssetLookupIndex lookup;
    for (const auto& candidate : build_preview_reference_candidates(reference)) {
        if (auto resolved = lookup_preview_asset_candidate(chart_path, candidate, lookup); resolved.has_value()) {
            return resolved;
        }
    }
    return std::nullopt;
}

std::vector<std::string> collect_bms_preview_references(const chart::BmsChart& chart) {
    std::vector<std::string> references;
    std::unordered_set<std::string> seen;
    auto append_reference = [&](std::string value) {
        value = normalize_asset_reference(std::move(value));
        if (value.empty()) {
            return;
        }
        const std::string key = normalize_asset_lookup_key(value);
        if (seen.emplace(key).second) {
            references.push_back(std::move(value));
        }
    };

    for (std::string_view header_key : {"STAGEFILE", "BACKBMP"}) {
        auto header_it = chart.headers.find(std::string(header_key));
        if (header_it != chart.headers.end()) {
            append_reference(header_it->second);
        }
    }

    auto append_bga_references = [&](std::string_view channel) {
        for (const auto& command : chart.commands) {
            if (command.channel != channel) {
                continue;
            }
            for (std::size_t i = 0; i + 1 < command.data.size(); i += 2) {
                std::string slot = command.data.substr(i, 2);
                if (slot == "00") {
                    continue;
                }
                slot = to_upper_ascii(std::move(slot));
                auto bmp_it = chart.bmp.find(slot);
                if (bmp_it == chart.bmp.end()) {
                    continue;
                }
                append_reference(bmp_it->second);
            }
        }
    };

    for (std::string_view channel : {"04", "07", "06"}) {
        append_bga_references(channel);
    }
    return references;
}

std::string song_source_display_name(const std::string& raw_path) {
    namespace fs = std::filesystem;
    if (raw_path.empty()) {
        return "Songs";
    }

    fs::path source_path = path_from_utf8(raw_path);
    try {
        source_path = source_path.lexically_normal();
        if (!source_path.filename().empty()) {
            return safe_ui_text(source_path.filename().u8string(), "Songs");
        }
        if (!source_path.root_name().empty()) {
            return safe_ui_text(source_path.root_name().u8string(), "Songs");
        }
        return safe_ui_text(source_path.u8string(), "Songs");
    } catch (...) {
        return "Songs";
    }
}

int judged_total(const gameplay::JudgementCounts& counts) {
    return counts.pg + counts.gr + counts.gd + counts.bd + counts.pr;
}

double calculate_accuracy(const gameplay::ResultStats& stats) {
    const int judged = judged_total(stats.counts);
    if (judged <= 0) {
        return 0.0;
    }
    const double weighted = static_cast<double>(stats.counts.pg) +
                            static_cast<double>(stats.counts.gr) * 0.80 +
                            static_cast<double>(stats.counts.gd) * 0.50 +
                            static_cast<double>(stats.counts.bd) * 0.20;
    return std::clamp(weighted / static_cast<double>(judged) * 100.0, 0.0, 100.0);
}

int64_t calculate_score(const gameplay::ResultStats& stats) {
    int64_t score = static_cast<int64_t>(stats.counts.pg) * 1000 +
                    static_cast<int64_t>(stats.counts.gr) * 700 +
                    static_cast<int64_t>(stats.counts.gd) * 300;
    score -= static_cast<int64_t>(stats.counts.bd) * 200;
    score -= static_cast<int64_t>(stats.counts.pr) * 500;
    return std::max<int64_t>(0, score);
}

bool infer_game_over(const gameplay::ResultStats& stats) {
    const int judged = judged_total(stats.counts);
    if (judged <= 0) {
        return true;
    }
    if (stats.total_notes > 0 && judged < stats.total_notes) {
        return true;
    }
    return false;
}

std::string calculate_rank(const gameplay::ResultStats& stats, bool game_over) {
    if (game_over) {
        return "F";
    }
    const int judged = judged_total(stats.counts);
    if (judged <= 0) {
        return "--";
    }
    const double accuracy = calculate_accuracy(stats);
    if (accuracy >= 99.0 && stats.counts.pr == 0 && stats.counts.bd == 0) {
        return "SS";
    }
    if (accuracy >= 95.0) {
        return "S";
    }
    if (accuracy >= 90.0) {
        return "A";
    }
    if (accuracy >= 80.0) {
        return "B";
    }
    if (accuracy >= 70.0) {
        return "C";
    }
    return "D";
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

const config::JsonValue* find_json_value(const config::JsonObject& root, std::string_view key) {
    auto it = root.find(std::string(key));
    if (it == root.end()) {
        return nullptr;
    }
    return &it->second;
}

const config::JsonObject* find_json_object(const config::JsonObject& root, std::string_view key) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return nullptr;
    }
    return value->as_object();
}

int read_json_int(const config::JsonObject& root, std::string_view key, int fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return static_cast<int>(std::llround(value->as_number(static_cast<double>(fallback))));
}

std::string read_json_string(const config::JsonObject& root, std::string_view key, std::string fallback = {}) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return safe_ui_text(value->as_string(std::move(fallback)));
}

bool read_json_bool(const config::JsonObject& root, std::string_view key, bool fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return value->as_bool(fallback);
}

double read_json_number(const config::JsonObject& root, std::string_view key, double fallback) {
    const auto* value = find_json_value(root, key);
    if (!value) {
        return fallback;
    }
    return value->as_number(fallback);
}

std::string normalize_path_key(const std::filesystem::path& raw_path) {
    if (raw_path.empty()) {
        return {};
    }
    try {
        std::error_code ec;
        std::filesystem::path normalized = raw_path;
        if (normalized.is_absolute()) {
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(normalized, ec);
            if (!ec && !canonical.empty()) {
                normalized = canonical;
            } else {
                ec.clear();
                normalized = normalized.lexically_normal();
            }
        } else {
            normalized = normalized.lexically_normal();
        }
        return to_lower_ascii(normalized.generic_u8string());
    } catch (...) {
        return {};
    }
}

void append_unique_key(std::vector<std::string>& keys, const std::string& key) {
    if (key.empty()) {
        return;
    }
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

bool relative_points_outside_root(const std::filesystem::path& relative) {
    if (relative.empty()) {
        return true;
    }
    const auto first = relative.begin();
    if (first == relative.end()) {
        return true;
    }
    return *first == "..";
}

std::vector<std::string> build_chart_path_keys(const std::string& chart_path, const std::string& songs_root) {
    namespace fs = std::filesystem;
    std::vector<std::string> keys;
    if (chart_path.empty()) {
        return keys;
    }

    const fs::path raw = path_from_utf8(chart_path);
    if (raw.empty()) {
        return keys;
    }
    append_unique_key(keys, normalize_path_key(raw));

    if (songs_root.empty()) {
        return keys;
    }

    try {
        std::error_code ec;
        fs::path root = path_from_utf8(songs_root);
        if (root.empty()) {
            return keys;
        }
        const fs::path root_canonical = fs::weakly_canonical(root, ec);
        if (!ec && !root_canonical.empty()) {
            root = root_canonical;
        } else {
            ec.clear();
            root = root.lexically_normal();
        }

        if (raw.is_absolute()) {
            fs::path absolute = raw;
            const fs::path absolute_canonical = fs::weakly_canonical(absolute, ec);
            if (!ec && !absolute_canonical.empty()) {
                absolute = absolute_canonical;
            } else {
                ec.clear();
                absolute = absolute.lexically_normal();
            }
            const fs::path relative = absolute.lexically_relative(root);
            if (!relative_points_outside_root(relative)) {
                append_unique_key(keys, normalize_path_key(relative));
            }
        } else {
            append_unique_key(keys, normalize_path_key(root / raw));
        }
    } catch (...) {
    }

    return keys;
}

struct ParsedResultRecord {
    std::string chart_path;
    std::string chart_format;
    std::string created_utc;
    std::string replay_path;
    std::string clear_status;
    std::string final_gauge;
    gameplay::ResultStats stats;
    bool game_over = false;
};

std::optional<ParsedResultRecord> parse_result_file(const std::filesystem::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "Failed to open result JSON.";
        }
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    auto parsed = config::parse_json(buffer.str());
    if (!parsed.success() || !parsed.root.has_value()) {
        if (error) {
            *error = parsed.error.empty() ? "Failed to parse result JSON." : parsed.error;
        }
        return std::nullopt;
    }

    const auto* root = parsed.root->as_object();
    if (!root) {
        if (error) {
            *error = "Result JSON root must be an object.";
        }
        return std::nullopt;
    }

    const auto* stats_obj = find_json_object(*root, "stats");
    if (!stats_obj) {
        if (error) {
            *error = "Result JSON missing stats object.";
        }
        return std::nullopt;
    }

    ParsedResultRecord out;
    out.chart_path = read_json_string(*root, "chart_path");
    out.chart_format = read_json_string(*root, "chart_format");
    out.created_utc = read_json_string(*root, "created_utc");
    out.replay_path = read_json_string(*root, "replay_path");
    out.clear_status = read_json_string(*root, "clear_status");
    out.final_gauge = read_json_string(*root, "final_gauge");
    if (out.chart_path.empty()) {
        if (error) {
            *error = "Result JSON missing chart_path.";
        }
        return std::nullopt;
    }

    const auto* counts_obj = find_json_object(*stats_obj, "counts");
    if (counts_obj) {
        out.stats.counts.pg = read_json_int(*counts_obj, "pg", 0);
        out.stats.counts.gr = read_json_int(*counts_obj, "gr", 0);
        out.stats.counts.gd = read_json_int(*counts_obj, "gd", 0);
        out.stats.counts.bd = read_json_int(*counts_obj, "bd", 0);
        out.stats.counts.pr = read_json_int(*counts_obj, "pr", 0);
    }
    out.stats.max_combo = read_json_int(*stats_obj, "max_combo", 0);
    out.stats.total_notes = read_json_int(*stats_obj, "total_notes", 0);
    out.stats.mean_delta_ms = read_json_number(*stats_obj, "mean_delta_ms", 0.0);
    const double stddev_delta_ms = read_json_number(*stats_obj, "stddev_delta_ms", 0.0);

    if (const auto* gauge_history = find_json_value(*stats_obj, "gauge_history")) {
        if (const auto* values = gauge_history->as_array()) {
            out.stats.gauge_history.reserve(values->size());
            for (const auto& value : *values) {
                const auto* item = value.as_object();
                if (!item) {
                    continue;
                }
                out.stats.gauge_history.push_back(gameplay::GaugeSample{
                    static_cast<int64_t>(std::llround(read_json_number(*item, "sample", 0.0))),
                    read_json_number(*item, "value", 0.0),
                });
            }
        }
    }
    if (const auto* shifts = find_json_value(*stats_obj, "shifts")) {
        if (const auto* values = shifts->as_array()) {
            out.stats.shifts.reserve(values->size());
            for (const auto& value : *values) {
                const auto* item = value.as_object();
                if (!item) {
                    continue;
                }
                out.stats.shifts.push_back(gameplay::ShiftEvent{
                    static_cast<int64_t>(std::llround(read_json_number(*item, "sample", 0.0))),
                    gauge_type_from_mode_string(read_json_string(*item, "from", "normal")),
                    gauge_type_from_mode_string(read_json_string(*item, "to", "normal")),
                });
            }
        }
    }

    const int judged = judged_total(out.stats.counts);
    if (out.stats.total_notes <= 0) {
        out.stats.total_notes = judged;
    }
    out.stats.delta_samples = judged;
    if (out.stats.delta_samples > 1) {
        out.stats.m2_delta_ms = stddev_delta_ms * stddev_delta_ms *
                                static_cast<double>(out.stats.delta_samples - 1);
    }
    out.game_over = read_json_bool(*root, "game_over", infer_game_over(out.stats));
    out.clear_status = normalized_clear_status(out.clear_status, out.game_over, out.final_gauge);
    return out;
}

struct ParsedReplayRecord {
    int sample_rate = 0;
    int lane_count = 0;
    int event_count = 0;
    int64_t duration_samples = 0;
    double rate = 1.0;
    double input_offset_ms = 0.0;
};

std::optional<ParsedReplayRecord> parse_replay_file(const std::filesystem::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) {
            *error = "Failed to open replay JSON.";
        }
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto parsed = config::parse_json(buffer.str());
    if (!parsed.success() || !parsed.root.has_value()) {
        if (error) {
            *error = parsed.error.empty() ? "Failed to parse replay JSON." : parsed.error;
        }
        return std::nullopt;
    }

    const auto* root = parsed.root->as_object();
    if (!root) {
        if (error) {
            *error = "Replay JSON root must be an object.";
        }
        return std::nullopt;
    }
    const auto* trace = find_json_object(*root, "trace");
    if (!trace) {
        if (error) {
            *error = "Replay JSON missing trace object.";
        }
        return std::nullopt;
    }

    ParsedReplayRecord out;
    out.sample_rate = read_json_int(*root, "sample_rate", read_json_int(*trace, "sample_rate", 0));
    out.rate = read_json_number(*root, "rate", read_json_number(*trace, "rate", 1.0));
    out.input_offset_ms = read_json_number(*root, "input_offset_ms", 0.0);
    out.lane_count = read_json_int(*trace, "lane_count", 0);
    out.duration_samples = static_cast<int64_t>(std::llround(read_json_number(*trace, "duration_samples", 0.0)));
    if (const auto* events = find_json_value(*trace, "events")) {
        if (const auto* values = events->as_array()) {
            out.event_count = static_cast<int>(values->size());
        }
    }
    return out;
}

bool is_better_record(int64_t candidate_score,
                      int candidate_clear_priority,
                      int candidate_combo,
                      int candidate_judged,
                      std::string_view candidate_created,
                      int64_t current_score,
                      int current_clear_priority,
                      int current_combo,
                      int current_judged,
                      std::string_view current_created) {
    if (candidate_clear_priority != current_clear_priority) {
        return candidate_clear_priority > current_clear_priority;
    }
    if (candidate_score != current_score) {
        return candidate_score > current_score;
    }
    if (candidate_combo != current_combo) {
        return candidate_combo > current_combo;
    }
    if (candidate_judged != current_judged) {
        return candidate_judged > current_judged;
    }
    return candidate_created > current_created;
}

}  // namespace

MenuApp::MenuApp() = default;

MenuApp::~MenuApp() {
    shutdown();
}

bool MenuApp::initialize(const CommandLineOptions& options) {
    exit_code_ = 0;
    options_ = options;
    songs_path_ = normalize_song_source_path(options.songs_path);
    if (songs_path_.empty()) {
        songs_path_ = options.songs_path;
    }

    const std::filesystem::path profile_dir = path_from_utf8("profiles") / path_from_utf8(options.profile);
    profile_dir_ = profile_dir.u8string();

    const std::filesystem::path songs_dir = path_from_utf8(songs_path_);
    cache_path_ = (songs_dir / ".tenriff" / "song_index.json").u8string();

    config::ConfigLoader config_loader;
    auto config_result = config_loader.load_profile(profile_dir_);
    if (!config_result.success()) {
        return false;
    }
    config_ = config_result.config;
    const bool migrated_config = migrate_bms_first_runtime_config(config_);

    if (config_result.used_defaults || migrated_config) {
        config_loader.save_profile(profile_dir_, config_);
    }
    config_.graphics.refresh_hz =
        clamp_int(config_.graphics.refresh_hz, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
    config_.graphics.resolution = normalize_resolution_preset(config_.graphics.resolution);

    config::KeymapManager keymap_manager;
    auto keymap_result = keymap_manager.load_profile(profile_dir_);
    if (!keymap_result.success()) {
        return false;
    }
    keymap_ = keymap_result.keymap;

    if (keymap_result.used_defaults) {
        keymap_manager.save_profile(profile_dir_, keymap_);
    }

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
            initial_song_source = normalize_song_source_path(config_.ui.active_song_source);
        }
    } else {
        for (const auto& recent_source : config_.ui.recent_song_sources) {
            if (recent_source.empty()) {
                continue;
            }
            std::error_code ec;
            const auto recent_path = path_from_utf8(recent_source);
            if (std::filesystem::is_directory(recent_path, ec)) {
                initial_song_source = normalize_song_source_path(recent_source);
                break;
            }
        }
    }

    switch_song_source(initial_song_source, false);

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
    input_config.raw_input.no_legacy = true;
    input_config.polling_hz = config_.input.polling_hz;

    if (!input_thread_.initialize(input_config)) {
        std::cerr << "[error] Failed to initialize input thread." << std::endl;
    } else {
        (void)input_thread_.start();
    }

    auto audio_result = audio_thread_.initialize(config_.audio, [](float* output, uint32_t frames, int64_t) {
        if (output && frames > 0) {
            std::fill(output, output + frames * 2, 0.0f);
        }
    });
    if (audio_result == audio::AudioResult::Success) {
        (void)audio_thread_.start();
    } else {
        std::cerr << "[error] Failed to initialize audio thread." << std::endl;
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
    audio_thread_.stop();
    input_thread_.stop();
}

void MenuApp::restart_input_thread() {
    input_thread_.shutdown();
    input::InputThreadConfig input_config;
    input_config.backend = config_.input.rawinput ? input::InputBackend::RawInput
                                                  : input::InputBackend::Polling;
    input_config.raw_input.register_keyboard = config_.input.rawinput;
    input_config.raw_input.input_sink = true;
    input_config.raw_input.no_legacy = true;
    input_config.polling_hz = config_.input.polling_hz;
    if (!input_thread_.initialize(input_config)) {
        std::cerr << "[error] Failed to reinitialize input thread." << std::endl;
        return;
    }
    (void)input_thread_.start();
}

void MenuApp::restart_audio_thread() {
    audio_thread_.shutdown();
    auto audio_result = audio_thread_.initialize(config_.audio, [](float* output, uint32_t frames, int64_t) {
        if (output && frames > 0) {
            std::fill(output, output + frames * 2, 0.0f);
        }
    });
    if (audio_result == audio::AudioResult::Success) {
        (void)audio_thread_.start();
    } else {
        std::cerr << "[error] Failed to reinitialize audio thread." << std::endl;
    }
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
    const std::string normalized = normalize_song_source_path(source_path);
    if (normalized.empty()) {
        return false;
    }

    const std::string normalized_key = normalize_path_key(path_from_utf8(normalized));
    std::vector<std::string> updated_sources;
    updated_sources.reserve(std::min<std::size_t>(config_.ui.recent_song_sources.size() + 1, kRecentSongSourceLimit));
    updated_sources.push_back(normalized);

    for (const auto& existing : config_.ui.recent_song_sources) {
        const std::string normalized_existing = normalize_song_source_path(existing);
        if (normalized_existing.empty()) {
            continue;
        }
        if (normalize_path_key(path_from_utf8(normalized_existing)) == normalized_key) {
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
    if (!loader.save_profile(profile_dir_, config_, &error)) {
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

    const std::string normalized_source = normalize_song_source_path(new_songs_path);
    songs_path_ = normalized_source.empty() ? new_songs_path : normalized_source;
    song_select_view_ = SongSelectView::Songs;
    const bool source_history_changed = remember_song_source(songs_path_);
    cache_path_ = (path_from_utf8(songs_path_) / ".tenriff" / "song_index.json").u8string();
    last_indexer_snapshot_ns_ = 0;
    song_indexer_.stop();
    const SongIndexOptions index_options{config_.mode.enable_osu_charts};

    log_memory_phase("MenuApp",
                     "cache-load-before",
                     query_process_memory_snapshot(),
                     "source=" + safe_ui_text_or_placeholder(songs_path_, "<invalid path>"));
    auto cache_result = load_song_index(cache_path_, index_options);
    if (cache_result.success() && cache_result.loaded_from_file) {
        const int cached_count = static_cast<int>(cache_result.index.entries.size());
        update_song_list(std::move(cache_result.index));
        source_song_counts_[normalize_path_key(path_from_utf8(songs_path_))] = cached_count;
    } else {
        update_song_list(SongIndex{});
        source_song_counts_[normalize_path_key(path_from_utf8(songs_path_))] = 0;
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

    switch (screen_) {
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
        }
        return;
    }
    if (event.kind == render::MenuHitTargetKind::FileDrop) {
        auto dropped_source = normalize_dropped_song_source(event.path);
        if (!dropped_source.has_value()) {
            std::cerr << "[warn] Ignored dropped path (expected a folder or supported chart file): " << event.path
                      << std::endl;
            return;
        }
        switch_song_source(dropped_source.value(), false);
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
            title_cursor_ = clamp_int(event.index, 0, 3);
            handle_title_input(key_enter_);
            return;
        case render::MenuHitTargetKind::OptionsItem:
            if (screen_ != Screen::OptionsHub) {
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
            song_select_nav_cursor_ = clamp_int(event.index, 0, 5);
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

    const uint32_t action_key = (event.part == render::MenuHitPart::Increment)
                                    ? key_right_
                                    : (event.part == render::MenuHitPart::Decrement ? key_left_ : key_enter_);

    switch (screen_) {
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
            settings_cursor_ = clamp_int(event.index, 0, 6);
            handle_skins_settings_input(action_key);
            return;
        case Screen::SettingsInput:
            settings_cursor_ = clamp_int(event.index, 0, 1);
            handle_input_settings_input(action_key);
            return;
        case Screen::ModeSelect:
            settings_cursor_ = clamp_int(event.index, 0, 8);
            handle_mode_settings_input(action_key);
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

void MenuApp::handle_title_input(uint32_t keycode) {
    if (keycode == key_f5_) {
        refresh_song_source(true);
        publish_snapshot();
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

void MenuApp::handle_options_hub_input(uint32_t keycode) {
    constexpr int item_count = 7;
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
                submenu_return_screen_ = Screen::Title;
                screen_ = Screen::SettingsAudio;
                settings_cursor_ = 0;
                break;
            case 1:
                submenu_return_screen_ = Screen::Title;
                screen_ = Screen::SettingsGraphics;
                settings_cursor_ = 0;
                break;
            case 2:
                submenu_return_screen_ = Screen::Title;
                screen_ = Screen::SettingsSkins;
                settings_cursor_ = 0;
                skin_dirty_ = false;
                skin_edit_mode_ = normalize_skin_edit_mode(config_.mode.key_mode);
                skin_edit_lane_ = 0;
                break;
            case 3:
                submenu_return_screen_ = Screen::Title;
                screen_ = Screen::SettingsInput;
                settings_cursor_ = 0;
                break;
            case 4:
                submenu_return_screen_ = Screen::Title;
                screen_ = Screen::ModeSelect;
                settings_cursor_ = 0;
                break;
            case 5:
                submenu_return_screen_ = Screen::Title;
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
                break;
            default:
                screen_ = Screen::Title;
                break;
        }
        publish_snapshot();
        return;
    }

    if (keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::Title;
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
            song_select_nav_cursor_ = clamp_int(song_select_nav_cursor_ - 1, 0, 5);
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
            song_select_nav_cursor_ = clamp_int(song_select_nav_cursor_ + 1, 0, 5);
            publish_snapshot();
            return;
        }
        if (move_song_select_selection(1)) {
            publish_snapshot();
        }
        return;
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
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::SongBrowser;
                    settings_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 4:
                    submenu_return_screen_ = Screen::SongSelect;
                    screen_ = Screen::ModeSelect;
                    settings_cursor_ = 0;
                    publish_snapshot();
                    return;
                case 5:
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
        static constexpr int kKeyFilters[] = {0, 4, 5, 6, 7, 8, 9, 10};
        const int direction = (keycode == key_left_) ? -1 : 1;
        int index = 0;
        for (int i = 0; i < static_cast<int>(sizeof(kKeyFilters) / sizeof(kKeyFilters[0])); ++i) {
            if (song_key_filter_ == kKeyFilters[i]) {
                index = i;
                break;
            }
        }
        index += direction;
        if (index < 0) {
            index = static_cast<int>(sizeof(kKeyFilters) / sizeof(kKeyFilters[0])) - 1;
        } else if (index >= static_cast<int>(sizeof(kKeyFilters) / sizeof(kKeyFilters[0]))) {
            index = 0;
        }
        song_key_filter_ = kKeyFilters[index];
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

void MenuApp::handle_audio_settings_input(uint32_t keycode) {
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

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        config_.audio_ui.preset = (config_.audio_ui.preset == "basic") ? "high" : "basic";
        apply_audio_preset(config_);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.audio_ui.bms_keysound_policy =
            cycle_bms_keysound_policy(config_.audio_ui.bms_keysound_policy, direction);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.master_volume = clamp_step_value(config_.audio_ui.master_volume + direction * kVolumeStep,
                                                          kVolumeMin, kVolumeMax, kVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.bgm_volume = clamp_step_value(config_.audio_ui.bgm_volume + direction * kChartMixVolumeStep,
                                                       kChartMixVolumeMin, kChartMixVolumeMax, kChartMixVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.audio_ui.keysound_volume =
            clamp_step_value(config_.audio_ui.keysound_volume + direction * kChartMixVolumeStep,
                             kChartMixVolumeMin, kChartMixVolumeMax, kChartMixVolumeStep);
        audio_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (audio_dirty_) {
            config::ConfigLoader loader;
            std::string error;
            if (!loader.save_profile(profile_dir_, config_, &error)) {
                std::cerr << "[error] " << error << std::endl;
            }
            restart_audio_thread();
            audio_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_graphics_settings_input(uint32_t keycode) {
    const int item_count = 7;
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

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        config_.graphics.display_mode = (config_.graphics.display_mode == "borderless") ? "fullscreen" : "borderless";
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.graphics.resolution = cycle_resolution_preset(config_.graphics.resolution, direction);
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        int next_value = config_.graphics.refresh_hz + direction * kRefreshHzStep;
        next_value = clamp_int(next_value, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
        config_.graphics.refresh_hz = next_value;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        config_.graphics.vsync = !config_.graphics.vsync;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        config_.graphics.performance_overlay = !config_.graphics.performance_overlay;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 5 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.visual_offset_ms = clamp_step_value(
            config_.visual_offset_ms + static_cast<double>(direction) * kVisualOffsetStep,
            kVisualOffsetMin, kVisualOffsetMax, kVisualOffsetStep);
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (graphics_dirty_) {
            config::ConfigLoader loader;
            std::string error;
            if (!loader.save_profile(profile_dir_, config_, &error)) {
                std::cerr << "[error] " << error << std::endl;
            }
            apply_runtime_graphics_config();
            graphics_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_skins_settings_input(uint32_t keycode) {
    const int item_count = 7;
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

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        skin_edit_mode_ = cycle_skin_edit_mode(skin_edit_mode_, direction);
        skin_edit_lane_ = clamp_int(skin_edit_lane_, 0, lane_count_for_skin_mode(skin_edit_mode_) - 1);
        editable_skin_lane_colors(config_.skin, skin_edit_mode_);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int lane_count = lane_count_for_skin_mode(skin_edit_mode_);
        int next_lane = skin_edit_lane_ + direction;
        if (next_lane < 0) {
            next_lane = lane_count - 1;
        } else if (next_lane >= lane_count) {
            next_lane = 0;
        }
        skin_edit_lane_ = next_lane;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        auto& lane_colors = editable_skin_lane_colors(config_.skin, skin_edit_mode_);
        const auto palette = config::supported_skin_color_tokens();
        const std::string current = config::normalize_skin_color_token(
            lane_colors[static_cast<std::size_t>(skin_edit_lane_)]);
        int index = 0;
        for (int i = 0; i < static_cast<int>(palette.size()); ++i) {
            if (palette[static_cast<std::size_t>(i)] == current) {
                index = i;
                break;
            }
        }
        index += direction;
        if (index < 0) {
            index = static_cast<int>(palette.size()) - 1;
        } else if (index >= static_cast<int>(palette.size())) {
            index = 0;
        }
        lane_colors[static_cast<std::size_t>(skin_edit_lane_)] = palette[static_cast<std::size_t>(index)];
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.judgement_line_position = clamp_step_value(
            config_.skin.judgement_line_position + static_cast<double>(direction) * kJudgementLinePositionStep,
            config::kJudgementLinePositionMin, config::kJudgementLinePositionMax, kJudgementLinePositionStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.note_width_scale = clamp_step_value(
            config_.skin.note_width_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kNoteWidthScaleMin, config::kNoteWidthScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 5 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.skin.note_height_scale = clamp_step_value(
            config_.skin.note_height_scale + static_cast<double>(direction) * kNoteSizeScaleStep,
            config::kNoteHeightScaleMin, config::kNoteHeightScaleMax, kNoteSizeScaleStep);
        skin_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (skin_dirty_) {
            config::ConfigLoader loader;
            std::string error;
            if (!loader.save_profile(profile_dir_, config_, &error)) {
                std::cerr << "[error] " << error << std::endl;
            }
            skin_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_input_settings_input(uint32_t keycode) {
    const int item_count = 2;
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

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int option_count = static_cast<int>(sizeof(kPollingOptions) / sizeof(kPollingOptions[0]));
        const int next_index = next_option_index(kPollingOptions, option_count, config_.input.polling_hz, direction);
        config_.input.polling_hz = kPollingOptions[next_index];
        input_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (input_dirty_) {
            config::ConfigLoader loader;
            std::string error;
            if (!loader.save_profile(profile_dir_, config_, &error)) {
                std::cerr << "[error] " << error << std::endl;
            }
            restart_input_thread();
            input_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_mode_settings_input(uint32_t keycode) {
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

    if (keycode == key_left_ || keycode == key_right_) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        if (settings_cursor_ == 0) {
            config_.mode.enable_osu_charts = !config_.mode.enable_osu_charts;
            if (config_.mode.enable_osu_charts) {
                if (to_lower_ascii(config_.mode.format) == "bms") {
                    config_.mode.format = "auto";
                }
            } else {
                config_.mode.format = "bms";
                config_.mode.key_mode = "10k";
            }
            mode_dirty_ = true;
            mode_library_dirty_ = true;
            rebuild_visible_song_list();
        } else if (settings_cursor_ == 1) {
            if (!config_.mode.enable_osu_charts) {
                config_.mode.format = "bms";
            } else {
                config_.mode.format = cycle_chart_filter(config_.mode.format, direction);
            }
            mode_dirty_ = true;
            rebuild_visible_song_list();
        } else if (settings_cursor_ == 2) {
            if (!config_.mode.enable_osu_charts) {
                config_.mode.key_mode = "10k";
            } else {
                config_.mode.key_mode = cycle_runtime_key_mode(config_.mode.key_mode, direction, true);
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 3) {
            if (config_.mode.gauge == "normal") {
                config_.mode.gauge = (direction > 0) ? "hard" : "easy";
            } else if (config_.mode.gauge == "hard") {
                config_.mode.gauge = (direction > 0) ? "easy" : "normal";
            } else {
                config_.mode.gauge = (direction > 0) ? "normal" : "hard";
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 4) {
            if (config_.mode.random == "off") {
                config_.mode.random = (direction > 0) ? "fr" : "sr";
            } else if (config_.mode.random == "fr") {
                config_.mode.random = (direction > 0) ? "sr" : "off";
            } else {
                config_.mode.random = (direction > 0) ? "off" : "fr";
            }
            mode_dirty_ = true;
        } else if (settings_cursor_ == 5) {
            int next_value = static_cast<int>(config_.mode.random_seed) + direction;
            next_value = clamp_int(next_value, kSeedMin, kSeedMax);
            config_.mode.random_seed = static_cast<uint32_t>(next_value);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 6) {
            config_.speed.rate = clamp_step_value(config_.speed.rate + static_cast<double>(direction) * kRateStep,
                                                  kRateMin, kRateMax, kRateStep);
            mode_dirty_ = true;
        } else if (settings_cursor_ == 7) {
            config_.speed.hi_speed = clamp_step_value(
                config_.speed.hi_speed + static_cast<double>(direction) * kHiSpeedStep,
                kHiSpeedMin, kHiSpeedMax, kHiSpeedStep);
            mode_dirty_ = true;
        }
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (mode_dirty_) {
            config::ConfigLoader loader;
            std::string error;
            if (!loader.save_profile(profile_dir_, config_, &error)) {
                std::cerr << "[error] " << error << std::endl;
            }
            if (mode_library_dirty_) {
                refresh_song_source(true);
            }
            mode_dirty_ = false;
            mode_library_dirty_ = false;
        }
        publish_snapshot();
    }
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
    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::SongSelect;
        publish_snapshot();
    }
}

void MenuApp::populate_gameplay_render_data(render::GameplayHudData& target, uint64_t* out_revision) {
    std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);

    target.revision = gameplay_hud_.revision;
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
    target.snapshot_time_ns = gameplay_hud_.snapshot_time_ns;
    target.lookahead_samples = gameplay_hud_.lookahead_samples;
    target.past_samples = gameplay_hud_.past_samples;
    target.judgement_line_position = std::clamp(
        config_.skin.judgement_line_position,
        config::kJudgementLinePositionMin,
        config::kJudgementLinePositionMax);
    target.note_width_scale = std::clamp(
        config_.skin.note_width_scale,
        config::kNoteWidthScaleMin,
        config::kNoteWidthScaleMax);
    target.note_height_scale = std::clamp(
        config_.skin.note_height_scale,
        config::kNoteHeightScaleMin,
        config::kNoteHeightScaleMax);
    target.visual_offset_ms = std::clamp(config_.visual_offset_ms, kVisualOffsetMin, kVisualOffsetMax);
    target.rate = gameplay_hud_.rate;
    target.hispeed = gameplay_hud_.hispeed;
    target.combo = gameplay_hud_.combo;
    target.max_combo = gameplay_hud_.max_combo;
    target.pg = gameplay_hud_.counts.pg;
    target.gr = gameplay_hud_.counts.gr;
    target.gd = gameplay_hud_.counts.gd;
    target.bd = gameplay_hud_.counts.bd;
    target.pr = gameplay_hud_.counts.pr;
    target.total_notes = gameplay_hud_.counts.pg + gameplay_hud_.counts.gr +
                         gameplay_hud_.counts.gd + gameplay_hud_.counts.bd +
                         gameplay_hud_.counts.pr;
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
    const std::string skin_mode = std::to_string(target.lane_count) + "k";
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

    int64_t score = static_cast<int64_t>(gameplay_hud_.counts.pg) * 1000 +
                    static_cast<int64_t>(gameplay_hud_.counts.gr) * 700 +
                    static_cast<int64_t>(gameplay_hud_.counts.gd) * 300;
    score -= static_cast<int64_t>(gameplay_hud_.counts.bd) * 200;
    score -= static_cast<int64_t>(gameplay_hud_.counts.pr) * 500;
    target.score = std::max<int64_t>(0, score);

    const int judged_total = gameplay_hud_.counts.pg + gameplay_hud_.counts.gr + gameplay_hud_.counts.gd +
                             gameplay_hud_.counts.bd + gameplay_hud_.counts.pr;
    if (judged_total > 0) {
        const double weighted = gameplay_hud_.counts.pg * 1.0 +
                                gameplay_hud_.counts.gr * 0.80 +
                                gameplay_hud_.counts.gd * 0.50 +
                                gameplay_hud_.counts.bd * 0.20;
        target.accuracy = std::clamp(weighted / static_cast<double>(judged_total) * 100.0, 0.0, 100.0);
    } else {
        target.accuracy = 0.0;
    }

    if (out_revision) {
        *out_revision = gameplay_hud_.revision;
    }
}

void MenuApp::update_gameplay_loading_state(int percent, std::string_view stage) {
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        gameplay_hud_.loading = true;
        gameplay_hud_.loading_percent = clamp_int(percent, 0, 100);
        gameplay_hud_.loading_stage = std::string(stage);
        ++gameplay_hud_.revision;
    }
    publish_snapshot();
}

void MenuApp::publish_snapshot() {
    if (screen_ == Screen::SongSelect) {
        sync_song_select_state();
    }

    MenuSnapshot snapshot;
    render::MenuRenderData render;
    render.screen_title = screen_title();
    render.performance.visible = config_.graphics.performance_overlay;

    auto current_track = [&]() -> std::string {
        if (screen_ == Screen::SongSelect && song_select_view_ == SongSelectView::Sources &&
            !config_.ui.recent_song_sources.empty() &&
            selected_source_ >= 0 &&
            selected_source_ < static_cast<int>(config_.ui.recent_song_sources.size())) {
            return song_source_display_name(
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
    };
    const BestResultRecord current_best = current_song_best_result();
    rebuild_current_song_record_indices();
    const LocalPlayRecord* selected_record = current_selected_record();

    if (screen_ == Screen::Title) {
        render.kind = render::MenuScreenKind::TitleMenu;
        render.title.profile = options_.profile;
        render.title.track = current_track();
        render.title.high_score = current_best.has_value ? current_best.best_score : 0;
        render.title.buttons = {
            render::MenuButtonData{"PLAY", u8"▶", title_cursor_ == 0},
            render::MenuButtonData{"EDIT", u8"✎", title_cursor_ == 1},
            render::MenuButtonData{"OPTIONS", u8"⚙", title_cursor_ == 2},
            render::MenuButtonData{"EXIT", u8"⏻", title_cursor_ == 3},
        };
    } else if (screen_ == Screen::SongSelect) {
        render.kind = render::MenuScreenKind::SongSelect;
        render.song_select.profile = options_.profile;
        render.song_select.track = current_track();
        render.song_select.song_count = static_cast<int>(visible_song_count());
        render.song_select.source_count = static_cast<int>(config_.ui.recent_song_sources.size());
        render.song_select.record_count = static_cast<int>(current_song_record_indices_.size());
        render.song_select.showing_sources = (song_select_view_ == SongSelectView::Sources);
        render.song_select.showing_records = (song_select_view_ == SongSelectView::Records);
        render.song_select.high_score =
            render.song_select.showing_sources ? 0 :
            (render.song_select.showing_records && selected_record ? selected_record->score :
             (current_best.has_value ? current_best.best_score : 0));
        render.song_select.current_source_name = safe_ui_text(song_source_display_name(songs_path_), "Songs");
        render.song_select.current_source_path = safe_ui_text_or_placeholder(songs_path_, "<invalid path>");
        render.song_select.browser_summary = browser_summary_label(song_search_query_,
                                                                   song_key_filter_,
                                                                   song_level_min_filter_,
                                                                   song_level_max_filter_);
        render.song_select.sort_summary = song_sort_detail_label(song_sort_mode_);

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
            render::MenuButtonData{"BROWSE", "F", song_select_nav_cursor_ == 3, browser_detail},
            render::MenuButtonData{"MOD", "M", song_select_nav_cursor_ == 4,
                                    format_multiplier(config_.speed.rate) + " / HS " + format_decimal(config_.speed.hi_speed)},
            render::MenuButtonData{"RECORDS", "R", song_select_nav_cursor_ == 5,
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

                for (int i = start; i < end; ++i) {
                    const std::string& source_path = config_.ui.recent_song_sources[static_cast<std::size_t>(i)];
                    render::SongCardData card;
                    card.title = safe_ui_text(song_source_display_name(source_path), "<invalid title>");
                    card.artist = safe_ui_text_or_placeholder(source_path, "<invalid artist>");
                    const auto count_it = source_song_counts_.find(normalize_path_key(path_from_utf8(source_path)));
                    card.level = (count_it != source_song_counts_.end()) ? count_it->second : 0;
                    card.song_index = i;
                    card.selected = (i == selected_source_);
                    card.detail = safe_ui_text((normalize_path_key(path_from_utf8(source_path)) ==
                                                normalize_path_key(path_from_utf8(songs_path_)))
                                                   ? "CURRENT SOURCE"
                                                   : "RECENT SOURCE");
                    render.song_select.songs.push_back(std::move(card));
                }

                const std::string& selected_source =
                    config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)];
                render.song_select.selected_source_name =
                    safe_ui_text(song_source_display_name(selected_source), "Songs");
                render.song_select.selected_source_path =
                    safe_ui_text_or_placeholder(selected_source, "<invalid path>");
                const auto count_it =
                    source_song_counts_.find(normalize_path_key(path_from_utf8(selected_source)));
                render.song_select.selected_source_song_count =
                    (count_it != source_song_counts_.end()) ? count_it->second : -1;
                    render.song_select.selected_source_active =
                        normalize_path_key(path_from_utf8(selected_source)) ==
                        normalize_path_key(path_from_utf8(songs_path_));
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

                for (int i = start; i < end; ++i) {
                    const LocalPlayRecord& record =
                        local_play_records_[current_song_record_indices_[static_cast<std::size_t>(i)]];
                    render::SongCardData card;
                    card.title = compact_timestamp_label(record.created_utc);
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
        } else {
            const int total = static_cast<int>(visible_song_count());
            if (total > 0) {
                constexpr int visible = kSongSelectVisibleCardCount;
                int start = std::max(0, selected_song_ - (visible / 2));
                const int max_start = std::max(0, total - visible);
                start = std::min(start, max_start);
                const int end = std::min(total, start + visible);

                for (int i = start; i < end; ++i) {
                    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(i));
                    if (!entry) {
                        continue;
                    }
                    render::SongCardData card;
                    card.title = song_title_for_ui(*entry);
                    card.artist = song_artist_for_ui(*entry);
                    card.detail = safe_ui_text_or_placeholder(
                        key_mode_label(std::to_string(std::max(1, entry->key_count)) + "k") + " " +
                            format_label(to_lower_ascii(entry->format)),
                        "<invalid detail>");
                    card.level = entry->level;
                    card.rating = entry->rating;
                    card.song_index = i;
                    card.selected = (i == selected_song_);
                    render.song_select.songs.push_back(std::move(card));
                }
            }
        }

        if (!render.song_select.showing_sources) {
            if (const SongEntry* entry = (selected_song_ >= 0)
                                             ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                             : nullptr) {
                render.song_select.selected_song_title = song_title_for_ui(*entry);
                render.song_select.selected_song_artist = song_artist_for_ui(*entry);
                render.song_select.selected_song_detail = safe_ui_text_or_placeholder(
                    key_mode_label(std::to_string(std::max(1, entry->key_count)) + "k") + " " +
                        format_label(to_lower_ascii(entry->format)),
                    "-");
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
                render.song_select.selected_record_created_utc = compact_timestamp_label(selected_record->created_utc);
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
    } else if (screen_ == Screen::SongBrowser) {
        render.kind = render::MenuScreenKind::GenericList;
        auto add_row = [&](std::string label,
                           std::string value,
                           bool selected,
                           render::MenuHitTargetKind kind,
                           int index,
                           bool activatable,
                           bool adjustable) {
            render::MenuRowData row;
            row.label = std::move(label);
            row.value = std::move(value);
            row.selected = selected;
            row.target_kind = kind;
            row.row_index = index;
            row.activatable = activatable;
            row.adjustable = adjustable;
            row.decrement_enabled = adjustable;
            row.increment_enabled = adjustable;
            render.generic.rows.push_back(std::move(row));
        };

        add_row("Search",
                song_search_query_.empty() ? std::string("<type to search>") : song_search_query_,
                settings_cursor_ == 0, render::MenuHitTargetKind::SettingsRow, 0, true, false);
        add_row("Key Filter", key_filter_label(song_key_filter_), settings_cursor_ == 1,
                render::MenuHitTargetKind::SettingsRow, 1, false, true);
        add_row("Difficulty Min", song_level_min_filter_ > 0 ? std::to_string(song_level_min_filter_) : "Any",
                settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
        add_row("Difficulty Max", song_level_max_filter_ > 0 ? std::to_string(song_level_max_filter_) : "Any",
                settings_cursor_ == 3, render::MenuHitTargetKind::SettingsRow, 3, false, true);
        add_row("Clear Filters", "", settings_cursor_ == 4, render::MenuHitTargetKind::SettingsRow, 4, true, false);
        add_row("Back", "", settings_cursor_ == 5, render::MenuHitTargetKind::SettingsRow, 5, true, false);

        render.generic.notes.push_back("Search matches title, artist, and chart path.");
        render.generic.notes.push_back("Use letters/numbers/space on the Search row. Backspace deletes, Delete clears.");
        render.generic.notes.push_back("Key Filter supports All plus 4K through 10K.");
        render.generic.notes.push_back("Difficulty filters apply to indexed LV values only.");
        render.generic.notes.push_back("Sort order stays available from Song Select: LEVEL toggles ASC/DESC, TITLE toggles A-Z/Z-A.");
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
        render.kind = render::MenuScreenKind::ResultScreen;
        render.result.profile = options_.profile;
        render.result.track = last_chart_title_.empty() ? current_track() : last_chart_title_;
        render.result.title = last_chart_title_.empty() ? "Unknown Chart" : last_chart_title_;
        render.result.artist = last_chart_artist_;

        if (!has_result_) {
            render.result.notes.push_back("No result data is available for this run.");
            render.result.notes.push_back("Press Enter or Esc to return to Song Select.");
        } else {
            const int judged = judged_total(last_result_.counts);
            const int total_notes = (last_result_.total_notes > 0) ? last_result_.total_notes : judged;
            const int64_t score = calculate_score(last_result_);
            const double accuracy = calculate_accuracy(last_result_);

            game::GaugeType final_gauge_type = gauge_type_from_mode_string(config_.mode.gauge);
            if (!last_result_.shifts.empty()) {
                final_gauge_type = last_result_.shifts.back().to;
            }

            render.result.rank = calculate_rank(last_result_, last_game_over_);
            render.result.status = last_game_over_ ? "GAME OVER" : "CLEAR";
            render.result.gauge_label = gauge_type_label(final_gauge_type);
            render.result.score = score;
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
            render.result.miss = last_result_.counts.pr;
            render.result.mean_delta_ms = last_result_.mean_delta_ms;
            render.result.stddev_delta_ms = last_result_.stddev_delta_ms();
            render.result.shift_count = static_cast<int>(last_result_.shifts.size());
            render.result.export_warning_count = static_cast<int>(last_export_warnings_.size());
            render.result.replay_file = filename_only(last_replay_path_);
            render.result.result_file = filename_only(last_result_path_);

            if (!render.result.replay_file.empty()) {
                render.result.notes.push_back("Replay: " + render.result.replay_file);
            }
            if (!render.result.result_file.empty()) {
                render.result.notes.push_back("Result: " + render.result.result_file);
            }
            render.result.notes.push_back("Timing center " + format_signed_ms(last_result_.mean_delta_ms) +
                                          "  spread " + format_decimal(render.result.stddev_delta_ms) + "ms");
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
                const float position = static_cast<float>(
                    std::clamp(static_cast<double>(shift.sample) / static_cast<double>(graph_end_sample), 0.0, 1.0));
                render.result.gauge_shifts.push_back(
                    render::ResultShiftMarker{position,
                                              short_gauge_type_label(shift.from) + "->" +
                                                  short_gauge_type_label(shift.to)});
            }
        }
    } else {
        render.kind = render::MenuScreenKind::GenericList;
        render.generic.heading = screen_title();
        auto add_row = [&](std::string label,
                           std::string value,
                           bool selected,
                           render::MenuHitTargetKind target_kind,
                           int row_index,
                           bool activatable,
                           bool adjustable) {
            render::MenuRowData row;
            row.label = std::move(label);
            row.value = std::move(value);
            row.selected = selected;
            row.target_kind = target_kind;
            row.row_index = row_index;
            row.activatable = activatable;
            row.adjustable = adjustable;
            row.decrement_enabled = adjustable;
            row.increment_enabled = adjustable;
            render.generic.rows.push_back(std::move(row));
        };

        if (screen_ == Screen::OptionsHub) {
            add_row("Audio", "", options_cursor_ == 0, render::MenuHitTargetKind::OptionsItem, 0, true, false);
            add_row("Graphics", "", options_cursor_ == 1, render::MenuHitTargetKind::OptionsItem, 1, true, false);
            add_row("Skins", "", options_cursor_ == 2, render::MenuHitTargetKind::OptionsItem, 2, true, false);
            add_row("Input", "", options_cursor_ == 3, render::MenuHitTargetKind::OptionsItem, 3, true, false);
            add_row("Mode", "", options_cursor_ == 4, render::MenuHitTargetKind::OptionsItem, 4, true, false);
            add_row("Keymap", "", options_cursor_ == 5, render::MenuHitTargetKind::OptionsItem, 5, true, false);
            add_row("Back", "", options_cursor_ == 6, render::MenuHitTargetKind::OptionsItem, 6, true, false);
            render.generic.notes.push_back("Up/Down to move, Enter to select, Esc to return.");
        } else if (screen_ == Screen::EditStub) {
            render.generic.notes.push_back("Editor is not implemented yet.");
            add_row("Back", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
        } else if (screen_ == Screen::SettingsAudio) {
            add_row("Preset", preset_label(config_.audio_ui.preset), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
            add_row("Keysound Mode", keysound_policy_label(config_.audio_ui.bms_keysound_policy), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
            add_row("Master Volume", format_percent(config_.audio_ui.master_volume), settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
            add_row("BGM Volume", format_percent(config_.audio_ui.bgm_volume), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
            add_row("Keysound Volume", format_percent(config_.audio_ui.keysound_volume), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
            add_row("Back", "", settings_cursor_ == 5, render::MenuHitTargetKind::SettingsRow, 5, true, false);
            render.generic.notes.push_back("Follow: note hits trigger keysounds. Autoplay: note keysounds are mixed into background audio.");
            render.generic.notes.push_back("Off: skip note keysounds. Autoplay mode routes note keysounds through BGM volume.");
            render.generic.notes.push_back("Left/Right or click +/- to change. Back saves and returns.");
        } else if (screen_ == Screen::SettingsGraphics) {
            add_row("Display", display_label(config_.graphics.display_mode), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
            add_row("Resolution", resolution_label(config_.graphics.resolution), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
            add_row("Refresh Hz", std::to_string(config_.graphics.refresh_hz), settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
            add_row("VSync", on_off(config_.graphics.vsync), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
            add_row("Performance HUD", on_off(config_.graphics.performance_overlay), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
            add_row("Display Offset", format_signed_offset_ms(config_.visual_offset_ms), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
            add_row("Back", "", settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, true, false);
            render.generic.notes.push_back("Performance HUD shows frame graph, AVG ms/FPS, 0.1%/0.01% lows, and max FPS.");
            render.generic.notes.push_back("Resolution cycles 720p, 1080p, QHD, or the current monitor native size. Refresh Hz ranges from 60 to 1050.");
            render.generic.notes.push_back("Menu rendering is capped at 300 Hz. Gameplay uses the configured value up to 1050 Hz.");
            render.generic.notes.push_back("Display Offset shifts only visuals from -500ms to +500ms. Positive values draw notes earlier.");
            render.generic.notes.push_back("Left/Right or click +/- to change. Back saves and returns.");
        } else if (screen_ == Screen::SettingsSkins) {
            skin_edit_mode_ = normalize_skin_edit_mode(skin_edit_mode_);
            const int lane_count = lane_count_for_skin_mode(skin_edit_mode_);
            skin_edit_lane_ = clamp_int(skin_edit_lane_, 0, lane_count - 1);
            const auto preview_lane_colors = config::resolved_skin_lane_colors(config_.skin, skin_edit_mode_);

            add_row("Key Mode", key_mode_label(skin_edit_mode_), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
            add_row("Target Lane",
                    lane_display_label(skin_edit_lane_) + " / " + std::to_string(lane_count),
                    settings_cursor_ == 1, render::MenuHitTargetKind::SettingsRow, 1, false, true);
            add_row("Lane Color",
                    config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]),
                    settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
            add_row("Judge Line", format_percent(config_.skin.judgement_line_position), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
            add_row("Note Width", format_percent(config_.skin.note_width_scale), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
            add_row("Note Height", format_percent(config_.skin.note_height_scale), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
            add_row("Back", "", settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, true, false);

            render.generic.skin_preview.visible = true;
            render.generic.skin_preview.mode_label = key_mode_label(skin_edit_mode_);
            render.generic.skin_preview.selected_color_label =
                config::skin_color_label(preview_lane_colors[static_cast<std::size_t>(skin_edit_lane_)]);
            render.generic.skin_preview.lane_count = lane_count;
            render.generic.skin_preview.selected_lane = skin_edit_lane_ + 1;
            render.generic.skin_preview.judgement_line_position = config_.skin.judgement_line_position;
            render.generic.skin_preview.note_width_scale = config_.skin.note_width_scale;
            render.generic.skin_preview.note_height_scale = config_.skin.note_height_scale;
            render.generic.skin_preview.lane_colors.fill(0);
            for (int lane = 0; lane < lane_count && lane < static_cast<int>(kGameplayHudMaxLanes); ++lane) {
                render.generic.skin_preview.lane_colors[static_cast<std::size_t>(lane)] =
                    config::skin_color_rgb(preview_lane_colors[static_cast<std::size_t>(lane)]);
            }

            render.generic.notes.push_back("Key Mode switches the editable 4K-10K lane layout.");
            render.generic.notes.push_back("Target Lane selects which lane color the Lane Color row edits.");
            render.generic.notes.push_back("The preview updates immediately with the current colors, judge line, and note size.");
        } else if (screen_ == Screen::SettingsInput) {
            add_row("Polling Hz", std::to_string(config_.input.polling_hz), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
            add_row("Back", "", settings_cursor_ == 1, render::MenuHitTargetKind::SettingsRow, 1, true, false);
            render.generic.notes.push_back("Left/Right or click +/- to change. Back saves and returns.");
        } else if (screen_ == Screen::ModeSelect) {
            add_row("OSU Charts", on_off(config_.mode.enable_osu_charts), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
            add_row("Chart Filter",
                    config_.mode.enable_osu_charts ? format_label(config_.mode.format) : std::string("BMS"),
                    settings_cursor_ == 1, render::MenuHitTargetKind::SettingsRow, 1, false, true);
            add_row("Key Mode",
                    config_.mode.enable_osu_charts ? key_mode_label(config_.mode.key_mode) : std::string("10K"),
                    settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, false, true);
            add_row("Gauge", gauge_label(config_.mode.gauge), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
            add_row("Random", random_label(config_.mode.random), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
            add_row("Random Seed", std::to_string(config_.mode.random_seed), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
            add_row("Rate", format_multiplier(config_.speed.rate), settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow, 6, false, true);
            add_row("Hi-Speed", format_decimal(config_.speed.hi_speed), settings_cursor_ == 7,
                    render::MenuHitTargetKind::SettingsRow, 7, false, true);
            add_row("Back", "", settings_cursor_ == 8, render::MenuHitTargetKind::SettingsRow, 8, true, false);
            render.generic.notes.push_back("OSU Charts adds 4K-10K .osu beatmaps to song indexing and runtime loading.");
            render.generic.notes.push_back("Chart Filter switches the visible library between BMS, OSU, or All.");
            render.generic.notes.push_back("Key Mode selects Auto or a preferred osu lane mode from 4K through 10K.");
            render.generic.notes.push_back("Back saves the toggle/filter and refreshes the song library cache when needed.");
        } else if (screen_ == Screen::Keymap) {
            config::KeymapManager keymap_manager;
            const auto current_bindings = keymap_manager.bindings_for_mode(working_keymap_, keymap_edit_mode_);
            add_row("Key Mode", key_mode_label(keymap_edit_mode_), keymap_cursor_ == 0,
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
                add_row(lane, key_name, static_cast<int>(i) + 1 == keymap_cursor_,
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
            add_row("Save", "", false, render::MenuHitTargetKind::KeymapButton, 0, !keymap_capture_active_, false);
            add_row("Reset", "", false, render::MenuHitTargetKind::KeymapButton, 1, !keymap_capture_active_, false);
            add_row("NKRO Test", "", false, render::MenuHitTargetKind::KeymapButton, 2, !keymap_capture_active_, false);
            add_row("Back", "", false, render::MenuHitTargetKind::KeymapButton, 3, !keymap_capture_active_, false);
            render.generic.notes.push_back("Left/Right on Key Mode selects which 4K-10K layout you are editing.");
            render.generic.notes.push_back("Enter binds the selected lane. A=Save  R=Reset  F2=NKRO Test  Esc=Back");
        } else if (screen_ == Screen::KeymapConfirm) {
            render.generic.notes.push_back("Duplicate binding detected.");
            render.generic.notes.push_back("Key: " + keymap_pending_key_);
            render.generic.notes.push_back("Already used by: " + keymap_duplicate_lane_);
            add_row("Replace Existing Binding", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
            add_row("Cancel", "", false, render::MenuHitTargetKind::SettingsRow, 1, true, false);
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
                add_row(lane, key_name + (down ? " [DOWN]" : ""), down,
                        render::MenuHitTargetKind::None, static_cast<int>(i), false, false);
            }
            add_row("Back", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
        } else if (screen_ == Screen::Result) {
            if (!has_result_) {
                render.generic.notes.push_back("No result data.");
            } else {
                add_row("Chart", last_chart_title_.empty() ? "Unknown" : last_chart_title_, false,
                        render::MenuHitTargetKind::None, 0, false, false);
                add_row("Status", last_game_over_ ? "Game Over" : "Clear", false,
                        render::MenuHitTargetKind::None, 1, false, false);
                add_row("Max Combo", std::to_string(last_result_.max_combo), false,
                        render::MenuHitTargetKind::None, 2, false, false);
                render.generic.notes.push_back("Judgements: PG " + std::to_string(last_result_.counts.pg) +
                                               " GR " + std::to_string(last_result_.counts.gr) +
                                               " GD " + std::to_string(last_result_.counts.gd) +
                                               " BD " + std::to_string(last_result_.counts.bd) +
                                               " PR " + std::to_string(last_result_.counts.pr));
                render.generic.notes.push_back("Timing: mean " +
                                               std::to_string(static_cast<int>(last_result_.mean_delta_ms)) +
                                               "ms  stddev " +
                                               std::to_string(static_cast<int>(last_result_.stddev_delta_ms())) + "ms");
                render.generic.notes.push_back("Gauge shifts: " + std::to_string(last_result_.shifts.size()));
                if (!last_replay_path_.empty()) {
                    render.generic.notes.push_back(
                        "Replay: " + path_from_utf8(last_replay_path_).filename().u8string());
                }
                if (!last_result_path_.empty()) {
                    render.generic.notes.push_back(
                        "Result file: " + path_from_utf8(last_result_path_).filename().u8string());
                }
                if (!last_export_warnings_.empty()) {
                    render.generic.notes.push_back("Export warnings: " +
                                                   std::to_string(last_export_warnings_.size()));
                }
            }
            add_row("Back to Song Select", "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
        }
    }

    snapshot.render = std::move(render);
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        snapshot_ = std::move(snapshot);
        ++snapshot_version_;
    }
}

void MenuApp::render_tick() {
    const bool show_performance_overlay = config_.graphics.performance_overlay;
    const render::RenderPerformanceSnapshot perf_snapshot =
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
        render_cache_.performance.visible = show_performance_overlay;
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
    }

    if (render_cache_.kind == render::MenuScreenKind::GameplayHud) {
        uint64_t gameplay_revision = 0;
        {
            std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
            gameplay_revision = gameplay_hud_.revision;
        }
        if (snapshot_changed || rendered_gameplay_hud_version_ != gameplay_revision) {
            populate_gameplay_render_data(render_cache_.gameplay, &gameplay_revision);
            if (render_cache_.gameplay.bpm > 0.0 && render_cache_.gameplay.rate > 0.0) {
                render_cache_.gameplay.scroll_speed =
                    (render_cache_.gameplay.bpm * render_cache_.gameplay.hispeed) / render_cache_.gameplay.rate;
            } else {
                render_cache_.gameplay.scroll_speed = 0.0;
            }
            rendered_gameplay_hud_version_ = gameplay_revision;
        }
    } else {
        rendered_gameplay_hud_version_ = 0;
    }
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
        auto parsed = parse_result_file(entry.path(), &parse_error);
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
        candidate.rank = calculate_rank(parsed->stats, parsed->game_over);
        candidate.best_score = calculate_score(parsed->stats);
        candidate.clear_status = parsed->clear_status;
        candidate.max_combo = parsed->stats.max_combo;
        candidate.perfect = parsed->stats.counts.pg;
        candidate.great = parsed->stats.counts.gr;
        candidate.good = parsed->stats.counts.gd;
        candidate.bad = parsed->stats.counts.bd;
        candidate.miss = parsed->stats.counts.pr;
        candidate.created_utc = parsed->created_utc;
        const int candidate_judged = judged_total(parsed->stats.counts);
        const int candidate_clear_priority =
            clear_status_priority(parsed->clear_status, parsed->game_over, parsed->final_gauge);

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
        record.score = candidate.best_score;
        record.accuracy = calculate_accuracy(parsed->stats);
        record.max_combo = parsed->stats.max_combo;
        record.total_notes = parsed->stats.total_notes;
        record.judged_notes = candidate_judged;
        record.perfect = parsed->stats.counts.pg;
        record.great = parsed->stats.counts.gr;
        record.good = parsed->stats.counts.gd;
        record.bad = parsed->stats.counts.bd;
        record.miss = parsed->stats.counts.pr;
        record.mean_delta_ms = parsed->stats.mean_delta_ms;
        record.stddev_delta_ms = parsed->stats.stddev_delta_ms();
        const std::size_t record_index = local_play_records_.size();
        local_play_records_.push_back(record);

        for (const auto& key : build_chart_path_keys(parsed->chart_path, songs_path_)) {
            chart_play_record_indices_[key].push_back(record_index);
            auto existing = chart_best_results_.find(key);
            if (existing == chart_best_results_.end()) {
                chart_best_results_.emplace(key, candidate);
                continue;
            }

            const int existing_judged = existing->second.perfect + existing->second.great + existing->second.good +
                                        existing->second.bad + existing->second.miss;
            const int existing_clear_priority =
                clear_status_priority(existing->second.clear_status, false, std::string_view{});
            if (is_better_record(candidate.best_score,
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
    for (const auto& key : build_chart_path_keys(entry->path, songs_path_)) {
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
                         const int lhs_clear = clear_status_priority(lhs.clear_status, lhs.game_over, lhs.final_gauge);
                         const int rhs_clear = clear_status_priority(rhs.clear_status, rhs.game_over, rhs.final_gauge);
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
        auto parsed = parse_replay_file(path_from_utf8(path), &parse_error);
        if (parsed.has_value()) {
            summary.sample_rate = parsed->sample_rate;
            summary.lane_count = parsed->lane_count;
            summary.event_count = parsed->event_count;
            summary.duration_samples = parsed->duration_samples;
            summary.rate = parsed->rate;
            summary.input_offset_ms = parsed->input_offset_ms;
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
        for (const auto& key : build_chart_path_keys(entry->path, songs_path_)) {
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
            if (is_better_record(found->second.best_score,
                                 clear_status_priority(found->second.clear_status, false, std::string_view{}),
                                 found->second.max_combo,
                                 found_judged,
                                 found->second.created_utc,
                                 best.best_score,
                                 clear_status_priority(best.clear_status, false, std::string_view{}),
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
    auto parsed = parse_result_file(path_from_utf8(record->result_path), &parse_error);
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
    last_replay_path_ = record->replay_path;
    last_result_path_ = record->result_path;
    last_export_warnings_.clear();
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

std::string MenuApp::selected_song_absolute_path() const {
    if (selected_song_ < 0) {
        return {};
    }
    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    if (!entry) {
        return {};
    }

    namespace fs = std::filesystem;
    fs::path candidate = path_from_utf8(entry->path);
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

std::string MenuApp::selected_song_background_preview_path() {
    const SongEntry* entry = (selected_song_ >= 0) ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                                   : nullptr;
    if (!entry) {
        return {};
    }

    const std::string chart_path = selected_song_absolute_path();
    if (chart_path.empty()) {
        return {};
    }

    const std::string cache_key = normalize_path_key(path_from_utf8(chart_path));
    if (!cache_key.empty()) {
        auto cached = song_background_preview_cache_.find(cache_key);
        if (cached != song_background_preview_cache_.end()) {
            return cached->second;
        }
    }

    namespace fs = std::filesystem;
    const fs::path chart_fs_path = path_from_utf8(chart_path);
    const std::string chart_ext = to_lower_ascii(chart_fs_path.extension().u8string());
    std::string resolved_path;

    if (chart_ext == ".osu") {
        std::ifstream file(chart_fs_path, std::ios::binary);
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            chart::OsuManiaLoader loader;
            const auto parsed = loader.parse(buffer.str());
            if (!parsed.chart.background_filename.empty()) {
                if (auto preview = resolve_preview_asset_path(chart_fs_path, parsed.chart.background_filename);
                    preview.has_value()) {
                    resolved_path = preview->u8string();
                }
            }
        }
    } else if (is_bms_chart_extension(chart_ext)) {
        chart::BmsParser parser;
        chart::BmsParserOptions options;
        options.tolerant = true;
        const auto parsed = parser.parseFile(chart_path, options);
        for (const auto& preview_reference : collect_bms_preview_references(parsed.chart)) {
            if (auto preview = resolve_preview_asset_path(chart_fs_path, preview_reference); preview.has_value()) {
                resolved_path = preview->u8string();
                break;
            }
        }
    }

    if (!cache_key.empty()) {
        song_background_preview_cache_[cache_key] = resolved_path;
    }
    return resolved_path;
}

void MenuApp::launch_selected_song() {
    const std::string chart_path = selected_song_absolute_path();
    if (chart_path.empty()) {
        return;
    }
    launch_gameplay(chart_path);
}

void MenuApp::launch_gameplay(const std::string& chart_path) {
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
        gameplay_hud_ = {};
        gameplay_hud_.loading = true;
        gameplay_hud_.loading_percent = 0;
        gameplay_hud_.loading_stage = "Preparing gameplay";
        ++gameplay_hud_.revision;
    }
    publish_snapshot();

    input_thread_.stop();
    audio_thread_.stop();

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
        gameplay_hud_.snapshot_time_ns = hud.snapshot_time_ns;
        gameplay_hud_.lookahead_samples = hud.lookahead_samples;
        gameplay_hud_.past_samples = hud.past_samples;
        gameplay_hud_.combo = hud.combo;
        gameplay_hud_.max_combo = hud.max_combo;
        gameplay_hud_.counts = hud.counts;
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
        ++gameplay_hud_.revision;
    });

    CommandLineOptions play_options = options_;
    play_options.chart_path = chart_path;
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
            gameplay_hud_ = {};
            ++gameplay_hud_.revision;
        }
        screen_ = Screen::SongSelect;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }

    session.run();
    session.shutdown();

    restart_input_thread();
    restart_audio_thread();
    const auto& result = session.result();
    if (result.has_value) {
        last_result_ = result.stats;
        last_game_over_ = result.game_over;
        has_result_ = true;
        last_replay_path_ = result.replay_path;
        last_result_path_ = result.result_path;
        last_export_warnings_ = result.export_warnings;
        reload_chart_best_results();
        screen_ = Screen::Result;
    } else {
        has_result_ = false;
        last_replay_path_.clear();
        last_result_path_.clear();
        last_export_warnings_.clear();
        screen_ = Screen::SongSelect;
    }
    apply_runtime_graphics_config();
    {
        std::lock_guard<std::mutex> lock(gameplay_hud_mutex_);
        gameplay_hud_ = {};
        ++gameplay_hud_.revision;
    }
    publish_snapshot();
}

std::string MenuApp::screen_title() const {
    switch (screen_) {
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
        case Screen::Keymap: return "Keymap";
        case Screen::KeymapConfirm: return "Keymap Confirm";
        case Screen::KeymapTest: return "NKRO Test";
        case Screen::Result: return "Result";
        default: return "Menu";
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
            source_song_counts_[normalize_path_key(path_from_utf8(songs_path_))] = 0;
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

    source_song_counts_[normalize_path_key(path_from_utf8(songs_path_))] =
        static_cast<int>(visible_song_count());
    log_memory_phase("MenuApp",
                     "visible-list-rebuilt",
                     query_process_memory_snapshot(),
                     "entries=" + std::to_string(indexed_songs_.size()) +
                         " visible=" + std::to_string(visible_song_count()));
}

}  // namespace tenriff::app
