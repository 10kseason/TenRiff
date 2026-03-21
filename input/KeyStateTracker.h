#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "input/InputEvent.h"

namespace tenriff::input {

/// Configuration for key state tracking and debouncing.
struct KeyStateConfig {
    int64_t debounce_window_ns = 5'000'000;  ///< 5ms default debounce window.
};

/// Tracks per-key state and filters duplicate/chatter events.
/// Thread-safety: NOT thread-safe. Use from InputThread only.
class KeyStateTracker {
public:
    explicit KeyStateTracker(const KeyStateConfig& config = {});

    /// Process an incoming input event.
    /// @param event  The raw input event from the device.
    /// @return std::nullopt if the event should be filtered (duplicate/chatter),
    ///         otherwise returns the event to forward.
    [[nodiscard]] std::optional<InputEvent> process(const InputEvent& event);

    /// Reset all key states (e.g., on focus loss).
    void reset();

    /// Check if a specific key is currently pressed.
    [[nodiscard]] bool is_key_pressed(uint32_t keycode) const;

    /// Get the number of currently pressed keys.
    [[nodiscard]] std::size_t pressed_count() const;

private:
    struct KeyState {
        InputState state = InputState::Released;
        int64_t last_event_time_ns = 0;
        bool has_event = false;  ///< Whether any event has been processed for this key.
    };

    KeyStateConfig config_;
    std::unordered_map<uint32_t, KeyState> key_states_;
    std::size_t pressed_count_ = 0;
};

}  // namespace tenriff::input
