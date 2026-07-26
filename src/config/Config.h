#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "audio/AudioConfig.h"
#include "game/GaugeManager.h"

namespace tenriff::config {

inline constexpr double kJudgementLinePositionMin = 0.00;
inline constexpr double kJudgementLinePositionMax = 1.00;
inline constexpr double kJudgementLinePositionDefault = 0.82;
inline constexpr double kLaneWidthScaleMin = 0.50;
inline constexpr double kLaneWidthScaleMax = 1.75;
inline constexpr double kLaneWidthScaleDefault = 1.00;
inline constexpr double kNoteWidthScaleMin = 0.50;
inline constexpr double kNoteWidthScaleMax = 1.40;
inline constexpr double kLaneSpacingScaleMin = 0.00;
inline constexpr double kLaneSpacingScaleMax = 2.00;
inline constexpr double kLaneSpacingScaleDefault = 0.00;
inline constexpr double kNoteHeightScaleMin = 0.50;
inline constexpr double kNoteHeightScaleMax = 4.00;
inline constexpr double kLaneDividerWidthScaleMin = 0.00;
inline constexpr double kLaneDividerWidthScaleMax = 2.00;
inline constexpr double kLaneDividerWidthScaleDefault = 1.00;
inline constexpr double kLaneCenterGapScaleMin = 0.00;
inline constexpr double kLaneCenterGapScaleMax = 2.00;
inline constexpr double kLaneCenterGapScaleDefault = 0.00;
inline constexpr double kHoldBodyWidthScaleMin = 0.50;
inline constexpr double kHoldBodyWidthScaleMax = 1.20;
inline constexpr double kHoldBodyWidthScaleDefault = 0.60;
inline constexpr double kComboPositionMin = 0.10;
inline constexpr double kComboPositionMax = 0.78;
inline constexpr double kComboPositionDefault = 0.24;
inline constexpr double kSkinLaneBackgroundOpacityMin = 0.00;
inline constexpr double kSkinLaneBackgroundOpacityMax = 0.45;
inline constexpr double kSkinLaneBackgroundOpacityDefault = 0.18;
inline constexpr double kSkinVisualOpacityMin = 0.20;
inline constexpr double kSkinVisualOpacityMax = 1.00;
inline constexpr double kSkinVisualOpacityDefault = 0.96;
inline constexpr double kSkinNoteOutlineOpacityMin = 0.00;
inline constexpr double kSkinNoteOutlineOpacityMax = 1.00;
inline constexpr double kSkinNoteOutlineOpacityDefault = 0.78;
inline constexpr double kSkinHoldBodyOpacityMin = 0.05;
inline constexpr double kSkinHoldBodyOpacityMax = 0.60;
inline constexpr double kSkinHoldBodyOpacityDefault = 0.24;

struct JudgeConfig {
    double pg_ms = 15.5;
    double gr_ms = 31.0;
    double gd_ms = 75.0;
    double bd_ms = 340.0;
    double indirect_miss_ms = 340.0;
    double hold_grace_ms = 80.0;
    double hold_break_ms = 200.0;
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
    bool vsync = false;
    int refresh_hz = 300;
    bool performance_overlay = false;
    std::string background_upscale_mode = "lunasr";
};

struct AudioUiConfig {
    std::string preset = "high";
    std::string bms_keysound_policy = "follow";
    bool background_sound_enabled = true;
    double master_volume = 1.0;
    double bgm_volume = 0.75;
    double keysound_volume = 1.0;
};

struct UiConfig {
    std::string language = "en";
    double result_tail_ms = 500.0;
    bool require_enter_to_exit = true;
    std::string active_song_source;
    std::vector<std::string> recent_song_sources;
    std::vector<std::string> favorite_chart_keys;
    std::unordered_map<std::string, std::vector<std::string>> collections;
    std::string song_collection_filter = "all";
};

struct SkinConfig {
    std::string source = "native";
    std::string osu_skin_name;
    std::string lr2_skin_name;
    std::string lr2_resolution_mode = "auto";
    std::string visual_preset = "tenriff";
    std::string note_shape = "rect";
    bool note_border_enabled = true;
    bool preserve_note_image_aspect_ratio = false;
    bool show_lane_dividers = true;
    bool show_judgement_line = true;
    bool show_gear_boundary_line = false;
    bool hold_tail_taper_enabled = false;
    bool judgement_line_glow_enabled = true;
    bool key_pulse_enabled = true;
    std::string key_label_position = "bottom";
    double judgement_line_position = kJudgementLinePositionDefault;
    double combo_position = kComboPositionDefault;
    double lane_background_opacity = kSkinLaneBackgroundOpacityDefault;
    double visual_opacity = kSkinVisualOpacityDefault;
    double note_outline_opacity = kSkinNoteOutlineOpacityDefault;
    double hold_body_opacity = kSkinHoldBodyOpacityDefault;
    std::unordered_map<std::string, std::vector<double>> lane_width_scales;
    double note_width_scale = 1.0;
    std::unordered_map<std::string, double> note_width_scales;
    std::unordered_map<std::string, std::vector<double>> lane_spacing_scales;
    double note_height_scale = 1.8;
    double lane_divider_width_scale = kLaneDividerWidthScaleDefault;
    double lane_center_gap_scale = kLaneCenterGapScaleDefault;
    double hold_body_width_scale = kHoldBodyWidthScaleDefault;
    std::unordered_map<std::string, double> note_height_scales;
    std::unordered_map<std::string, double> lane_divider_width_scales;
    std::unordered_map<std::string, double> lane_center_gap_scales;
    std::unordered_map<std::string, std::vector<std::string>> lane_colors;
};

struct InputConfig {
    std::string backend = "rawinput";
    bool rawinput = true;
    bool use_qpc = true;
    bool grab = false;
    std::size_t queue_size = 2048;
    int polling_hz = 1000;
    int judgement_hz = 4000;
    double debounce_ms = 8.0;
};

struct ModeConfig {
    std::string format = "auto";
    std::string key_mode = "auto";
    std::string gauge = "normal";
    std::string random = "off";
    uint32_t random_seed = 0;
    std::vector<std::string> mods;
    bool enable_osu_charts = false;
    bool ghost_battle_enabled = false;
    bool autoplay_enabled = false;
    bool practice_no_fail_enabled = false;
    bool one_miss_fail_enabled = false;
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
    bool migrated = false;

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
[[nodiscard]] std::string normalize_ui_language_token(std::string_view token);
[[nodiscard]] std::string normalize_song_index_profile_token(std::string_view token);
[[nodiscard]] std::string normalize_background_upscale_mode(std::string_view token);
[[nodiscard]] std::vector<std::string> supported_skin_mode_tokens();
[[nodiscard]] std::vector<std::string> supported_skin_color_tokens();
[[nodiscard]] std::string normalize_skin_source_token(std::string_view token);
[[nodiscard]] std::string normalize_skin_lr2_resolution_mode_token(std::string_view token);
[[nodiscard]] std::string normalize_skin_visual_preset_token(std::string_view token);
[[nodiscard]] std::vector<std::string> supported_skin_visual_preset_tokens();
[[nodiscard]] std::string skin_visual_preset_label(std::string_view token);
void apply_skin_visual_preset(SkinConfig& skin, std::string_view token);
[[nodiscard]] std::string normalize_skin_key_label_position_token(std::string_view token);
[[nodiscard]] std::string skin_key_label_position_label(std::string_view token);
[[nodiscard]] std::string normalize_skin_color_token(std::string_view token);
[[nodiscard]] std::string normalize_skin_note_shape_token(std::string_view token);
[[nodiscard]] std::string skin_note_shape_label(std::string_view token);
[[nodiscard]] std::string skin_color_label(std::string_view token);
[[nodiscard]] uint32_t skin_color_rgb(std::string_view token);
[[nodiscard]] std::vector<double> resolved_skin_lane_width_scales(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_note_width_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<double> resolved_skin_lane_spacing_scales(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_note_height_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_lane_divider_width_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_lane_center_gap_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<std::string> default_skin_lane_colors(std::string_view key_mode);
[[nodiscard]] std::vector<std::string> resolved_skin_lane_colors(const SkinConfig& skin, std::string_view key_mode);

}  // namespace tenriff::config
