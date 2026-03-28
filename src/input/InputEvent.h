#pragma once

#include <cstdint>

namespace tenriff::input {

enum class InputState : uint8_t {
    Released = 0,
    Pressed = 1,
};

using InputSourceToken = std::uintptr_t;

inline constexpr InputSourceToken kPollingAggregateDeviceId = 0;

// POD describing a single device event stamped on the input clock.
struct InputEvent {
    uint32_t keycode = 0;        // Platform scancode or evdev keycode.
    InputState state = InputState::Released;
    int64_t input_time_ns = 0;   // Monotonic nanoseconds on the input clock.
    InputSourceToken device_id = kPollingAggregateDeviceId;  // Full-width caller-defined source token.
};

}  // namespace tenriff::input
