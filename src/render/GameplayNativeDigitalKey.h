#pragma once

#include <algorithm>

namespace tenriff::render {

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