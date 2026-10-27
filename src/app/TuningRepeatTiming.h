#pragma once

#include <cstdint>

namespace tenriff::app {

// Keep each adjustment precise; only its cadence accelerates during a hold.
// Shared by lobby numeric controls and gameplay (including the pause menu).
inline constexpr int64_t tuning_repeat_interval_ns(int64_t held_ns) {
    return held_ns >= 1'500'000'000LL ? 15'000'000LL
         : held_ns >= 600'000'000LL ? 25'000'000LL
         : 45'000'000LL;
}

}  // namespace tenriff::app
