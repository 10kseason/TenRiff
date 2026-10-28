#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "render/NativeGameplayOverrides.h"

namespace tenriff::render {

inline float gameplay_gauge_width_scale(double scale) {
    return std::isfinite(scale) ? static_cast<float>(std::clamp(scale, 1.0, 1.5)) : 1.0f;
}

// Grow away from the field: a left-side gauge retains its right edge. Height,
// fill ratio and judgement geometry do not participate in the width control.
inline std::array<float, 4> scale_gameplay_gauge_rect(
    std::array<float, 4> rect, double scale, bool grow_left) {
    const float extra = (rect[2] - rect[0]) * (gameplay_gauge_width_scale(scale) - 1.0f);
    if (grow_left) rect[0] -= extra;
    else rect[2] += extra;
    return rect;
}

inline std::array<float, 4> gameplay_gauge_rect(const app::NativeGameplaySkinStyle& style,
    float surface_left, float top, float bottom, float base_width, double scale, bool grow_left) {
    // Surface layout already reserves the wider default bar. Recover its normal
    // anchor before applying authored additive offsets, then scale exactly once.
    const float normal_left = surface_left +
        (grow_left ? base_width * (gameplay_gauge_width_scale(scale) - 1.0f) : 0.0f);
    const auto authored = native_gameplay_rect(style, "gauge",
        {normal_left, top, normal_left + base_width, bottom});
    return scale_gameplay_gauge_rect(authored, scale, grow_left);
}

inline std::array<float, 2> gameplay_battle_drag_handle_x(float player_right, float ghost_left,
    float normal_width, float normal_gap) {
    const float gap = std::max(0.0f, ghost_left - player_right);
    const float width = std::min(normal_width, std::max(0.0f, gap - normal_gap));
    const float left = player_right + (gap - width) * 0.5f;
    return {left, left + width};
}

}  // namespace tenriff::render
