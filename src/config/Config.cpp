#include "config/Config.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "app/RuntimeConfigMigration.h"
#include "app/ModeManager.h"
#include "config/SimpleJson.h"

namespace tenriff::config {

namespace {

constexpr double kMasterVolumeMin = 0.0;
constexpr double kMasterVolumeMax = 1.0;
constexpr double kChartMixVolumeMin = 0.0;
constexpr double kChartMixVolumeMax = 2.0;
constexpr double kInputDebounceWindowMin = 0.0;
constexpr double kInputDebounceWindowMax = 25.0;
constexpr double kVisualOffsetMin = -500.0;
constexpr double kVisualOffsetMax = 500.0;
constexpr int kRefreshHzMin = 60;
constexpr int kRefreshHzMax = 1050;

struct SkinPaletteEntry {
    const char* token;
    const char* label;
    uint32_t rgb;
};

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const std::vector<SkinPaletteEntry>& skin_palette_entries() {
    static const std::vector<SkinPaletteEntry> kEntries = {
        {"ice", "Ice", 0xF6F8FF},
        {"azure", "Azure", 0x4F80FF},
        {"gold", "Gold", 0xFAE36E},
        {"mint", "Mint", 0x5EE5A7},
        {"rose", "Rose", 0xFF6B6B},
        {"violet", "Violet", 0xB794F4},
        {"orange", "Orange", 0xFF9F43},
        {"teal", "Teal", 0x6EE7F2},
    };
    return kEntries;
}

int lane_count_for_skin_mode_token(std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    if (normalized == "4k") {
        return 4;
    }
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
    if (normalized == "16k") {
        return 16;
    }
    return 10;
}

const std::unordered_map<std::string, std::vector<std::string>>& default_skin_lane_color_map() {
    static const std::unordered_map<std::string, std::vector<std::string>> kDefaults = {
        {"4k", {"ice", "azure", "azure", "ice"}},
        {"5k", {"ice", "azure", "gold", "azure", "ice"}},
        {"6k", {"ice", "azure", "ice", "ice", "azure", "ice"}},
        {"7k", {"ice", "azure", "ice", "gold", "ice", "azure", "ice"}},
        {"8k", {"ice", "azure", "ice", "teal", "teal", "ice", "azure", "ice"}},
        {"9k", {"ice", "azure", "ice", "teal", "gold", "teal", "ice", "azure", "ice"}},
        {"10k", {"ice", "azure", "ice", "azure", "ice", "ice", "azure", "ice", "azure", "ice"}},
        {"16k", {"ice", "azure", "ice", "azure", "ice", "gold", "teal", "ice",
                 "ice", "teal", "gold", "ice", "azure", "ice", "azure", "ice"}},
    };
    return kDefaults;
}

std::size_t lane_gap_count_for_skin_mode_token(std::string_view key_mode) {
    const int lane_count = lane_count_for_skin_mode_token(key_mode);
    return (lane_count > 1) ? static_cast<std::size_t>(lane_count - 1) : 0u;
}

std::vector<double> default_skin_scale_vector(std::size_t count, double default_value) {
    return std::vector<double>(count, default_value);
}

std::vector<double> sanitize_skin_scale_vector(std::string_view key_mode,
                                               const std::vector<double>& values,
                                               double default_value,
                                               double min_value,
                                               double max_value,
                                               bool spacing_vector) {
    const std::size_t expected_count =
        spacing_vector ? lane_gap_count_for_skin_mode_token(key_mode)
                       : static_cast<std::size_t>(lane_count_for_skin_mode_token(key_mode));
    std::vector<double> sanitized = default_skin_scale_vector(expected_count, default_value);
    for (std::size_t i = 0; i < expected_count && i < values.size(); ++i) {
        const double value = values[i];
        if (!std::isfinite(value)) {
            continue;
        }
        sanitized[i] = std::clamp(value, min_value, max_value);
    }
    return sanitized;
}

void sanitize_skin_config(SkinConfig& skin) {
    skin.source = normalize_skin_source_token(skin.source);
    skin.lr2_resolution_mode = normalize_skin_lr2_resolution_mode_token(skin.lr2_resolution_mode);
    skin.note_shape = normalize_skin_note_shape_token(skin.note_shape);
    skin.judgement_line_position = std::clamp(
        skin.judgement_line_position, kJudgementLinePositionMin, kJudgementLinePositionMax);
    skin.combo_position = std::clamp(
        skin.combo_position, kComboPositionMin, kComboPositionMax);
    std::unordered_map<std::string, std::vector<double>> sanitized_lane_width_scales;
    skin.note_width_scale = std::clamp(
        skin.note_width_scale, kNoteWidthScaleMin, kNoteWidthScaleMax);
    std::unordered_map<std::string, double> sanitized_note_width_scales;
    std::unordered_map<std::string, std::vector<double>> sanitized_lane_spacing_scales;
    skin.note_height_scale = std::clamp(
        skin.note_height_scale, kNoteHeightScaleMin, kNoteHeightScaleMax);
    skin.lane_divider_width_scale = std::clamp(
        skin.lane_divider_width_scale, kLaneDividerWidthScaleMin, kLaneDividerWidthScaleMax);
    skin.lane_center_gap_scale = std::clamp(
        skin.lane_center_gap_scale, kLaneCenterGapScaleMin, kLaneCenterGapScaleMax);
    skin.hold_body_width_scale = std::clamp(
        skin.hold_body_width_scale, kHoldBodyWidthScaleMin, kHoldBodyWidthScaleMax);

    const auto& supported_modes = supported_skin_mode_tokens();
    std::unordered_map<std::string, double> sanitized_note_height_scales;
    std::unordered_map<std::string, double> sanitized_lane_divider_width_scales;
    std::unordered_map<std::string, double> sanitized_lane_center_gap_scales;
    std::unordered_map<std::string, std::vector<std::string>> sanitized_lane_colors;
    for (const auto& [mode, values] : skin.lane_width_scales) {
        const std::string normalized = normalize_skin_mode_token(mode);
        if (std::find(supported_modes.begin(), supported_modes.end(), normalized) == supported_modes.end()) {
            continue;
        }
        sanitized_lane_width_scales[normalized] = sanitize_skin_scale_vector(
            normalized,
            values,
            kLaneWidthScaleDefault,
            kLaneWidthScaleMin,
            kLaneWidthScaleMax,
            false);
    }
    for (const auto& [mode, value] : skin.note_width_scales) {
        const std::string normalized = normalize_skin_mode_token(mode);
        if (!std::isfinite(value) ||
            std::find(supported_modes.begin(), supported_modes.end(), normalized) == supported_modes.end()) {
            continue;
        }
        sanitized_note_width_scales[normalized] =
            std::clamp(value, kNoteWidthScaleMin, kNoteWidthScaleMax);
    }
    for (const auto& [mode, values] : skin.lane_spacing_scales) {
        const std::string normalized = normalize_skin_mode_token(mode);
        if (std::find(supported_modes.begin(), supported_modes.end(), normalized) == supported_modes.end()) {
            continue;
        }
        sanitized_lane_spacing_scales[normalized] = sanitize_skin_scale_vector(
            normalized,
            values,
            kLaneSpacingScaleDefault,
            kLaneSpacingScaleMin,
            kLaneSpacingScaleMax,
            true);
    }
    for (const auto& [mode, value] : skin.note_height_scales) {
        const std::string normalized = normalize_skin_mode_token(mode);
        if (!std::isfinite(value) ||
            std::find(supported_modes.begin(), supported_modes.end(), normalized) == supported_modes.end()) {
            continue;
        }
        sanitized_note_height_scales[normalized] =
            std::clamp(value, kNoteHeightScaleMin, kNoteHeightScaleMax);
    }
    for (const auto& [mode, value] : skin.lane_divider_width_scales) {
        const std::string normalized = normalize_skin_mode_token(mode);
        if (!std::isfinite(value) ||
            std::find(supported_modes.begin(), supported_modes.end(), normalized) == supported_modes.end()) {
            continue;
        }
        sanitized_lane_divider_width_scales[normalized] =
            std::clamp(value, kLaneDividerWidthScaleMin, kLaneDividerWidthScaleMax);
    }
    for (const auto& [mode, value] : skin.lane_center_gap_scales) {
        const std::string normalized = normalize_skin_mode_token(mode);
        if (!std::isfinite(value) ||
            std::find(supported_modes.begin(), supported_modes.end(), normalized) == supported_modes.end()) {
            continue;
        }
        sanitized_lane_center_gap_scales[normalized] =
            std::clamp(value, kLaneCenterGapScaleMin, kLaneCenterGapScaleMax);
    }
    for (const auto& mode : supported_modes) {
        sanitized_lane_colors.emplace(mode, resolved_skin_lane_colors(skin, mode));
    }
    skin.lane_width_scales = std::move(sanitized_lane_width_scales);
    skin.note_width_scales = std::move(sanitized_note_width_scales);
    skin.lane_spacing_scales = std::move(sanitized_lane_spacing_scales);
    skin.note_height_scales = std::move(sanitized_note_height_scales);
    skin.lane_divider_width_scales = std::move(sanitized_lane_divider_width_scales);
    skin.lane_center_gap_scales = std::move(sanitized_lane_center_gap_scales);
    skin.lane_colors = std::move(sanitized_lane_colors);
}

const JsonObject* get_object(const JsonObject& object, std::string_view key) {
    auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return it->second.as_object();
}

const JsonValue* get_value(const JsonObject& object, std::string_view key) {
    auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

bool get_bool(const JsonObject& object, std::string_view key, bool fallback) {
    auto value = get_value(object, key);
    if (!value) {
        return fallback;
    }
    return value->as_bool(fallback);
}

double get_number(const JsonObject& object, std::string_view key, double fallback) {
    auto value = get_value(object, key);
    if (!value) {
        return fallback;
    }
    return value->as_number(fallback);
}

std::string get_string(const JsonObject& object, std::string_view key, std::string fallback) {
    auto value = get_value(object, key);
    if (!value) {
        return fallback;
    }
    return value->as_string(std::move(fallback));
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

std::string normalize_display_mode(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "fullscreen" || value == "windowed") {
        return value;
    }
    return "borderless";
}

std::string normalize_song_index_profile(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "fast" || value == "performance") {
        return "fast";
    }
    return "safe";
}

std::string normalize_ui_language(std::string value) {
    value = to_lower_ascii(std::move(value));
    if (value == "ko" || value == "kr" || value == "korean" || value == "ko-kr") {
        return "ko";
    }
    return "en";
}

int sanitize_refresh_hz(int value, std::vector<std::string>& warnings) {
    if (value >= kRefreshHzMin && value <= kRefreshHzMax) {
        return value;
    }
    warnings.push_back("graphics.refresh_hz must be between 60 and 1050; clamping into range.");
    return std::clamp(value, kRefreshHzMin, kRefreshHzMax);
}

std::vector<std::string> get_string_array(const JsonObject& object, std::string_view key) {
    std::vector<std::string> values;
    auto value = get_value(object, key);
    if (!value) {
        return values;
    }
    const auto* array = value->as_array();
    if (!array) {
        return values;
    }
    values.reserve(array->size());
    for (const auto& item : *array) {
        if (!item.is_string()) {
            continue;
        }
        const std::string text = item.as_string();
        if (!text.empty()) {
            values.push_back(text);
        }
    }
    return values;
}

std::unordered_map<std::string, std::vector<std::string>> get_string_array_object(const JsonObject& object,
                                                                                   std::string_view key) {
    std::unordered_map<std::string, std::vector<std::string>> values;
    const auto* value = get_value(object, key);
    if (!value) {
        return values;
    }
    const auto* root = value->as_object();
    if (!root) {
        return values;
    }

    for (const auto& [name, entry] : *root) {
        if (name.empty()) {
            continue;
        }
        const auto* array = entry.as_array();
        if (!array) {
            continue;
        }
        std::vector<std::string> items;
        items.reserve(array->size());
        for (const auto& item : *array) {
            if (!item.is_string()) {
                continue;
            }
            const std::string text = item.as_string();
            if (!text.empty()) {
                items.push_back(text);
            }
        }
        values.emplace(name, std::move(items));
    }
    return values;
}

std::string normalize_song_collection_filter(std::string value) {
    const std::string lowered = to_lower_ascii(value);
    if (lowered == "favorites" || lowered == "favorite") {
        return "favorites";
    }
    if (lowered == "all" || lowered.empty()) {
        return "all";
    }
    return value;
}

bool is_allowed_polling_hz(int value) {
    return value == 1000 || value == 2000 || value == 4000 || value == 8000;
}

int sanitize_polling_hz(int value, std::vector<std::string>& warnings) {
    if (is_allowed_polling_hz(value)) {
        return value;
    }
    warnings.push_back("input.polling_hz must be 1000/2000/4000/8000; using 1000.");
    return 1000;
}

int sanitize_judgement_hz(int value, std::vector<std::string>& warnings) {
    if (is_allowed_polling_hz(value)) {
        return value;
    }
    warnings.push_back("input.judgement_hz must be 1000/2000/4000/8000; using 4000.");
    return 4000;
}

double sanitize_input_debounce_ms(double value, std::vector<std::string>& warnings) {
    if (std::isfinite(value) && value >= kInputDebounceWindowMin && value <= kInputDebounceWindowMax) {
        return value;
    }
    warnings.push_back("input.debounce_ms must be between 0 and 25; clamping into range.");
    if (!std::isfinite(value)) {
        return 8.0;
    }
    return std::clamp(value, kInputDebounceWindowMin, kInputDebounceWindowMax);
}

void apply_audio_preset(RuntimeConfig& config) {
    if (config.audio_ui.preset == "basic") {
        config.audio.frames_per_buffer = 256;
        config.audio.periods = 3;
    } else if (config.audio_ui.preset == "high") {
        config.audio.frames_per_buffer = 320;
        config.audio.periods = 3;
    }
}

std::string normalize_bms_keysound_policy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "ignore" || value == "off") {
        return "ignore";
    }
    if (value == "autoplay") {
        return "autoplay";
    }
    return "follow";
}

