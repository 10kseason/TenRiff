#pragma once

#include <algorithm>
#include <cmath>

namespace tenriff::render {

struct GameplayGearRect {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

inline float gameplay_gear_scale_multiplier(double note_width_scale) {
    if (!std::isfinite(note_width_scale)) {
        return 2.0f;
    }
    return std::clamp(static_cast<float>(note_width_scale) * 2.0f, 1.25f, 2.8f);
}

// LR2 Gear art is one authored panel. Fit it as one bottom-anchored rectangle
// instead of stretching its lane slices independently across the playfield.
inline GameplayGearRect fit_gameplay_gear_rect(const GameplayGearRect& bounds,
                                               float source_width,
                                               float source_height,
                                               float scale_multiplier = 1.0f) {
    const float available_width = std::max(0.0f, bounds.right - bounds.left);
    const float available_height = std::max(0.0f, bounds.bottom - bounds.top);
    if (!std::isfinite(source_width) || !std::isfinite(source_height) ||
        source_width <= 0.0f || source_height <= 0.0f ||
        available_width <= 0.0f || available_height <= 0.0f) {
        return bounds;
    }

    const float contain_scale = std::min(available_width / source_width,
                                         available_height / source_height);
    const float safe_multiplier = std::isfinite(scale_multiplier)
                                      ? std::clamp(scale_multiplier, 1.0f, 4.0f)
                                      : 1.0f;
    const float scale = std::min(contain_scale * safe_multiplier,
                                 available_width / source_width);
    const float width = source_width * scale;
    const float height = source_height * scale;
    const float left = bounds.left + (available_width - width) * 0.5f;
    return GameplayGearRect{
        left,
        bounds.bottom - height,
        left + width,
        bounds.bottom,
    };
}

}  // namespace tenriff::render