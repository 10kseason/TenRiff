#include "input/KeyStateTracker.h"

namespace tenriff::input {

KeyStateTracker::KeyStateTracker(const KeyStateConfig& config)
    : config_(config) {}

std::optional<InputEvent> KeyStateTracker::process(const InputEvent& event) {
    auto& key_state = key_states_[event.keycode];
    const auto now_ns = event.input_time_ns;

    // Filter duplicate state transitions (DOWN while DOWN, UP while UP).
    if (key_state.state == event.state) {
        return std::nullopt;
    }

    // Filter chatter: rapid state changes within debounce window.
    // Only apply debounce if this key has had a prior event.
    if (key_state.has_event) {
        const auto time_delta = now_ns - key_state.last_event_time_ns;
        if (time_delta > 0 && time_delta < config_.debounce_window_ns) {
            // This is potential chatter - keep the original state, reject this event.
            return std::nullopt;
        }
    }

    // Valid state transition - update tracking.
    const auto old_state = key_state.state;
    key_state.state = event.state;
    key_state.last_event_time_ns = now_ns;
    key_state.has_event = true;

    // Update pressed count.
    if (old_state == InputState::Released && event.state == InputState::Pressed) {
        ++pressed_count_;
    } else if (old_state == InputState::Pressed && event.state == InputState::Released) {
        if (pressed_count_ > 0) {
            --pressed_count_;
        }
    }

    return event;
}

void KeyStateTracker::reset() {
    key_states_.clear();
    pressed_count_ = 0;
}

bool KeyStateTracker::is_key_pressed(uint32_t keycode) const {
    auto it = key_states_.find(keycode);
    if (it == key_states_.end()) {
        return false;
    }
    return it->second.state == InputState::Pressed;
}

std::size_t KeyStateTracker::pressed_count() const {
    return pressed_count_;
}

}  // namespace tenriff::input