void apply_config_object(const JsonObject& root, RuntimeConfig& config) {
    if (auto* audio = get_object(root, "audio")) {
        config.audio.sample_rate = static_cast<uint32_t>(get_number(*audio, "rate", config.audio.sample_rate));
        config.audio.frames_per_buffer =
            static_cast<uint32_t>(get_number(*audio, "frames", config.audio.frames_per_buffer));
        config.audio.periods = static_cast<uint32_t>(get_number(*audio, "periods", config.audio.periods));
        config.audio.exclusive_mode = get_bool(*audio, "exclusive", config.audio.exclusive_mode);
        config.audio.use_mmcss = get_bool(*audio, "use_mmcss", config.audio.use_mmcss);
        config.audio.affinity_core = static_cast<int32_t>(get_number(*audio, "affinity", config.audio.affinity_core));
        config.audio_ui.preset = get_string(*audio, "preset", config.audio_ui.preset);
        config.audio_ui.bms_keysound_policy =
            normalize_bms_keysound_policy(get_string(*audio,
                                                     "bms_keysound_policy",
                                                     config.audio_ui.bms_keysound_policy));
        config.audio_ui.background_sound_enabled =
            get_bool(*audio, "background_sound_enabled", config.audio_ui.background_sound_enabled);
        config.audio_ui.master_volume =
            std::clamp(get_number(*audio, "volume", config.audio_ui.master_volume),
                       kMasterVolumeMin, kMasterVolumeMax);
        config.audio_ui.bgm_volume =
            std::clamp(get_number(*audio, "bgm_volume", config.audio_ui.bgm_volume),
                       kChartMixVolumeMin, kChartMixVolumeMax);
        config.audio_ui.keysound_volume =
            std::clamp(get_number(*audio, "keysound_volume", config.audio_ui.keysound_volume),
                       kChartMixVolumeMin, kChartMixVolumeMax);
    }

    if (auto* input = get_object(root, "input")) {
        config.input.backend = get_string(*input, "backend", config.input.backend);
        config.input.rawinput = get_bool(*input, "rawinput", config.input.rawinput);
        config.input.use_qpc = get_bool(*input, "use_qpc", config.input.use_qpc);
        config.input.grab = get_bool(*input, "grab", config.input.grab);
        config.input.queue_size = static_cast<std::size_t>(get_number(*input, "queue_size", config.input.queue_size));
        config.input.polling_hz = static_cast<int>(get_number(*input, "polling_hz", config.input.polling_hz));
        config.input.judgement_hz = static_cast<int>(get_number(*input, "judgement_hz", config.input.judgement_hz));
        config.input.debounce_ms = get_number(*input, "debounce_ms", config.input.debounce_ms);
    }

    if (auto* judge = get_object(root, "judge")) {
        config.judge.pg_ms = get_number(*judge, "pg", config.judge.pg_ms);
        config.judge.gr_ms = get_number(*judge, "gr", config.judge.gr_ms);
        config.judge.gd_ms = get_number(*judge, "gd", config.judge.gd_ms);
        config.judge.bd_ms = get_number(*judge, "bd", config.judge.bd_ms);
        static_cast<void>(get_number(*judge, "indirect_miss", config.judge.indirect_miss_ms));
        config.judge.hold_grace_ms = get_number(*judge, "hold_grace", config.judge.hold_grace_ms);
        config.judge.hold_break_ms = get_number(*judge, "hold_break", config.judge.hold_break_ms);
        config.judge.mask_ms = get_number(*judge, "mask", config.judge.mask_ms);
        config.judge.indirect_miss_ms = config.judge.bd_ms;
        config.judge.hold_break_ms = std::max(config.judge.hold_break_ms, config.judge.hold_grace_ms);
    }

    if (auto* speed = get_object(root, "speed")) {
        config.speed.rate = get_number(*speed, "rate", config.speed.rate);
        config.speed.hi_speed = get_number(*speed, "hispeed", config.speed.hi_speed);
        config.speed.target_scroll_bps = get_number(*speed, "target_scroll_bps", config.speed.target_scroll_bps);
    }

    if (auto* gauge = get_object(root, "gauge")) {
        if (auto* delta = get_object(*gauge, "delta")) {
            if (auto* hard = get_object(*delta, "hard")) {
                config.gauge.hard.pg = get_number(*hard, "PG", config.gauge.hard.pg);
                config.gauge.hard.gr = get_number(*hard, "GR", config.gauge.hard.gr);
                config.gauge.hard.gd = get_number(*hard, "GD", config.gauge.hard.gd);
                config.gauge.hard.bd = get_number(*hard, "BD", config.gauge.hard.bd);
                config.gauge.hard.pr = get_number(*hard, "PR", config.gauge.hard.pr);
            }
            if (auto* normal = get_object(*delta, "normal")) {
                config.gauge.normal.pg = get_number(*normal, "PG", config.gauge.normal.pg);
                config.gauge.normal.gr = get_number(*normal, "GR", config.gauge.normal.gr);
                config.gauge.normal.gd = get_number(*normal, "GD", config.gauge.normal.gd);
                config.gauge.normal.bd = get_number(*normal, "BD", config.gauge.normal.bd);
                config.gauge.normal.pr = get_number(*normal, "PR", config.gauge.normal.pr);
            }
            if (auto* easy = get_object(*delta, "easy")) {
                config.gauge.easy.pg = get_number(*easy, "PG", config.gauge.easy.pg);
                config.gauge.easy.gr = get_number(*easy, "GR", config.gauge.easy.gr);
                config.gauge.easy.gd = get_number(*easy, "GD", config.gauge.easy.gd);
                config.gauge.easy.bd = get_number(*easy, "BD", config.gauge.easy.bd);
                config.gauge.easy.pr = get_number(*easy, "PR", config.gauge.easy.pr);
            }
        }
    }

    if (auto* graphics = get_object(root, "graphics")) {
        config.graphics.display_mode =
            normalize_display_mode(get_string(*graphics, "display_mode", config.graphics.display_mode));
        config.graphics.resolution =
            normalize_resolution_preset(get_string(*graphics, "resolution", config.graphics.resolution));
        config.graphics.vsync = get_bool(*graphics, "vsync", config.graphics.vsync);
        const auto* refresh_value = get_value(*graphics, "refresh_hz");
        const double fallback_refresh = refresh_value
                                            ? refresh_value->as_number(static_cast<double>(config.graphics.refresh_hz))
                                            : get_number(*graphics, "fps_limit", static_cast<double>(config.graphics.refresh_hz));
        config.graphics.refresh_hz = static_cast<int>(fallback_refresh);
        config.graphics.performance_overlay =
            get_bool(*graphics, "performance_overlay", config.graphics.performance_overlay);
    }

    if (auto* mode = get_object(root, "mode")) {
        config.mode.format = get_string(*mode, "format", config.mode.format);
        config.mode.key_mode = get_string(*mode, "key_mode", config.mode.key_mode);
        config.mode.gauge = get_string(*mode, "gauge", config.mode.gauge);
        config.mode.random = get_string(*mode, "random", config.mode.random);
        config.mode.random_seed = static_cast<uint32_t>(get_number(*mode, "random_seed", config.mode.random_seed));
        if (const auto* mods = get_value(*mode, "mods")) {
            if (mods->as_array()) {
                config.mode.mods = ::tenriff::app::normalize_mode_mod_tokens(get_string_array(*mode, "mods"));
            }
        }
        config.mode.enable_osu_charts = get_bool(*mode, "enable_osu_charts", config.mode.enable_osu_charts);
        config.mode.ghost_battle_enabled =
            get_bool(*mode, "ghost_battle_enabled", config.mode.ghost_battle_enabled);
        config.mode.autoplay_enabled =
            get_bool(*mode, "autoplay_enabled", config.mode.autoplay_enabled);
        config.mode.practice_no_fail_enabled =
            get_bool(*mode, "practice_no_fail_enabled", config.mode.practice_no_fail_enabled);
        config.mode.song_index_profile =
            normalize_song_index_profile(get_string(*mode, "song_index_profile", config.mode.song_index_profile));
    }

    if (auto* ui = get_object(root, "ui")) {
        config.ui.language = normalize_ui_language(get_string(*ui, "language", config.ui.language));
        config.ui.result_tail_ms = get_number(*ui, "result_tail_ms", config.ui.result_tail_ms);
        config.ui.require_enter_to_exit = get_bool(*ui, "require_enter_to_exit", config.ui.require_enter_to_exit);
        config.ui.active_song_source =
            get_string(*ui, "active_song_source", config.ui.active_song_source);
        if (const auto* recent_sources = get_value(*ui, "recent_song_sources")) {
            if (recent_sources->as_array()) {
                config.ui.recent_song_sources = get_string_array(*ui, "recent_song_sources");
            }
        }
        if (const auto* favorites = get_value(*ui, "favorite_chart_keys")) {
            if (favorites->as_array()) {
                config.ui.favorite_chart_keys = get_string_array(*ui, "favorite_chart_keys");
            }
        }
        if (const auto* collections = get_value(*ui, "collections")) {
            if (collections->as_object()) {
                config.ui.collections = get_string_array_object(*ui, "collections");
            }
        }
        config.ui.song_collection_filter = normalize_song_collection_filter(
            get_string(*ui, "song_collection_filter", config.ui.song_collection_filter));
    }

    if (auto* skin = get_object(root, "skin")) {
        config.skin.source =
            normalize_skin_source_token(get_string(*skin, "source", config.skin.source));
        config.skin.osu_skin_name =
            get_string(*skin, "osu_skin_name", config.skin.osu_skin_name);
        config.skin.lr2_skin_name =
            get_string(*skin, "lr2_skin_name", config.skin.lr2_skin_name);
        config.skin.lr2_resolution_mode = normalize_skin_lr2_resolution_mode_token(
            get_string(*skin, "lr2_resolution_mode", config.skin.lr2_resolution_mode));
        config.skin.note_shape =
            normalize_skin_note_shape_token(get_string(*skin, "note_shape", config.skin.note_shape));
        config.skin.note_border_enabled =
            get_bool(*skin, "note_border_enabled", config.skin.note_border_enabled);
        config.skin.preserve_note_image_aspect_ratio =
            get_bool(*skin,
                     "preserve_note_image_aspect_ratio",
                     config.skin.preserve_note_image_aspect_ratio);
        config.skin.show_lane_dividers =
            get_bool(*skin, "show_lane_dividers", config.skin.show_lane_dividers);
        config.skin.show_judgement_line =
            get_bool(*skin, "show_judgement_line", config.skin.show_judgement_line);
        config.skin.show_gear_boundary_line =
            get_bool(*skin, "show_gear_boundary_line", config.skin.show_gear_boundary_line);
        config.skin.hold_tail_taper_enabled =
            get_bool(*skin, "hold_tail_taper_enabled", config.skin.hold_tail_taper_enabled);
        config.skin.judgement_line_position = std::clamp(
            get_number(*skin, "judgement_line_position", config.skin.judgement_line_position),
            kJudgementLinePositionMin, kJudgementLinePositionMax);
        config.skin.combo_position = std::clamp(
            get_number(*skin, "combo_position", config.skin.combo_position),
            kComboPositionMin, kComboPositionMax);
        config.skin.note_width_scale = std::clamp(
            get_number(*skin, "note_width_scale", config.skin.note_width_scale),
            kNoteWidthScaleMin, kNoteWidthScaleMax);
        if (const auto* lane_width_scales = get_object(*skin, "lane_width_scales")) {
            for (const auto& [mode, value] : *lane_width_scales) {
                const auto* array = value.as_array();
                if (!array) {
                    continue;
                }
                std::vector<double> raw_values;
                raw_values.reserve(array->size());
                for (const auto& item : *array) {
                    if (!item.is_number()) {
                        continue;
                    }
                    raw_values.push_back(item.as_number(kLaneWidthScaleDefault));
                }
                config.skin.lane_width_scales[normalize_skin_mode_token(mode)] = sanitize_skin_scale_vector(
                    mode,
                    raw_values,
                    kLaneWidthScaleDefault,
                    kLaneWidthScaleMin,
                    kLaneWidthScaleMax,
                    false);
            }
        }
        config.skin.note_height_scale = std::clamp(
            get_number(*skin, "note_height_scale", config.skin.note_height_scale),
            kNoteHeightScaleMin, kNoteHeightScaleMax);
        if (const auto* lane_spacing_scales = get_object(*skin, "lane_spacing_scales")) {
            for (const auto& [mode, value] : *lane_spacing_scales) {
                const auto* array = value.as_array();
                if (!array) {
                    continue;
                }
                std::vector<double> raw_values;
                raw_values.reserve(array->size());
                for (const auto& item : *array) {
                    if (!item.is_number()) {
                        continue;
                    }
                    raw_values.push_back(item.as_number(kLaneSpacingScaleDefault));
                }
                config.skin.lane_spacing_scales[normalize_skin_mode_token(mode)] = sanitize_skin_scale_vector(
                    mode,
                    raw_values,
                    kLaneSpacingScaleDefault,
                    kLaneSpacingScaleMin,
                    kLaneSpacingScaleMax,
                    true);
            }
        }
        config.skin.lane_divider_width_scale = std::clamp(
            get_number(*skin, "lane_divider_width_scale", config.skin.lane_divider_width_scale),
            kLaneDividerWidthScaleMin, kLaneDividerWidthScaleMax);
        config.skin.lane_center_gap_scale = std::clamp(
            get_number(*skin, "lane_center_gap_scale", config.skin.lane_center_gap_scale),
            kLaneCenterGapScaleMin, kLaneCenterGapScaleMax);
        config.skin.hold_body_width_scale = std::clamp(
            get_number(*skin, "hold_body_width_scale", config.skin.hold_body_width_scale),
            kHoldBodyWidthScaleMin, kHoldBodyWidthScaleMax);
        if (const auto* note_width_scales = get_object(*skin, "note_width_scales")) {
            for (const auto& [mode, value] : *note_width_scales) {
                if (!value.is_number()) {
                    continue;
                }
                config.skin.note_width_scales[normalize_skin_mode_token(mode)] =
                    std::clamp(value.as_number(config.skin.note_width_scale), kNoteWidthScaleMin, kNoteWidthScaleMax);
            }
        }
        if (const auto* note_height_scales = get_object(*skin, "note_height_scales")) {
            for (const auto& [mode, value] : *note_height_scales) {
                if (!value.is_number()) {
                    continue;
                }
                config.skin.note_height_scales[normalize_skin_mode_token(mode)] =
                    std::clamp(value.as_number(config.skin.note_height_scale), kNoteHeightScaleMin, kNoteHeightScaleMax);
            }
        }
        if (const auto* lane_divider_width_scales = get_object(*skin, "lane_divider_width_scales")) {
            for (const auto& [mode, value] : *lane_divider_width_scales) {
                if (!value.is_number()) {
                    continue;
                }
                config.skin.lane_divider_width_scales[normalize_skin_mode_token(mode)] = std::clamp(
                    value.as_number(config.skin.lane_divider_width_scale),
                    kLaneDividerWidthScaleMin,
                    kLaneDividerWidthScaleMax);
            }
        }
        if (const auto* lane_center_gap_scales = get_object(*skin, "lane_center_gap_scales")) {
            for (const auto& [mode, value] : *lane_center_gap_scales) {
                if (!value.is_number()) {
                    continue;
                }
                config.skin.lane_center_gap_scales[normalize_skin_mode_token(mode)] = std::clamp(
                    value.as_number(config.skin.lane_center_gap_scale),
                    kLaneCenterGapScaleMin,
                    kLaneCenterGapScaleMax);
            }
        }
        if (const auto* lane_colors = get_object(*skin, "lane_colors")) {
            for (const auto& [mode, value] : *lane_colors) {
                const auto* array = value.as_array();
                if (!array) {
                    continue;
                }
                auto& dest = config.skin.lane_colors[normalize_skin_mode_token(mode)];
                dest.clear();
                dest.reserve(array->size());
                for (const auto& item : *array) {
                    if (!item.is_string()) {
                        continue;
                    }
                    dest.push_back(normalize_skin_color_token(item.as_string()));
                }
            }
        }
    }
    sanitize_skin_config(config.skin);

    if (auto* offsets = get_object(root, "offsets")) {
        config.input_offset_ms = get_number(*offsets, "input", config.input_offset_ms);
        config.visual_offset_ms = std::clamp(
            get_number(*offsets, "visual", config.visual_offset_ms),
            kVisualOffsetMin, kVisualOffsetMax);
    }
}

