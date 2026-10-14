#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "input/InputEvent.h"

namespace tenriff::input::gamepad {

inline constexpr std::uint32_t kKeycodeBase = 0x20000u;
inline constexpr std::size_t kControlCount = 16;
inline constexpr std::size_t kControllerCount = 4;
inline constexpr std::uint8_t kTriggerThreshold = 30;
inline constexpr std::array<std::string_view, kControlCount> kKeyNames{
    "PadUp", "PadDown", "PadLeft", "PadRight", "PadStart", "PadBack",
    "PadLS", "PadRS", "PadLB", "PadRB", "PadA", "PadB", "PadX", "PadY",
    "PadLT", "PadRT",
};

[[nodiscard]] inline constexpr std::optional<std::size_t> keycode_index(std::uint32_t keycode) noexcept {
    if (keycode < kKeycodeBase || keycode >= kKeycodeBase + kControlCount) return std::nullopt;
    return static_cast<std::size_t>(keycode - kKeycodeBase);
}

[[nodiscard]] inline constexpr bool is_gamepad_keycode(std::uint32_t keycode) noexcept {
    return keycode_index(keycode).has_value();
}

// Portable subset of XINPUT_GAMEPAD. The native poller owns API calls and copies
// these fields; this decoder intentionally has no Windows or device lifetime.
struct GamepadState {
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
};

[[nodiscard]] inline constexpr std::uint16_t decode_mask(const GamepadState& state) noexcept {
    // XInput's ten navigation/shoulder bits are contiguous; ABXY occupy bits
    // 12..15. Reserved bits 10/11 must not become synthetic controls.
    return static_cast<std::uint16_t>((state.buttons & 0x03ffu) |
        ((state.buttons & 0xf000u) >> 2u) |
        (state.left_trigger > kTriggerThreshold ? 0x4000u : 0u) |
        (state.right_trigger > kTriggerThreshold ? 0x8000u : 0u));
}

class GamepadTracker {
public:
    [[nodiscard]] std::vector<InputEvent> update(
        const std::array<std::optional<GamepadState>, kControllerCount>& states,
        bool allowed, std::int64_t time_ns) {
        std::uint16_t next_mask = 0;
        for (std::size_t index = 0; index < states.size(); ++index) {
            auto& slot = slots_[index];
            if (!states[index]) {
                slot = {};
                continue;
            }
            const auto raw_mask = decode_mask(*states[index]);
            // Connection/focus transitions cannot manufacture a press from a
            // button already held elsewhere. Suppression is per slot so one
            // controller's release cannot arm another controller's held key.
            if (!slot.connected || !allowed || !allowed_) slot.suppressed = raw_mask;
            else slot.suppressed &= raw_mask;
            slot.connected = true;
            if (allowed) next_mask |= static_cast<std::uint16_t>(raw_mask & ~slot.suppressed);
        }
        allowed_ = allowed;

        std::vector<InputEvent> events;
        const auto changed = static_cast<std::uint16_t>(current_mask_ ^ next_mask);
        for (std::size_t index = 0; index < kControlCount; ++index) {
            const auto bit = static_cast<std::uint16_t>(1u << index);
            if ((changed & bit) == 0) continue;
            events.push_back(InputEvent{kKeycodeBase + static_cast<std::uint32_t>(index),
                (next_mask & bit) != 0 ? InputState::Pressed : InputState::Released,
                time_ns, kPollingAggregateDeviceId});
        }
        current_mask_ = next_mask;
        return events;
    }

    [[nodiscard]] std::uint16_t current_mask() const noexcept { return current_mask_; }

private:
    struct Slot {
        bool connected = false;
        std::uint16_t suppressed = 0;
    };
    std::array<Slot, kControllerCount> slots_{};
    std::uint16_t current_mask_ = 0;
    bool allowed_ = false;
};

}  // namespace tenriff::input::gamepad
