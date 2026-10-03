#pragma once

#include "app/NativeMenuSkin.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace tenriff::render {

inline float native_skin_metric_minimum(const char* key) {
    const std::string_view slot(key);
    // Style ratios and square corners can be zero. Do not force the new tint
    // defaults (0.143/0.234) up to the dimension minimum of one.
    if (slot == "options_grid.tint" || slot == "options_grid.selected_tint" ||
        slot == "options_grid.radius") return 0.0f;
    if (slot == "icon.stroke_width" || slot == "options_grid.border_width" ||
        slot == "options_grid.selected_border_width") return 0.5f;
    if (slot.find("line_height") != std::string_view::npos) return 8.0f;
    if (slot.find("gap") != std::string_view::npos) return 0.0f;
    // Named menu metrics are dimensions or positive anchors. Signed movement
    // belongs in rect deltas, which cannot enter row-count divisors.
    return 1.0f;
}

inline float native_skin_number(const std::unordered_map<std::string, float>& values,
                                const char* key, float fallback, float minimum, float maximum) {
    if (values.empty()) return fallback;
    const auto it = values.find(key);
    return it == values.end() || !std::isfinite(it->second)
        ? fallback : std::clamp(it->second, minimum, maximum);
}

// These are deltas, not absolute rectangles: editing a repeated card style keeps
// each card on its own row. Hit testing receives the same adjusted rectangle.
inline std::array<float, 4> native_skin_rect(const app::NativeMenuSkinStyle& style,
                                           const char* key, std::array<float, 4> rect) {
    if (style.rects.empty()) return rect;
    const auto it = style.rects.find(key);
    if (it == style.rects.end()) return rect;
    const auto& delta = it->second;
    for (float value : delta) if (!std::isfinite(value)) return rect;
    if (delta == std::array<float, 4>{}) return rect;
    const float width = std::max(1.0f, rect[2] - rect[0] + delta[2]);
    const float height = std::max(1.0f, rect[3] - rect[1] + delta[3]);
    rect[0] += delta[0]; rect[1] += delta[1];
    rect[2] = rect[0] + width; rect[3] = rect[1] + height;
    return rect;
}

} // namespace tenriff::render