bool load_config_file(const std::filesystem::path& path,
                      RuntimeConfig& config,
                      std::string& error,
                      std::vector<std::string>& warnings,
                      bool warn_missing) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (warn_missing) {
            warnings.push_back(path.u8string() + " not found; using defaults.");
        }
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto parse = parse_json(buffer.str());
    if (!parse.success() || !parse.root.has_value()) {
        error = parse.error.empty() ? ("Failed to parse " + path.u8string() + ".") : parse.error;
        return false;
    }

    const auto* root = parse.root->as_object();
    if (!root) {
        error = path.u8string() + " root must be an object.";
        return false;
    }

    apply_config_object(*root, config);
    return true;
}

JsonValue build_json_root(const RuntimeConfig& config) {
    JsonObject root;
    JsonObject audio;
    audio.emplace("rate", JsonValue{static_cast<double>(config.audio.sample_rate)});
    audio.emplace("frames", JsonValue{static_cast<double>(config.audio.frames_per_buffer)});
    audio.emplace("periods", JsonValue{static_cast<double>(config.audio.periods)});
    audio.emplace("exclusive", JsonValue{config.audio.exclusive_mode});
    audio.emplace("use_mmcss", JsonValue{config.audio.use_mmcss});
    audio.emplace("affinity", JsonValue{static_cast<double>(config.audio.affinity_core)});
    audio.emplace("preset", JsonValue{config.audio_ui.preset});
    audio.emplace("bms_keysound_policy", JsonValue{config.audio_ui.bms_keysound_policy});
    audio.emplace("background_sound_enabled", JsonValue{config.audio_ui.background_sound_enabled});
    audio.emplace("volume", JsonValue{config.audio_ui.master_volume});
    audio.emplace("bgm_volume", JsonValue{config.audio_ui.bgm_volume});
    audio.emplace("keysound_volume", JsonValue{config.audio_ui.keysound_volume});
    root.emplace("audio", JsonValue{std::move(audio)});

    JsonObject input;
    input.emplace("backend", JsonValue{config.input.backend});
    input.emplace("rawinput", JsonValue{config.input.rawinput});
    input.emplace("use_qpc", JsonValue{config.input.use_qpc});
    input.emplace("grab", JsonValue{config.input.grab});
    input.emplace("queue_size", JsonValue{static_cast<double>(config.input.queue_size)});
    input.emplace("polling_hz", JsonValue{static_cast<double>(config.input.polling_hz)});
    input.emplace("judgement_hz", JsonValue{static_cast<double>(config.input.judgement_hz)});
    input.emplace("debounce_ms", JsonValue{config.input.debounce_ms});
    root.emplace("input", JsonValue{std::move(input)});

    JsonObject judge;
    judge.emplace("pg", JsonValue{config.judge.pg_ms});
    judge.emplace("gr", JsonValue{config.judge.gr_ms});
    judge.emplace("gd", JsonValue{config.judge.gd_ms});
    judge.emplace("bd", JsonValue{config.judge.bd_ms});
    judge.emplace("hold_grace", JsonValue{config.judge.hold_grace_ms});
    judge.emplace("hold_break", JsonValue{std::max(config.judge.hold_break_ms, config.judge.hold_grace_ms)});
    judge.emplace("mask", JsonValue{config.judge.mask_ms});
    root.emplace("judge", JsonValue{std::move(judge)});

    JsonObject speed;
    speed.emplace("rate", JsonValue{config.speed.rate});
    speed.emplace("hispeed", JsonValue{config.speed.hi_speed});
    speed.emplace("target_scroll_bps", JsonValue{config.speed.target_scroll_bps});
    root.emplace("speed", JsonValue{std::move(speed)});

    JsonObject gauge;
    JsonObject delta;
    JsonObject hard;
    hard.emplace("PG", JsonValue{config.gauge.hard.pg});
    hard.emplace("GR", JsonValue{config.gauge.hard.gr});
    hard.emplace("GD", JsonValue{config.gauge.hard.gd});
    hard.emplace("BD", JsonValue{config.gauge.hard.bd});
    hard.emplace("PR", JsonValue{config.gauge.hard.pr});
    delta.emplace("hard", JsonValue{std::move(hard)});

    JsonObject normal;
    normal.emplace("PG", JsonValue{config.gauge.normal.pg});
    normal.emplace("GR", JsonValue{config.gauge.normal.gr});
    normal.emplace("GD", JsonValue{config.gauge.normal.gd});
    normal.emplace("BD", JsonValue{config.gauge.normal.bd});
    normal.emplace("PR", JsonValue{config.gauge.normal.pr});
    delta.emplace("normal", JsonValue{std::move(normal)});

    JsonObject easy;
    easy.emplace("PG", JsonValue{config.gauge.easy.pg});
    easy.emplace("GR", JsonValue{config.gauge.easy.gr});
    easy.emplace("GD", JsonValue{config.gauge.easy.gd});
    easy.emplace("BD", JsonValue{config.gauge.easy.bd});
    easy.emplace("PR", JsonValue{config.gauge.easy.pr});
    delta.emplace("easy", JsonValue{std::move(easy)});

    gauge.emplace("delta", JsonValue{std::move(delta)});
    root.emplace("gauge", JsonValue{std::move(gauge)});

    JsonObject graphics;
    graphics.emplace("display_mode", JsonValue{normalize_display_mode(config.graphics.display_mode)});
    graphics.emplace("resolution", JsonValue{config.graphics.resolution});
    graphics.emplace("vsync", JsonValue{config.graphics.vsync});
    graphics.emplace("refresh_hz", JsonValue{static_cast<double>(config.graphics.refresh_hz)});
    graphics.emplace("performance_overlay", JsonValue{config.graphics.performance_overlay});
    root.emplace("graphics", JsonValue{std::move(graphics)});

    JsonObject mode;
    mode.emplace("format", JsonValue{config.mode.format});
    mode.emplace("key_mode", JsonValue{config.mode.key_mode});
    mode.emplace("gauge", JsonValue{config.mode.gauge});
    mode.emplace("random", JsonValue{config.mode.random});
    mode.emplace("random_seed", JsonValue{static_cast<double>(config.mode.random_seed)});
    const auto normalized_mods = ::tenriff::app::normalize_mode_mod_tokens(config.mode.mods);
    JsonArray mods;
    mods.reserve(normalized_mods.size());
    for (const auto& token : normalized_mods) {
        mods.emplace_back(token);
    }
    mode.emplace("mods", JsonValue{std::move(mods)});
    mode.emplace("enable_osu_charts", JsonValue{config.mode.enable_osu_charts});
    mode.emplace("ghost_battle_enabled", JsonValue{config.mode.ghost_battle_enabled});
    mode.emplace("autoplay_enabled", JsonValue{config.mode.autoplay_enabled});
    mode.emplace("practice_no_fail_enabled", JsonValue{config.mode.practice_no_fail_enabled});
    mode.emplace("song_index_profile", JsonValue{normalize_song_index_profile(config.mode.song_index_profile)});
    root.emplace("mode", JsonValue{std::move(mode)});

    JsonObject ui;
    ui.emplace("language", JsonValue{normalize_ui_language(config.ui.language)});
    ui.emplace("result_tail_ms", JsonValue{config.ui.result_tail_ms});
    ui.emplace("require_enter_to_exit", JsonValue{config.ui.require_enter_to_exit});
    ui.emplace("active_song_source", JsonValue{config.ui.active_song_source});
    JsonArray recent_song_sources;
    recent_song_sources.reserve(config.ui.recent_song_sources.size());
    for (const auto& source : config.ui.recent_song_sources) {
        recent_song_sources.emplace_back(source);
    }
    ui.emplace("recent_song_sources", JsonValue{std::move(recent_song_sources)});
    JsonArray favorite_chart_keys;
    favorite_chart_keys.reserve(config.ui.favorite_chart_keys.size());
    for (const auto& key : config.ui.favorite_chart_keys) {
        favorite_chart_keys.emplace_back(key);
    }
    ui.emplace("favorite_chart_keys", JsonValue{std::move(favorite_chart_keys)});
    JsonObject collections;
    for (const auto& [name, chart_keys] : config.ui.collections) {
        if (name.empty()) {
            continue;
        }
        JsonArray keys;
        keys.reserve(chart_keys.size());
        for (const auto& chart_key : chart_keys) {
            keys.emplace_back(chart_key);
        }
        collections.emplace(name, JsonValue{std::move(keys)});
    }
    ui.emplace("collections", JsonValue{std::move(collections)});
    ui.emplace("song_collection_filter", JsonValue{normalize_song_collection_filter(config.ui.song_collection_filter)});
    root.emplace("ui", JsonValue{std::move(ui)});

    JsonObject skin;
    skin.emplace("source", JsonValue{normalize_skin_source_token(config.skin.source)});
    skin.emplace("osu_skin_name", JsonValue{config.skin.osu_skin_name});
    skin.emplace("lr2_skin_name", JsonValue{config.skin.lr2_skin_name});
    skin.emplace("lr2_resolution_mode",
                 JsonValue{normalize_skin_lr2_resolution_mode_token(config.skin.lr2_resolution_mode)});
    skin.emplace("note_shape", JsonValue{normalize_skin_note_shape_token(config.skin.note_shape)});
    skin.emplace("note_border_enabled", JsonValue{config.skin.note_border_enabled});
    skin.emplace("preserve_note_image_aspect_ratio",
                 JsonValue{config.skin.preserve_note_image_aspect_ratio});
    skin.emplace("show_lane_dividers", JsonValue{config.skin.show_lane_dividers});
    skin.emplace("show_judgement_line", JsonValue{config.skin.show_judgement_line});
    skin.emplace("show_gear_boundary_line", JsonValue{config.skin.show_gear_boundary_line});
    skin.emplace("hold_tail_taper_enabled", JsonValue{config.skin.hold_tail_taper_enabled});
    skin.emplace("judgement_line_position", JsonValue{config.skin.judgement_line_position});
    skin.emplace("combo_position", JsonValue{config.skin.combo_position});
    skin.emplace("note_width_scale", JsonValue{config.skin.note_width_scale});
    JsonObject lane_width_scales;
    JsonObject note_width_scales;
    JsonObject lane_spacing_scales;
    skin.emplace("note_height_scale", JsonValue{config.skin.note_height_scale});
    skin.emplace("lane_divider_width_scale", JsonValue{config.skin.lane_divider_width_scale});
    skin.emplace("lane_center_gap_scale", JsonValue{config.skin.lane_center_gap_scale});
    skin.emplace("hold_body_width_scale", JsonValue{config.skin.hold_body_width_scale});
    JsonObject note_height_scales;
    JsonObject lane_divider_width_scales;
    JsonObject lane_center_gap_scales;
    for (const auto& mode : supported_skin_mode_tokens()) {
        JsonArray lane_widths;
        const auto resolved_lane_widths = resolved_skin_lane_width_scales(config.skin, mode);
        lane_widths.reserve(resolved_lane_widths.size());
        for (const double value : resolved_lane_widths) {
            lane_widths.emplace_back(JsonValue{value});
        }
        lane_width_scales.emplace(mode, JsonValue{std::move(lane_widths)});
        note_width_scales.emplace(mode, JsonValue{resolved_skin_note_width_scale(config.skin, mode)});
        JsonArray lane_spacings;
        const auto resolved_lane_spacings = resolved_skin_lane_spacing_scales(config.skin, mode);
        lane_spacings.reserve(resolved_lane_spacings.size());
        for (const double value : resolved_lane_spacings) {
            lane_spacings.emplace_back(JsonValue{value});
        }
        lane_spacing_scales.emplace(mode, JsonValue{std::move(lane_spacings)});
        note_height_scales.emplace(mode, JsonValue{resolved_skin_note_height_scale(config.skin, mode)});
        lane_divider_width_scales.emplace(mode, JsonValue{resolved_skin_lane_divider_width_scale(config.skin, mode)});
        lane_center_gap_scales.emplace(mode, JsonValue{resolved_skin_lane_center_gap_scale(config.skin, mode)});
    }
    skin.emplace("lane_width_scales", JsonValue{std::move(lane_width_scales)});
    skin.emplace("note_width_scales", JsonValue{std::move(note_width_scales)});
    skin.emplace("lane_spacing_scales", JsonValue{std::move(lane_spacing_scales)});
    skin.emplace("note_height_scales", JsonValue{std::move(note_height_scales)});
    skin.emplace("lane_divider_width_scales", JsonValue{std::move(lane_divider_width_scales)});
    skin.emplace("lane_center_gap_scales", JsonValue{std::move(lane_center_gap_scales)});
    JsonObject lane_colors;
    for (const auto& mode : supported_skin_mode_tokens()) {
        JsonArray colors;
        const auto resolved = resolved_skin_lane_colors(config.skin, mode);
        colors.reserve(resolved.size());
        for (const auto& token : resolved) {
            colors.emplace_back(token);
        }
        lane_colors.emplace(mode, JsonValue{std::move(colors)});
    }
    skin.emplace("lane_colors", JsonValue{std::move(lane_colors)});
    root.emplace("skin", JsonValue{std::move(skin)});

    JsonObject offsets;
    offsets.emplace("input", JsonValue{config.input_offset_ms});
    offsets.emplace("visual", JsonValue{config.visual_offset_ms});
    root.emplace("offsets", JsonValue{std::move(offsets)});

    return JsonValue{std::move(root)};
}

}  // namespace

