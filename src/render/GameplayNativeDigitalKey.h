#pragma once

#include "render/NativeGameplayOverrides.h"
#include <algorithm>
#include <cmath>

namespace tenriff::render {

// Visual travel follows physical key state, independently of judgement/hit flashes.
// Exponential release is independent of the render cap; gaps after focus loss reset it.
inline float advance_native_key_travel(float current, bool pressed, double elapsed_seconds, float press_response = 100.0f, float release_response = 24.0f) {
    const float target = pressed ? 1.0f : 0.0f;
    if (!std::isfinite(current) || !std::isfinite(elapsed_seconds) || elapsed_seconds > 0.25)
        return target;
    if (!std::isfinite(press_response)) press_response=100.0f;
    if (!std::isfinite(release_response)) release_response=24.0f;
    const float start = std::clamp(current, 0.0f, 1.0f);
    const float response = static_cast<float>(1.0 - std::exp(-std::max(0.0, elapsed_seconds) * static_cast<double>(pressed ? std::clamp(press_response,1.0f,500.0f) : std::clamp(release_response,1.0f,500.0f))));
    return std::clamp(start + (target - start) * response, 0.0f, 1.0f);
}

struct NativeKeyBounds { float left, top, right, bottom; };

inline NativeKeyBounds native_key_bounds(float left, float right, float field_top,
                                         float field_bottom, float gear_top, const app::NativeGameplaySkinStyle& style = {}) {
    const float width = std::max(0.0f, right - left);
    const float inset = std::min(native_gameplay_number(style,"key_inset"), width * native_gameplay_number(style,"key_inset_ratio"));
    const float bottom = std::max(field_top, field_bottom - native_gameplay_number(style,"key_bottom_gap"));
    const float height = std::clamp(bottom - gear_top, std::min(native_gameplay_number(style,"key_min_height"),native_gameplay_number(style,"key_max_height")), std::max(native_gameplay_number(style,"key_min_height"),native_gameplay_number(style,"key_max_height")));
    return {left + inset, std::max(field_top, bottom - height), right - inset, bottom};
}

struct NativeDigitalKeyVisual {
    float press_offset = 0.0f;
    float glitch_strength = 0.0f;
};

// Imported LR2 pressed frames are hit pulses, not physical key-down states.
// Otherwise an LN hold can leave hold-head art parked on the judgement line.
inline bool should_use_imported_pressed_key(float activity) {
    return std::clamp(activity, 0.0f, 1.0f) > 0.05f;
}

inline NativeDigitalKeyVisual resolve_native_digital_key_visual(bool pressed,
                                                                 float activity,
                                                                 float key_height) {
    const float clamped_activity = std::clamp(activity, 0.0f, 1.0f);
    const float safe_height = std::max(0.0f, key_height);
    return NativeDigitalKeyVisual{
        pressed ? std::clamp(safe_height * 0.075f, 2.0f, 6.0f) : 0.0f,
        clamped_activity * clamped_activity,
    };
}

}  // namespace tenriff::render
