#pragma once

#include "chart.hpp"
#include "safety_audit.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace keyweaver_eplus::nk2 {

struct EFullFieldTapBoostStats {
    std::int64_t inputNotes = 0;
    std::int64_t retainedSourceNotes = 0;
    std::int64_t totalAddedBudget = 0;
    std::int64_t addedTaps = 0;
    std::int64_t candidates = 0;
    std::int64_t rejectedBySafety = 0;
    std::int64_t rejectedByBudget = 0;
    // Capacity for 8K/10K; cells above the requested output count stay zero.
    std::array<std::int64_t, 10> addedLaneCounts{};
    int minTapGuardMs = 0;
    int maxTapGuardMs = 0;
    int strict500CreatedRepeats = 0;
    int tempoAllowedTapRepeats = 0;
    // Internal provenance for the final independent audit, not serialized IDs.
    TapRepeatGuards newTapGuards;
    bool reverted = false;
};

inline int fullFieldTapWindow(int time) {
    return time / 2000 - (time % 2000 < 0 ? 1 : 0);
}

inline std::int64_t fullFieldTapBudget(std::int64_t originals) {
    return (3 * originals + 3) / 4;
}

inline std::int64_t fullFieldTapPhraseBudget(std::int64_t originals) {
    return std::min<std::int64_t>(40, fullFieldTapBudget(originals));
}

// Append taps at retained original head times to an already completed E+.
// Existing note fields/lanes/LNs stay fixed; successful output is time ordered.
EFullFieldTapBoostStats addEFullFieldTaps(const Chart& source, Chart& completedEPlus, int targetKeyCount = 10);

}  // namespace keyweaver_eplus::nk2