std::string normalize_skin_mode_token(std::string_view key_mode) {
    std::string normalized = to_lower_ascii(std::string(key_mode));
    if (normalized == "4" || normalized == "4key" || normalized == "keys4") {
        return "4k";
    }
    if (normalized == "5" || normalized == "5key" || normalized == "keys5") {
        return "5k";
    }
    if (normalized == "6" || normalized == "6key" || normalized == "keys6") {
        return "6k";
    }
    if (normalized == "7" || normalized == "7key" || normalized == "keys7") {
        return "7k";
    }
    if (normalized == "8" || normalized == "8key" || normalized == "keys8") {
        return "8k";
    }
    if (normalized == "9" || normalized == "9key" || normalized == "keys9") {
        return "9k";
    }
    if (normalized == "10" || normalized == "10key" || normalized == "keys10") {
        return "10k";
    }
    if (normalized == "16" || normalized == "16key" || normalized == "keys16") {
        return "16k";
    }
    if (normalized == "4k" || normalized == "5k" || normalized == "6k" || normalized == "7k" ||
        normalized == "8k" || normalized == "9k" || normalized == "10k" || normalized == "16k") {
        return normalized;
    }
    return "10k";
}

std::string normalize_song_index_profile_token(std::string_view token) {
    return normalize_song_index_profile(std::string(token));
}

