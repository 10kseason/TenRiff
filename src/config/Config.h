#pragma once

#include <cstddef>
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
inline constexpr double kVisualOffsetMin = -500.0;
inline constexpr double kVisualOffsetMax = 500.0;
inline constexpr double kSoundOffsetMin = -500.0;
inline constexpr double kSoundOffsetMax = 500.0;
inline constexpr double kGameplayFieldOffsetXMin = -720.0;
inline constexpr double kGameplayFieldOffsetXMax = 720.0;
inline constexpr double kGameplayFieldOffsetXDefault = 0.0;
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
inline constexpr double kNoteDividerGapPxMin = 0.0;
inline constexpr double kNoteDividerGapPxMax = 40.0;
inline constexpr double kNoteDividerGapPxDefault = 12.0;
inline constexpr double kNoteDividerGapPxStep = 1.0;
inline constexpr double kLaneCenterGapScaleMin = 0.00;
inline constexpr double kLaneCenterGapScaleMax = 2.00;
inline constexpr double kLaneCenterGapScaleDefault = 0.00;
inline constexpr double kHoldBodyWidthScaleMin = 0.50;
inline constexpr double kHoldBodyWidthScaleMax = 1.20;
inline constexpr double kHoldBodyWidthScaleDefault = 1.00;
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
inline constexpr double kSkinHoldBodyOpacityMax = 1.00;
inline constexpr double kSkinHoldBodyOpacityDefault = 1.00;
inline constexpr double kSkinKeyPulseBrightnessMin = 0.00;
inline constexpr double kSkinKeyPulseBrightnessMax = 1.00;
inline constexpr double kSkinKeyPulseBrightnessDefault = 1.00;
inline constexpr double kPacemakerAccuracyMin = 0.0;
inline constexpr double kPacemakerAccuracyMax = 100.0;
inline constexpr double kPacemakerAccuracyDefault = 90.0;
inline constexpr int64_t kPacemakerScoreMin = 0;
inline constexpr int64_t kPacemakerScoreMax = 10'000;
inline constexpr int64_t kPacemakerScoreDefault = 8'000;

struct JudgeConfig {
    double pg_ms = 20.0;
    double gr_ms = 65.0;
    double gd_ms = 115.0;
    double bd_ms = 210.0;
    double indirect_miss_ms = 210.0;
    // Runtime mode policy: Judge Hard turns an unplayed object into an indirect POOR.
    bool indirect_miss_enabled = false;
    double hold_grace_ms = 80.0;
    double hold_break_ms = 200.0;
    double mask_ms = 30.0;
};

struct SpeedConfig {
    double rate = 1.0;
    double hi_speed = 10.0;
    double target_scroll_bps = 380.0;
};

struct GraphicsConfig {
    std::string display_mode = "borderless";
    std::string resolution = "native";
    bool vsync = false;
    // -1 follows the active display, 0 removes gameplay pacing when VSync is off.
    int refresh_hz = -1;
    bool performance_overlay = false;
    bool bga_enabled = true;
    std::string background_upscale_mode = "off";
    std::string background_upscale_model_path;
    bool background_upscale_prefer_npu = false;
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
    std::string profile_nickname;
    std::string profile_avatar_path;
    std::string language = "en";
    double result_tail_ms = 500.0;
    bool require_enter_to_exit = true;
    // Gameplay keeps the skin pointer on screen by default so players can drag the
    // playfield mid-song or run the game in a window alongside overlays.
    bool show_cursor_in_gameplay = true;
    std::string active_song_source;
    std::vector<std::string> recent_song_sources;
    std::string session_mix_lr2_course_path;
    std::vector<std::string> favorite_chart_keys;
    std::unordered_map<std::string, std::vector<std::string>> collections;
    std::string song_collection_filter = "all";
    int song_key_filter = 0;
    int song_level_min_filter = 0;
    int song_level_max_filter = 0;
    std::string difficulty_table_path;
    std::string difficulty_table_url;
    // Development default. Public 1.5.0 builds must replace this with the
    // deployed HTTPS records endpoint or expose it through server setup UI.
    std::string online_records_server_url = "http://127.0.0.1:27302";
};

