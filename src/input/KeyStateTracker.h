#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "input/InputEvent.h"

namespace tenriff::input {

/// Configuration for key state tracking and debouncing.
struct KeyStateConfig {
    int64_t debounce_window_ns = 0;  ///< Suppress re-presses after release; 0 preserves every real edge.
};

/// Tracks per-source ownership, optionally suppressing re-presses without delaying release edges.
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
    struct SourceState {
        InputState state = InputState::Released;
        bool suppressed_press = false;
        int64_t last_release_time_ns = 0;
        bool has_release = false;
    };

    struct KeyState {
        std::unordered_map<InputSourceToken, SourceState> sources;
        std::size_t pressed_source_count = 0;
        bool polling_only_cycle = false;
    };

    [[nodiscard]] bool within_debounce(const SourceState& source, int64_t now_ns) const;
    static bool release_source(SourceState& source, int64_t now_ns);

    KeyStateConfig config_;
    std::unordered_map<uint32_t, KeyState> key_states_;
    std::size_t pressed_key_count_ = 0;
};

}  // namespace tenriff::input