std::string normalize_ui_language_token(std::string_view token) {
    return normalize_ui_language(std::string(token));
}

std::vector<std::string> supported_skin_mode_tokens() {
    return {"4k", "5k", "6k", "7k", "8k", "9k", "10k", "16k"};
}

std::vector<std::string> supported_skin_color_tokens() {
    std::vector<std::string> tokens;
    tokens.reserve(skin_palette_entries().size());
    for (const auto& entry : skin_palette_entries()) {
        tokens.emplace_back(entry.token);
    }
    return tokens;
}

std::string normalize_skin_source_token(std::string_view token) {
    const std::string normalized = to_lower_ascii(std::string(token));
    if (normalized == "osu" || normalized == "osu-mania" || normalized == "osu_mania") {
        return "osu";
    }
    if (normalized == "lr2" || normalized == "lunaticrave2" || normalized == "lr2skin") {
        return "lr2";
    }
    return "native";
}

std::string normalize_skin_lr2_resolution_mode_token(std::string_view token) {
    const std::string normalized = to_lower_ascii(std::string(token));
    if (normalized == "sd" || normalized == "hd" || normalized == "fhd") {
        return normalized;
    }
    return "auto";
}

std::string normalize_skin_color_token(std::string_view token) {
    const std::string normalized = to_lower_ascii(std::string(token));
    for (const auto& entry : skin_palette_entries()) {
        if (normalized == entry.token) {
            return std::string(entry.token);
        }
    }
    return "ice";
}

