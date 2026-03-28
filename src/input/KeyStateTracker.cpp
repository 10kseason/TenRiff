#include "input/KeyStateTracker.h"

namespace tenriff::input {

KeyStateTracker::KeyStateTracker(const KeyStateConfig& config)
    : config_(config) {}

std::optional<InputEvent> KeyStateTracker::process(const InputEvent& event) {
    auto& key_state = key_states_[event.keycode];
    auto& source_state = key_state.sources[event.device_id];
    const auto now_ns = event.input_time_ns;

    if (source_state.state == event.state) {
        return std::nullopt;
    }

    if (source_state.has_event) {
        const auto time_delta = now_ns - source_state.last_event_time_ns;
        if (time_delta > 0 && time_delta < config_.debounce_window_ns) {
            return std::nullopt;
        }
    }

    const bool was_pressed = key_state.pressed_source_count > 0;
    const auto previous_source_state = source_state.state;
    source_state.state = event.state;
    source_state.last_event_time_ns = now_ns;
    source_state.has_event = true;

    if (previous_source_state == InputState::Released && event.state == InputState::Pressed) {
        ++key_state.pressed_source_count;
    } else if (previous_source_state == InputState::Pressed &&
               event.state == InputState::Released &&
               key_state.pressed_source_count > 0) {
        --key_state.pressed_source_count;
    }

    const bool is_pressed = key_state.pressed_source_count > 0;
    if (was_pressed == is_pressed) {
        return std::nullopt;
    }

    if (!was_pressed && is_pressed) {
        ++pressed_key_count_;
    } else if (was_pressed && !is_pressed && pressed_key_count_ > 0) {
        --pressed_key_count_;
    }

    InputEvent logical_event = event;
    logical_event.state = is_pressed ? InputState::Pressed : InputState::Released;
    return logical_event;
}

void KeyStateTracker::reset() {
    key_states_.clear();
    pressed_key_count_ = 0;
}

bool KeyStateTracker::is_key_pressed(uint32_t keycode) const {
    auto it = key_states_.find(keycode);
    if (it == key_states_.end()) {
        return false;
    }
    return it->second.pressed_source_count > 0;
}

std::size_t KeyStateTracker::pressed_count() const {
    return pressed_key_count_;
}

}  // namespace tenriff::input
