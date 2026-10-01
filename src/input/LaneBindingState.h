#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "input/InputEvent.h"

namespace tenriff::input {

// Multiple physical bindings share one logical pressed state. A release is
// emitted only when the last held binding is released, including LN handoffs.
class LaneBindingState {
public:
    void configure(const std::unordered_map<std::uint32_t, int>& bindings) {
        keys_.clear();
        int maximum_lane = 0;
        for (const auto& [key, lane] : bindings) {
            if (lane <= 0) continue;
            keys_.emplace(key, PhysicalKey{lane, false});
            maximum_lane = std::max(maximum_lane, lane);
        }
        held_counts_.assign(static_cast<std::size_t>(maximum_lane), 0);
    }

    void reset() noexcept {
        for (auto& [key, binding] : keys_) binding.down = false;
        std::fill(held_counts_.begin(), held_counts_.end(), 0);
    }

    [[nodiscard]] std::optional<InputState> apply(std::uint32_t key, InputState state) noexcept {
        const auto found = keys_.find(key);
        if (found == keys_.end()) return std::nullopt;
        auto& binding = found->second;
        const bool down = state == InputState::Pressed;
        if (down == binding.down) return std::nullopt;
        auto& held = held_counts_[static_cast<std::size_t>(binding.lane - 1)];
        const bool was_pressed = held != 0;
        binding.down = down;
        held += down ? 1 : -1;
        const bool is_pressed = held != 0;
        if (was_pressed == is_pressed) return std::nullopt;
        return is_pressed ? InputState::Pressed : InputState::Released;
    }

    [[nodiscard]] bool pressed(int lane) const noexcept {
        return lane > 0 && static_cast<std::size_t>(lane) <= held_counts_.size() &&
               held_counts_[static_cast<std::size_t>(lane - 1)] != 0;
    }

private:
    struct PhysicalKey {
        int lane = 0;
        bool down = false;
    };
    std::unordered_map<std::uint32_t, PhysicalKey> keys_;
    std::vector<int> held_counts_;
};

}  // namespace tenriff::input
