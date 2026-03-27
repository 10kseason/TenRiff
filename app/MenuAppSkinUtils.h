#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "config/Config.h"
#include "render/MenuWindow.h"

namespace tenriff::app {

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
    if (value == "8k") {
        return "8K";
    }
    if (value == "9k") {
        return "9K";
    }
    if (value == "10k") {
        return "10K";
    }
    if (value == "16k") {
        return "16K";
    }
    return "None";
}

inline std::string normalize_skin_edit_mode(std::string value) {
    value = config::normalize_skin_mode_token(value);
    if (value == "4k" || value == "5k" || value == "6k" || value == "7k" || value == "8k" ||
        value == "9k" || value == "10k" || value == "16k") {
        return value;
    }
    return "10k";
}

inline std::string cycle_skin_edit_mode(std::string_view current, int direction) {
    static constexpr const char* kSkinModes[] = {"4k", "5k", "6k", "7k", "8k", "9k", "10k", "16k"};
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
    if (normalized == "4k") {
        return 4;
    }
    return 10;
}

inline std::string lane_display_label(int lane_index) {
    return "Lane " + std::to_string(std::max(1, lane_index + 1));
}

inline std::string skin_source_label(std::string_view value) {
    const std::string normalized = config::normalize_skin_source_token(value);
    if (normalized == "osu") {
        return "osu!mania";
    }
    if (normalized == "lr2") {
        return "LR2";
    }
    return "Native";
}

inline std::string cycle_skin_source(std::string_view value, int direction) {
    static constexpr const char* kSources[] = {"native", "osu", "lr2"};
    int index = 0;
    const std::string normalized = config::normalize_skin_source_token(value);
    for (int i = 0; i < 3; ++i) {
        if (normalized == kSources[i]) {
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
    return kSources[index];
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
