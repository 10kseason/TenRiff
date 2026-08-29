#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "config/Config.h"
#include "render/MenuWindow.h"

namespace tenriff::app {

enum class SkinSettingsRowId {
    KeyMode,
    ScratchPosition,
    SkinSource,
    ImportedSkin,
    Lr2Resolution,
    ImportSkin,
    CreateSkin,
    OpenSkinFolder,
    ReloadSkin,
    TargetLane,
    TargetGap,
    LaneColor,
    SingleColor,
    NoteShape,
    NoteBorder,
    ImageAspect,
    LaneDividers,
    JudgementLine,
    GearBoundary,
    ShowHoldTail,
    LnTailTaper,
    VisualPreset,
    LaneBackgroundOpacity,
    VisualOpacity,
    NoteOutlineOpacity,
    LnBodyOpacity,
    JudgementLineGlow,
    HitBurstStyle,
    KeyPulse,
    KeyLabelPosition,
    JudgeLinePosition,
    LaneWidth,
    NoteWidth,
    LaneSpacing,
    DividerWidth,
    CenterGap,
    LnBodyWidth,
    NoteHeight,
    ComboY,
    BlackPlayfield,
    UiFont,
    VisualLatency,
    NoteGap,
    GameplayCursor,
    TimingFeedback,
    Back,
};

inline constexpr std::array<SkinSettingsRowId, 46> kSkinSettingsRowOrder = {
    SkinSettingsRowId::KeyMode,
    SkinSettingsRowId::ScratchPosition,
    SkinSettingsRowId::SkinSource,
    SkinSettingsRowId::ImportedSkin,
    SkinSettingsRowId::Lr2Resolution,
    SkinSettingsRowId::ImportSkin,
    SkinSettingsRowId::CreateSkin,
    SkinSettingsRowId::OpenSkinFolder,
    SkinSettingsRowId::ReloadSkin,
    SkinSettingsRowId::TargetLane,
    SkinSettingsRowId::TargetGap,
    SkinSettingsRowId::LaneColor,
    SkinSettingsRowId::SingleColor,
    SkinSettingsRowId::NoteShape,
    SkinSettingsRowId::NoteBorder,
    SkinSettingsRowId::ImageAspect,
    SkinSettingsRowId::LaneDividers,
    SkinSettingsRowId::JudgementLine,
    SkinSettingsRowId::GearBoundary,
    SkinSettingsRowId::ShowHoldTail,
    SkinSettingsRowId::LnTailTaper,
    SkinSettingsRowId::VisualPreset,
    SkinSettingsRowId::LaneBackgroundOpacity,
    SkinSettingsRowId::VisualOpacity,
    SkinSettingsRowId::NoteOutlineOpacity,
    SkinSettingsRowId::LnBodyOpacity,
    SkinSettingsRowId::JudgementLineGlow,
    SkinSettingsRowId::HitBurstStyle,
    SkinSettingsRowId::KeyPulse,
    SkinSettingsRowId::KeyLabelPosition,
    SkinSettingsRowId::JudgeLinePosition,
    SkinSettingsRowId::LaneWidth,
    SkinSettingsRowId::NoteWidth,
    SkinSettingsRowId::LaneSpacing,
    SkinSettingsRowId::DividerWidth,
    SkinSettingsRowId::CenterGap,
    SkinSettingsRowId::LnBodyWidth,
    SkinSettingsRowId::NoteHeight,
    SkinSettingsRowId::ComboY,
    SkinSettingsRowId::BlackPlayfield,
    SkinSettingsRowId::UiFont,
    SkinSettingsRowId::VisualLatency,
    SkinSettingsRowId::NoteGap,
    SkinSettingsRowId::GameplayCursor,
    SkinSettingsRowId::TimingFeedback,
    SkinSettingsRowId::Back,
};

struct SkinSettingsRows {
    bool lr2_source = false;

    [[nodiscard]] constexpr int count() const {
        return static_cast<int>(kSkinSettingsRowOrder.size()) - (lr2_source ? 0 : 1);
    }

