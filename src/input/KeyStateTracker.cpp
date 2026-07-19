#include "input/KeyStateTracker.h"

namespace tenriff::input {

KeyStateTracker::KeyStateTracker(const KeyStateConfig& config)
    : config_(config) {}

std::optional<InputEvent> KeyStateTracker::process(const InputEvent& event) {
    auto& key_state = key_states_[event.keycode];
    const bool was_pressed = key_state.pressed_source_count > 0;

    // Polling aggregate is a physical-state mirror, not another keyboard source.
    if (event.device_id == kPollingAggregateDeviceId && event.state == InputState::Released) {
        auto& source_state = key_state.sources[event.device_id];
        source_state.state = InputState::Released;
        source_state.last_event_time_ns = event.input_time_ns;
        source_state.has_event = true;

        if (!was_pressed) {
            return std::nullopt;
        }

        for (auto& [source, state] : key_state.sources) {
            static_cast<void>(source);
            state.state = InputState::Released;
            state.last_event_time_ns = event.input_time_ns;
            state.has_event = true;
        }
        key_state.pressed_source_count = 0;
        if (pressed_key_count_ > 0) {
            --pressed_key_count_;
        }

        InputEvent logical_event = event;
        logical_event.state = InputState::Released;
        return logical_event;
    }

    const auto now_ns = event.input_time_ns;

    // Polling is an aggregate physical mirror. If it observed the press first,
    // move that single logical ownership to the concrete RawInput source when
    // the delayed raw edge arrives. Counting both sources makes the raw release
    // leave the key logically pressed and swallows the next fast jack.
    if (event.device_id != kPollingAggregateDeviceId && event.state == InputState::Pressed) {
        auto polling = key_state.sources.find(kPollingAggregateDeviceId);
        if (polling != key_state.sources.end() && polling->second.state == InputState::Pressed) {
            polling->second.state = InputState::Released;
            polling->second.last_event_time_ns = now_ns;
            polling->second.has_event = true;
            if (key_state.pressed_source_count > 0) {
                --key_state.pressed_source_count;
            }
        }
    }

    auto& source_state = key_state.sources[event.device_id];

    if (source_state.state == event.state) {
        return std::nullopt;
    }

    // A polling press that agrees with RawInput should not keep the key down after RawInput releases.
    if (event.device_id == kPollingAggregateDeviceId && event.state == InputState::Pressed && was_pressed) {
        // Keep the aggregate source non-owning. Its later release can still
        // clear stale raw sources through the dedicated release path above.
        source_state.state = InputState::Released;
        source_state.last_event_time_ns = now_ns;
        source_state.has_event = true;
        return std::nullopt;
    }

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