struct SkinConfig {
    std::string source = "native";
    std::string tenriff_skin_name;
    std::string lr2_skin_name;
    std::string lr2_resolution_mode = "auto";
    // Visual placement for the single scratch lane in 7+1 SP charts. The
    // gameplay/replay lane identity remains canonical; only presentation moves.
    std::string scratch_position = "left";
    std::string visual_preset = "tenriff";
    std::string note_shape = "rect";
    bool note_border_enabled = true;
    // How note art fills its rect: "stretch" ignores the image aspect, "contain"
    // shrinks the image to fit, and "width" keeps the lane width and derives the
    // height from the image, which is what arrow and circle art needs.
    std::string note_image_aspect = "stretch";
    // Retained so builds older than the three-way option keep reading a sane value.
    bool preserve_note_image_aspect_ratio = false;
    bool show_lane_dividers = true;
    // Clearance in pixels between each note edge and the lane divider line. Zero
    // makes notes meet the dividers; the default reproduces the historic inset.
    double note_divider_gap_px = kNoteDividerGapPxDefault;
    bool show_judgement_line = true;
    bool show_gear_boundary_line = false;
    bool show_timing_feedback = true;
    bool show_hold_tail = false;
    bool hold_tail_taper_enabled = false;
    bool judgement_line_glow_enabled = true;
    bool key_pulse_enabled = true;
    // Hit-explosion brightness, 0.0 (off) .. 1.0. key_pulse_enabled is kept as the
    // on/off form older builds read, and is written as brightness > 0.
    double key_pulse_brightness = kSkinKeyPulseBrightnessDefault;
    // Built-in hit-burst material: prism | ring | spark.
    std::string hit_burst_style = "prism";
    std::string key_label_position = "bottom";
    // UI text font token: default | malgun | bahnschrift | consolas.
    std::string ui_font = "default";
    double judgement_line_position = kJudgementLinePositionDefault;
    // Horizontal gameplay-field offset in the renderer's 1920x1080 base space.
    double gameplay_field_offset_x = kGameplayFieldOffsetXDefault;
    double combo_position = kComboPositionDefault;
    double lane_background_opacity = kSkinLaneBackgroundOpacityDefault;
    bool black_playfield_enabled = true;
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
    // "off" preserves the per-lane palettes. A palette token overrides every
    // rendered lane without destroying those saved per-lane values.
    std::string single_color = "off";
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
    std::string key_mode = "auto";
    std::string key_conversion_algorithm = "krrcream";
    std::string key_conversion_nk2_preset = "native";
    std::string gauge = "normal";
    std::string random = "off";
    uint32_t random_seed = 0;
    std::vector<std::string> mods;
    bool ghost_battle_enabled = false;
    bool autoplay_enabled = false;
    bool practice_no_fail_enabled = false;
    bool one_miss_fail_enabled = false;
    std::string pacemaker_mode = "off";
    double pacemaker_target_accuracy = kPacemakerAccuracyDefault;
    int64_t pacemaker_target_score = kPacemakerScoreDefault;
    std::string song_index_profile = "safe";
    bool calculate_song_index_difficulty = false;
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
    double sound_offset_ms = 0.0;
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
[[nodiscard]] std::string normalize_skin_scratch_position_token(std::string_view token);
[[nodiscard]] std::string normalize_online_records_server_url(std::string_view value);
[[nodiscard]] std::string normalize_ui_language_token(std::string_view token);
[[nodiscard]] std::string normalize_song_index_profile_token(std::string_view token);
[[nodiscard]] std::string normalize_pacemaker_mode_token(std::string_view token);
[[nodiscard]] std::string normalize_profile_nickname(std::string_view value);
[[nodiscard]] std::string normalize_profile_avatar_path(std::string_view value);
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
[[nodiscard]] std::string normalize_skin_hit_burst_style_token(std::string_view token);
[[nodiscard]] std::string skin_hit_burst_style_label(std::string_view token);
[[nodiscard]] std::string normalize_skin_ui_font_token(std::string_view token);
[[nodiscard]] std::string skin_ui_font_label(std::string_view token);
[[nodiscard]] std::string normalize_skin_color_token(std::string_view token);
[[nodiscard]] std::string normalize_skin_single_color_token(std::string_view token);
[[nodiscard]] std::string normalize_skin_note_shape_token(std::string_view token);
[[nodiscard]] std::string skin_note_shape_label(std::string_view token);
[[nodiscard]] std::string normalize_skin_note_image_aspect_token(std::string_view token);
[[nodiscard]] std::string skin_note_image_aspect_label(std::string_view token);
[[nodiscard]] std::string skin_color_label(std::string_view token);
[[nodiscard]] std::string skin_single_color_label(std::string_view token);
[[nodiscard]] uint32_t skin_color_rgb(std::string_view token);
[[nodiscard]] std::vector<double> resolved_skin_lane_width_scales(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_note_width_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<double> resolved_skin_lane_spacing_scales(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_note_height_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_lane_divider_width_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] double resolved_skin_lane_center_gap_scale(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<std::string> default_skin_lane_colors(std::string_view key_mode);
[[nodiscard]] std::vector<std::string> resolved_skin_lane_palette(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<std::string> resolved_skin_lane_colors(const SkinConfig& skin, std::string_view key_mode);
[[nodiscard]] std::vector<std::string> resolved_skin_lane_colors_for_layout(
    const SkinConfig& skin,
    int lane_count,
    const int* scratch_lanes,
    std::size_t scratch_lane_count);

}  // namespace tenriff::config