    [[nodiscard]] constexpr int index_of(SkinSettingsRowId id) const {
        int index = 0;
        for (const auto candidate : kSkinSettingsRowOrder) {
            if (!lr2_source && candidate == SkinSettingsRowId::Lr2Resolution) continue;
            if (candidate == id) return index;
            ++index;
        }
        return -1;
    }
};

[[nodiscard]] inline constexpr int skin_settings_row_count(bool lr2_source) {
    return SkinSettingsRows{lr2_source}.count();
}

inline std::string on_off(bool value) {
    return value ? "On" : "Off";
}

inline int clamp_int(int value, int min_value, int max_value) {
    return std::max(min_value, std::min(max_value, value));
}

inline void append_menu_row(render::GenericMenuData& menu,
                            std::string label,
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
    row.activatable = activatable;
    row.adjustable = adjustable;
    row.increment_enabled = adjustable;
    row.decrement_enabled = adjustable;
    row.target_kind = target_kind;
    row.row_index = row_index;
    menu.rows.push_back(std::move(row));
}

inline void append_slider_menu_row(render::GenericMenuData& menu,
                                   std::string label,
                                   std::string value,
                                   double slider_ratio,
                                   bool selected,
                                   render::MenuHitTargetKind target_kind,
                                   int row_index) {
    render::MenuRowData row;
    row.label = std::move(label);
    row.value = std::move(value);
    row.selected = selected;
    row.adjustable = true;
    row.increment_enabled = true;
    row.decrement_enabled = true;
    row.slider = true;
    row.slider_ratio = std::clamp(slider_ratio, 0.0, 1.0);
    row.target_kind = target_kind;
    row.row_index = row_index;
    menu.rows.push_back(std::move(row));
}

inline std::string format_percent(double value) {
    const int percent = static_cast<int>(std::lround(std::clamp(value, 0.0, 2.0) * 100.0));
    return std::to_string(percent) + "%";
}

inline double clamp_step_value(double value, double min_value, double max_value, double step) {
    if (!std::isfinite(value)) {
        return min_value;
    }
    const double clamped = std::clamp(value, min_value, max_value);
    const double snapped = std::round(clamped / step) * step;
    return std::clamp(snapped, min_value, max_value);
}

inline std::string key_mode_label(const std::string& value) {
    if (value == "none" || value == "auto") {
        return "None";
    }
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
    if (config::normalize_skin_mode_token(value) == "7+1") {
        return "7+1 SP";
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
    if (value == "12k") {
        return "12K";
    }
    if (value == "14k") {
        return "14K";
    }
    if (value == "16k") {
        return "16K";
    }
    return "None";
}

inline std::string normalize_skin_edit_mode(std::string value) {
    value = config::normalize_skin_mode_token(value);
    if (value == "4k" || value == "5k" || value == "6k" || value == "7k" || value == "7+1" || value == "8k" ||
        value == "9k" || value == "10k" || value == "12k" || value == "14k" || value == "16k") {
        return value;
    }
    return "10k";
}

inline std::string cycle_skin_edit_mode(std::string_view current, int direction) {
    static constexpr const char* kSkinModes[] = {"4k", "5k", "6k", "7k", "7+1", "8k", "9k", "10k", "12k", "14k", "16k"};
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

inline int lane_count_for_skin_mode(std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    if (normalized == "7+1") {
        return 8;
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
    if (normalized == "12k") {
        return 12;
    }
    if (normalized == "14k") {
        return 14;
    }
    if (normalized == "16k") {
        return 16;
    }
    if (normalized == "4k") {
        return 4;
    }
    return 10;
}

inline std::string lane_display_label(int lane_index) {
    return "Lane " + std::to_string(std::max(1, lane_index + 1));
}

inline double slider_value_from_ratio(double ratio,
                                      double min_value,
                                      double max_value,
                                      double step) {
    const double safe_ratio = std::isfinite(ratio) ? std::clamp(ratio, 0.0, 1.0) : 0.0;
    return clamp_step_value(
        min_value + safe_ratio * (max_value - min_value), min_value, max_value, step);
}

inline std::string skin_lane_display_label(std::string_view mode,
                                           int lane_index,
                                           std::string_view scratch_position) {
    const int lane = std::max(1, lane_index + 1);
    if (config::normalize_skin_mode_token(mode) == "7+1") {
        const int scratch_lane =
            config::normalize_skin_scratch_position_token(scratch_position) == "right" ? 8 : 1;
        if (lane == scratch_lane) return "Scratch";
    }
    return lane_display_label(lane_index);
}

inline std::string skin_source_label(std::string_view value) {
    const std::string normalized = config::normalize_skin_source_token(value);
    if (normalized == "lr2") {
        return "LR2";
    }
    if (normalized == "tenriff") {
        return "TenRiff";
    }
    return "Native";
}

inline std::string cycle_skin_source(std::string_view value, int direction) {
    static constexpr std::array<std::string_view, 3> kSources = {"native", "tenriff", "lr2"};
    const std::string normalized = config::normalize_skin_source_token(value);
    int index = 0;
    for (int i = 0; i < static_cast<int>(kSources.size()); ++i) {
        if (kSources[static_cast<std::size_t>(i)] == normalized) {
            index = i;
            break;
        }
    }
    if (direction == 0) {
        return std::string(kSources[static_cast<std::size_t>(index)]);
    }
    index = (index + (direction < 0 ? -1 : 1) + static_cast<int>(kSources.size())) %
            static_cast<int>(kSources.size());
    return std::string(kSources[static_cast<std::size_t>(index)]);
}

inline std::string cycle_skin_visual_preset(std::string_view value, int direction) {
    const auto presets = config::supported_skin_visual_preset_tokens();
    int index = 0;
    const std::string normalized = config::normalize_skin_visual_preset_token(value);
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        if (presets[static_cast<std::size_t>(i)] == normalized) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = static_cast<int>(presets.size()) - 1;
    } else if (index >= static_cast<int>(presets.size())) {
        index = 0;
    }
    return presets[static_cast<std::size_t>(index)];
}

inline std::string cycle_skin_note_image_aspect(std::string_view value, int direction) {
    static constexpr const char* kAspects[] = {"stretch", "contain", "width"};
    constexpr int kCount = 3;
    int index = 0;
    const std::string normalized = config::normalize_skin_note_image_aspect_token(value);
    for (int i = 0; i < kCount; ++i) {
        if (normalized == kAspects[i]) {
            index = i;
            break;
        }
    }
    index = (index + direction % kCount + kCount) % kCount;
    return kAspects[index];
}

inline std::string cycle_skin_ui_font(std::string_view value, int direction) {
    static constexpr const char* kFonts[] = {"default", "malgun", "bahnschrift", "consolas"};
    constexpr int kCount = 4;
    int index = 0;
    const std::string normalized = config::normalize_skin_ui_font_token(value);
    for (int i = 0; i < kCount; ++i) {
        if (normalized == kFonts[i]) {
            index = i;
            break;
        }
    }
    index = (index + direction % kCount + kCount) % kCount;
    return kFonts[index];
}

inline std::string cycle_skin_key_label_position(std::string_view value, int direction) {
    static constexpr const char* kPositions[] = {"bottom", "top", "off"};
    int index = 0;
    const std::string normalized = config::normalize_skin_key_label_position_token(value);
    for (int i = 0; i < 3; ++i) {
        if (normalized == kPositions[i]) {
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
    return kPositions[index];
}

inline std::vector<std::string>& editable_skin_lane_colors(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& colors = skin.lane_colors[normalized];
    colors = config::resolved_skin_lane_colors(skin, normalized);
    return colors;
}

inline std::vector<double>& editable_skin_lane_width_scales(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& scales = skin.lane_width_scales[normalized];
    scales = config::resolved_skin_lane_width_scales(skin, normalized);
    return scales;
}

inline double& editable_skin_note_width_scale(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& scale = skin.note_width_scales[normalized];
    scale = config::resolved_skin_note_width_scale(skin, normalized);
    return scale;
}

inline std::vector<double>& editable_skin_lane_spacing_scales(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& scales = skin.lane_spacing_scales[normalized];
    scales = config::resolved_skin_lane_spacing_scales(skin, normalized);
    return scales;
}

inline double& editable_skin_note_height_scale(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& scale = skin.note_height_scales[normalized];
    scale = config::resolved_skin_note_height_scale(skin, normalized);
    return scale;
}

inline double& editable_skin_lane_divider_width_scale(config::SkinConfig& skin, std::string_view key_mode) {
    static_cast<void>(key_mode);
    skin.lane_divider_width_scale = std::clamp(
        skin.lane_divider_width_scale,
        config::kLaneDividerWidthScaleMin,
        config::kLaneDividerWidthScaleMax);
    return skin.lane_divider_width_scale;
}

inline double& editable_skin_lane_center_gap_scale(config::SkinConfig& skin, std::string_view key_mode) {
    const std::string normalized = config::normalize_skin_mode_token(key_mode);
    auto& scale = skin.lane_center_gap_scales[normalized];
    scale = config::resolved_skin_lane_center_gap_scale(skin, normalized);
    return scale;
}

}  // namespace tenriff::app
