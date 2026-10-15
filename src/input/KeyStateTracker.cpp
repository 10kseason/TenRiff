#include "input/KeyStateTracker.h"

namespace tenriff::input {

KeyStateTracker::KeyStateTracker(const KeyStateConfig& config)
    : config_(config) {}

bool KeyStateTracker::within_debounce(const SourceState& source, int64_t now_ns) const {
    if (config_.debounce_window_ns <= 0 || !source.has_release || now_ns < source.last_release_time_ns) {
        // A clock reset must not strand the key behind an old, future deadline.
        return false;
    }
    const auto elapsed = static_cast<uint64_t>(now_ns) - static_cast<uint64_t>(source.last_release_time_ns);
    return elapsed < static_cast<uint64_t>(config_.debounce_window_ns);
}

bool KeyStateTracker::release_source(SourceState& source, int64_t now_ns) {
    if (source.state == InputState::Released && !source.suppressed_press) {
        return false;  // Repeated UP events must not extend the debounce window.
    }
    source.state = InputState::Released;
    source.suppressed_press = false;
    source.last_release_time_ns = now_ns;
    source.has_release = true;
    return true;
}

std::optional<InputEvent> KeyStateTracker::process(const InputEvent& event) {
    auto& key_state = key_states_[event.keycode];
    const bool was_pressed = key_state.pressed_source_count > 0;
    const auto now_ns = event.input_time_ns;

    // Polling aggregate is a physical-state mirror, not another keyboard source.
    if (event.device_id == kPollingAggregateDeviceId && event.state == InputState::Released) {
        auto& source_state = key_state.sources[event.device_id];
        bool released = false;
        for (auto& [source, state] : key_state.sources) {
            static_cast<void>(source);
            released = release_source(state, now_ns) || released;
        }
        if (released) {
            source_state.last_release_time_ns = now_ns;
            source_state.has_release = true;
        }
        if (!was_pressed) {
            return std::nullopt;
        }
        key_state.pressed_source_count = 0;
        if (pressed_key_count_ > 0) {
            --pressed_key_count_;
        }

        InputEvent logical_event = event;
        logical_event.state = InputState::Released;
        return logical_event;
    }

    // Polling is an aggregate physical mirror. If it observed the press first,
    // move that single logical ownership to the concrete RawInput source when
    // the delayed raw edge arrives. Counting both sources makes the raw release
    // leave the key logically pressed and swallows the next fast jack.
    bool transferred_polling_press = false;
    if (event.device_id != kPollingAggregateDeviceId && event.state == InputState::Pressed) {
        auto polling = key_state.sources.find(kPollingAggregateDeviceId);
        if (polling != key_state.sources.end() && polling->second.state == InputState::Pressed) {
            polling->second.state = InputState::Released;
            transferred_polling_press = true;
            if (key_state.pressed_source_count > 0) {
                --key_state.pressed_source_count;
            }
        }
    }

    auto& source_state = key_state.sources[event.device_id];

    if (event.state == InputState::Released) {
        const bool owned_press = source_state.state == InputState::Pressed;
        const auto polling = key_state.sources.find(kPollingAggregateDeviceId);
        if (key_state.polling_only_cycle && polling != key_state.sources.end() &&
            polling->second.suppressed_press) {
            // A raw UP can identify a polling-only rejected DOWN even when its
            // raw DOWN never arrived. Attribute only this source, not every key.
            source_state.suppressed_press = true;
            key_state.polling_only_cycle = false;
        }
        if (!release_source(source_state, now_ns)) {
            return std::nullopt;
        }
        if (owned_press && key_state.pressed_source_count > 0) {
            --key_state.pressed_source_count;
        }
        if (key_state.pressed_source_count == 0) {
            // The mirror must share the final raw release, including a rejected
            // press's UP, so a late polling DOWN cannot bypass the cooldown.
            auto& polling = key_state.sources[kPollingAggregateDeviceId];
            polling.suppressed_press = false;
            for (const auto& [source, state] : key_state.sources) {
                if (source != kPollingAggregateDeviceId && state.suppressed_press) {
                    polling.suppressed_press = true;
                    break;
                }
            }
            polling.last_release_time_ns = now_ns;
            polling.has_release = true;
        }
        if (!was_pressed || key_state.pressed_source_count > 0) {
            return std::nullopt;
        }
        if (pressed_key_count_ > 0) --pressed_key_count_;
        return event;
    }

    if (transferred_polling_press) {
        source_state.suppressed_press = false;
    } else if (source_state.state == InputState::Pressed || source_state.suppressed_press) {
        return std::nullopt;
    }

    // A polling press that agrees with RawInput should not keep the key down after RawInput releases.
    if (event.device_id == kPollingAggregateDeviceId && event.state == InputState::Pressed && was_pressed) {
        // Keep the aggregate source non-owning. Its later release can still
        // clear stale raw sources through the dedicated release path above.
        source_state.state = InputState::Released;
        return std::nullopt;
    }

    const auto polling = key_state.sources.find(kPollingAggregateDeviceId);
    const bool blocked_unattributed_mirror =
        key_state.polling_only_cycle && polling != key_state.sources.end() &&
        (polling->second.suppressed_press || within_debounce(polling->second, now_ns));
    if (!transferred_polling_press &&
        (within_debounce(source_state, now_ns) || blocked_unattributed_mirror)) {
        source_state.suppressed_press = true;
        if (event.device_id == kPollingAggregateDeviceId) {
            // Remember the physical DOWN until UP, even if its raw copy arrives
            // after the window. Assign its owner only when a raw edge arrives;
            // other recently released keyboards must not acquire a held latch.
            key_state.polling_only_cycle = true;
        } else if (!was_pressed) {
            key_state.sources[kPollingAggregateDeviceId].suppressed_press = true;
            key_state.polling_only_cycle = false;
        }
        return std::nullopt;
    }

    source_state.state = InputState::Pressed;
    ++key_state.pressed_source_count;
    key_state.polling_only_cycle = event.device_id == kPollingAggregateDeviceId;
    if (was_pressed) return std::nullopt;
    ++pressed_key_count_;
    return event;
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
