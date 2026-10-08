#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "GameplayHudLimits.h"
#include "gameplay/GameplayChart.h"

namespace tenriff::gameplay {

struct RiffMap {
    std::array<uint8_t, kGameplayRiffMapBins> values{};
    std::size_t count = 0;
};

// A display-only, normalized density map of note heads. bins is capped to the
// fixed HUD payload; mines and long-note tails do not contribute extra heads.
[[nodiscard]] RiffMap build_riff_map(const GameplayChart& chart,
                                   std::size_t bins = kGameplayRiffMapBins);

}  // namespace tenriff::gameplay
