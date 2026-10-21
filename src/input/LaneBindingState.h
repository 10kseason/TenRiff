#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "input/InputEvent.h"

namespace tenriff::input {

// Every fresh physical press strikes the lane and takes ownership. Releasing
// that key releases the lane; an older held key cannot revive it without a new
// physical press. Transfers do not insert a release that would break an LN.
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
        owners_.assign(static_cast<std::size_t>(maximum_lane), std::nullopt);
    }

    void reset() noexcept {
        for (auto& [key, binding] : keys_) binding.down = false;
        std::fill(owners_.begin(), owners_.end(), std::nullopt);
    }

    void rebaseline(const std::vector<std::uint32_t>& held_keys, bool preserve_ownership) {
        if (!preserve_ownership) {
            reset();
            for (const auto key : held_keys) static_cast<void>(apply(key, InputState::Pressed));
            return;
        }
        // Resume reconciles physical releases without inventing a new strike
        // or reviving a surrendered key. Keys first held during pause must be
        // released and pressed in gameplay before they can become owners.
        for (auto& [key, binding] : keys_) {
            binding.down = std::find(held_keys.begin(), held_keys.end(), key) != held_keys.end();
            auto& owner = owners_[static_cast<std::size_t>(binding.lane - 1)];
            if (!binding.down && owner == key) owner.reset();
        }
    }

    [[nodiscard]] std::optional<InputState> apply(std::uint32_t key, InputState state) noexcept {
        const auto found = keys_.find(key);
        if (found == keys_.end()) return std::nullopt;
        auto& binding = found->second;
        const bool down = state == InputState::Pressed;
        if (down == binding.down) return std::nullopt;
        binding.down = down;
        auto& owner = owners_[static_cast<std::size_t>(binding.lane - 1)];
        if (down) {
            owner = key;
            return InputState::Pressed;
        }
        if (owner != key) return std::nullopt;
        owner.reset();
        return InputState::Released;
    }

    [[nodiscard]] bool pressed(int lane) const noexcept {
        return lane > 0 && static_cast<std::size_t>(lane) <= owners_.size() &&
               owners_[static_cast<std::size_t>(lane - 1)].has_value();
    }

private:
    struct PhysicalKey {
        int lane = 0;
        bool down = false;
    };
    std::unordered_map<std::uint32_t, PhysicalKey> keys_;
    std::vector<std::optional<std::uint32_t>> owners_;
};

}  // namespace tenriff::input
