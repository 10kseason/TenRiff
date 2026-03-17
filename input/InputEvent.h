#pragma once

#include <cstdint>

namespace tenriff::input {

enum class InputState : uint8_t {
    Released = 0,
    Pressed = 1,
};

// POD describing a single device event stamped on the input clock.
struct InputEvent {
    uint32_t keycode = 0;        // Platform scancode or evdev keycode.
    InputState state = InputState::Released;
    int64_t input_time_ns = 0;   // Monotonic nanoseconds on the input clock.
    uint8_t device_id = 0;       // Caller-defined device index for remapping/filters.
};

}  // namespace tenriff::input