std::string normalize_skin_note_shape_token(std::string_view token) {
    const std::string normalized = to_lower_ascii(std::string(token));
    if (normalized == "circle") {
        return "circle";
    }
    return "rect";
}

std::string skin_note_shape_label(std::string_view token) {
    const std::string normalized = normalize_skin_note_shape_token(token);
    if (normalized == "circle") {
        return "Circle";
    }
    return "Rect";
}

std::string skin_color_label(std::string_view token) {
    const std::string normalized = normalize_skin_color_token(token);
    for (const auto& entry : skin_palette_entries()) {
        if (normalized == entry.token) {
            return entry.label;
        }
    }
    return "Ice";
}

uint32_t skin_color_rgb(std::string_view token) {
    const std::string normalized = normalize_skin_color_token(token);
    for (const auto& entry : skin_palette_entries()) {
        if (normalized == entry.token) {
            return entry.rgb;
        }
    }
    return 0xF6F8FF;
}

std::vector<double> resolved_skin_lane_width_scales(const SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    std::vector<double> resolved = default_skin_scale_vector(
        static_cast<std::size_t>(lane_count_for_skin_mode_token(normalized)),
        kLaneWidthScaleDefault);
    const auto it = skin.lane_width_scales.find(normalized);
    if (it == skin.lane_width_scales.end()) {
        return resolved;
    }
    return sanitize_skin_scale_vector(
        normalized,
        it->second,
        kLaneWidthScaleDefault,
        kLaneWidthScaleMin,
        kLaneWidthScaleMax,
        false);
}

