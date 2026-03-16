#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "audio/AudioConfig.h"
#include "game/GaugeManager.h"

namespace tenriff::config {

inline constexpr double kJudgementLinePositionMin = 0.55;
inline constexpr double kJudgementLinePositionMax = 0.86;
inline constexpr double kJudgementLinePositionDefault = 0.82;
inline constexpr double kNoteWidthScaleMin = 0.50;
inline constexpr double kNoteWidthScaleMax = 1.40;
inline constexpr double kNoteHeightScaleMin = 0.50;
inline constexpr double kNoteHeightScaleMax = 2.00;
inline constexpr double kHoldBodyWidthScaleMin = 0.50;
inline constexpr double kHoldBodyWidthScaleMax = 1.20;
inline constexpr double kHoldBodyWidthScaleDefault = 0.60;
inline constexpr double kComboPositionMin = 0.10;
inline constexpr double kComboPositionMax = 0.78;
inline constexpr double kComboPositionDefault = 0.24;

struct JudgeConfig {
    double pg_ms = 15.5;
    double gr_ms = 31.0;
    double gd_ms = 55.0;
    double bd_ms = 200.0;
    double indirect_miss_ms = 500.0;
    double hold_grace_ms = 35.0;
    double hold_break_ms = 100.0;
    double mask_ms = 30.0;
};

struct SpeedConfig {
    double rate = 1.0;
    double hi_speed = 3.0;
    double target_scroll_bps = 380.0;
};

struct GraphicsConfig {
    std::string display_mode = "borderless";
    std::string resolution = "native";
    bool vsync = true;
    int refresh_hz = 1050;
    bool performance_overlay = false;
};

struct AudioUiConfig {
    std::string preset = "high";
    std::string bms_keysound_policy = "follow";
    double master_volume = 1.0;
    double bgm_volume = 0.75;
    double keysound_volume = 1.0;
};

struct UiConfig {
    double result_tail_ms = 500.0;
    bool require_enter_to_exit = true;
    std::string active_song_source;
    std::vector<std::string> recent_song_sources;
};

struct SkinConfig {
    std::string note_shape = "rect";
    bool note_border_enabled = true;
    double judgement_line_position = kJudgementLinePositionDefault;
    double combo_position = kComboPositionDefault;
    double note_width_scale = 1.0;
    double note_height_scale = 1.0;
    double hold_body_width_scale = kHoldBodyWidthScaleDefault;
    std::unordered_map<std::string, double> note_width_scales;
    std::unordered_map<std::string, double> note_height_scales;
    std::unordered_map<std::string, std::vector<std::string>> lane_colors;
};

struct InputConfig {
    std::string backend = "polling";
    bool rawinput = true;
    bool use_qpc = true;
    bool grab = false;
    std::size_t queue_size = 2048;
    int polling_hz = 1000;
    double debounce_ms = 8.0;
};

struct ModeConfig {
    std::string format = "auto";
    std::string key_mode = "auto";
    std::string gauge = "normal";
    std::string random = "off";
    uint32_t random_seed = 0;
    bool enable_osu_charts = false;
    std::string song_index_profile = "safe";
};

struct RuntimeConfig {
    audio::AudioConfig audio;
    AudioUiConfig audio_ui;
    InputConfig input;
    JudgeConfig judge;
    SpeedConfig speed;
    game::GaugeConfig gauge;
    GraphicsConfig graphics;
    ModeConfig mode;
    UiConfig ui;
    SkinConfig skin;
    double input_offset_ms = 0.0;
    double visual_offset_ms = 0.0;
};

struct ConfigLoadResult {
    RuntimeConfig config;
    std::vector<std::string> warnings;
    std::string error;
    bool used_defaults = false;

    [[nodiscard]] bool success() const { return error.empty(); }
};

class ConfigLoader {
public:
    [[nodiscard]] RuntimeConfig defaults() const;

    [[nodiscard]] ConfigLoadResult load_profile(std::string_view profile_dir) const;

    bool save_profile(std::string_view profile_dir, const RuntimeConfig& config, std::string* error = nullptr) const;

    bool save_global(const RuntimeConfig& config, std::string* error = nullptr) const;
};

[[nodiscard]] std::string normalize_skin_mode_token(std::string_view key_mode);
[[nodiscard]] std::string normalize_song_index_profile_token(std::string_view token);
[[nodiscard]] std::vector<std::string> supported_skin_mode_tokens();
[[nodiscard]] std::vector<std::string> supported_skin_color_tokens();
[[nodiscard]] std::string normalize_skin_color_token(std::string_view token);
[[nodiscard]] std::string normalize_skin_note_shape_token(std::string_view token);
[[nodiscard]] std::string skin_note_shape_label(std::string_view token);
[[nodiscard]] std::string skin_color_label(std::string_view token);
[[nodiscard]] uint32_t skin_color_rgb(std::string_view token);
[[nodiscard]] double resolved_skin_note_width_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_note_height_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<std::string> default_skin_lane_colors(std::string_view key_mode);
[[nodiscard]] std::vector<std::string> resolved_skin_lane_colors(const SkinConfig& skin, std::string_view key_mode);

}  // namespace tenriff::config