double resolved_skin_note_width_scale(const SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    double resolved = std::clamp(skin.note_width_scale, kNoteWidthScaleMin, kNoteWidthScaleMax);
    const auto it = skin.note_width_scales.find(normalized);
    if (it == skin.note_width_scales.end() || !std::isfinite(it->second)) {
        return resolved;
    }
    return std::clamp(it->second, kNoteWidthScaleMin, kNoteWidthScaleMax);
}

std::vector<double> resolved_skin_lane_spacing_scales(const SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    std::vector<double> resolved =
        default_skin_scale_vector(lane_gap_count_for_skin_mode_token(normalized), kLaneSpacingScaleDefault);
    const auto it = skin.lane_spacing_scales.find(normalized);
    if (it == skin.lane_spacing_scales.end()) {
        return resolved;
    }
    return sanitize_skin_scale_vector(
        normalized,
        it->second,
        kLaneSpacingScaleDefault,
        kLaneSpacingScaleMin,
        kLaneSpacingScaleMax,
        true);
}

double resolved_skin_note_height_scale(const SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    double resolved = std::clamp(skin.note_height_scale, kNoteHeightScaleMin, kNoteHeightScaleMax);
    const auto it = skin.note_height_scales.find(normalized);
    if (it == skin.note_height_scales.end() || !std::isfinite(it->second)) {
        return resolved;
    }
    return std::clamp(it->second, kNoteHeightScaleMin, kNoteHeightScaleMax);
}

double resolved_skin_lane_divider_width_scale(const SkinConfig& skin, std::string_view key_mode) {
    static_cast<void>(key_mode);
    return std::clamp(
        skin.lane_divider_width_scale,
        kLaneDividerWidthScaleMin,
        kLaneDividerWidthScaleMax);
}

double resolved_skin_lane_center_gap_scale(const SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    double resolved = std::clamp(
        skin.lane_center_gap_scale,
        kLaneCenterGapScaleMin,
        kLaneCenterGapScaleMax);
    const auto it = skin.lane_center_gap_scales.find(normalized);
    if (it == skin.lane_center_gap_scales.end() || !std::isfinite(it->second)) {
        return resolved;
    }
    return std::clamp(it->second, kLaneCenterGapScaleMin, kLaneCenterGapScaleMax);
}

std::vector<std::string> default_skin_lane_colors(std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    const auto& defaults = default_skin_lane_color_map();
    const auto it = defaults.find(normalized);
    if (it != defaults.end()) {
        return it->second;
    }
    return defaults.at("10k");
}

std::vector<std::string> resolved_skin_lane_colors(const SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = normalize_skin_mode_token(key_mode);
    std::vector<std::string> resolved = default_skin_lane_colors(normalized);
    const auto it = skin.lane_colors.find(normalized);
    if (it == skin.lane_colors.end()) {
        return resolved;
    }

    const int lane_count = lane_count_for_skin_mode_token(normalized);
    if (lane_count <= 0) {
        return resolved;
    }

    const auto& custom = it->second;
    for (int lane = 0; lane < lane_count && lane < static_cast<int>(resolved.size()); ++lane) {
        if (lane < static_cast<int>(custom.size())) {
            resolved[static_cast<std::size_t>(lane)] = normalize_skin_color_token(custom[static_cast<std::size_t>(lane)]);
        }
    }
    return resolved;
}

RuntimeConfig ConfigLoader::defaults() const {
    RuntimeConfig config;
    config.audio.sample_rate = 44100;
    config.audio.frames_per_buffer = 256;
    config.audio.periods = 3;
    config.audio.exclusive_mode = true;
    config.audio.use_mmcss = true;
    config.audio.affinity_core = -1;

    config.audio_ui.preset = "high";
    config.audio_ui.bms_keysound_policy = "follow";
    config.audio_ui.background_sound_enabled = true;
    config.audio_ui.master_volume = 1.0;
    config.audio_ui.bgm_volume = 0.75;
    config.audio_ui.keysound_volume = 1.0;

    config.input.backend = "polling";
    config.input.rawinput = true;
    config.input.use_qpc = true;
    config.input.grab = false;
    config.input.queue_size = 2048;
    config.input.polling_hz = 1000;
    config.input.judgement_hz = 4000;
    config.input.debounce_ms = 8.0;

    config.judge = {};
    config.speed.rate = 1.0;
    config.speed.hi_speed = 3.0;
    config.speed.target_scroll_bps = 380.0;

    config.gauge = {};

    config.graphics.display_mode = "borderless";
    config.graphics.resolution = "native";
    config.graphics.vsync = false;
    config.graphics.refresh_hz = 1050;
    config.graphics.performance_overlay = false;

    config.mode.format = "bms";
    config.mode.key_mode = "none";
    config.mode.gauge = "normal";
    config.mode.random = "off";
    config.mode.random_seed = 0;
    config.mode.mods.clear();
    config.mode.enable_osu_charts = false;
    config.mode.ghost_battle_enabled = true;
    config.mode.autoplay_enabled = false;
    config.mode.practice_no_fail_enabled = false;
    config.mode.song_index_profile = "safe";

    config.ui.language = "en";
    config.ui.result_tail_ms = 3000.0;
    config.ui.require_enter_to_exit = true;
    config.ui.favorite_chart_keys.clear();
    config.ui.collections.clear();
    config.ui.song_collection_filter = "all";

    config.skin = {};
    sanitize_skin_config(config.skin);
    config.input_offset_ms = 0.0;
    config.visual_offset_ms = 0.0;
    return config;
}

ConfigLoadResult ConfigLoader::load_profile(std::string_view profile_dir) const {
    ConfigLoadResult result;
    result.config = defaults();

    std::filesystem::path global_path("config");
    global_path /= "config.json";
    if (!load_config_file(global_path, result.config, result.error, result.warnings, false) && !result.error.empty()) {
        return result;
    }

    std::filesystem::path profile_path(profile_dir);
    profile_path /= "config.json";
    if (!load_config_file(profile_path, result.config, result.error, result.warnings, true)) {
        if (!result.error.empty()) {
            return result;
        }
        result.used_defaults = true;
    }

    result.migrated = ::tenriff::app::migrate_bms_first_runtime_config(result.config);
    apply_audio_preset(result.config);
    result.config.input.polling_hz = sanitize_polling_hz(result.config.input.polling_hz, result.warnings);
    result.config.input.judgement_hz = sanitize_judgement_hz(result.config.input.judgement_hz, result.warnings);
    result.config.input.debounce_ms = sanitize_input_debounce_ms(result.config.input.debounce_ms, result.warnings);
    result.config.graphics.refresh_hz = sanitize_refresh_hz(result.config.graphics.refresh_hz, result.warnings);
    result.config.graphics.resolution = normalize_resolution_preset(result.config.graphics.resolution);
    result.config.ui.language = normalize_ui_language(result.config.ui.language);
    return result;
}

bool ConfigLoader::save_profile(std::string_view profile_dir, const RuntimeConfig& config, std::string* error) const {
    std::filesystem::path path(profile_dir);
    std::filesystem::create_directories(path);
    path /= "config.json";

    JsonValue root_value = build_json_root(config);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) {
            *error = "Failed to open config.json for writing.";
        }
        return false;
    }
    out << json_stringify(root_value, 2);
    return true;
}

bool ConfigLoader::save_global(const RuntimeConfig& config, std::string* error) const {
    std::filesystem::path path("config");
    std::filesystem::create_directories(path);
    path /= "config.json";

    JsonValue root_value = build_json_root(config);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) {
            *error = "Failed to open global config.json for writing.";
        }
        return false;
    }

    out << json_stringify(root_value, 2);
    return true;
}

}  // namespace tenriff::config
