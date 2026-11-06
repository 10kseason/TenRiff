#pragma once

#include "chart.hpp"
#include <cstdint>

namespace keyweaver_eplus::nk2 {

constexpr bool supportsEFullFieldTarget(int keyCount) {
    return keyCount == 7 || keyCount == 8 || keyCount == 10;
}

struct EFullFieldStats {
    int movedNotes = 0;
    int movedGroups = 0;
    int acceptedMoves = 0;
    int splitLongChains = 0;
    int phraseSwaps = 0;
    std::uint64_t safetyWork = 0;
    bool budgetLimited = false;
    bool reverted = false;
    double activeLanesBefore = 0.0;
    double activeLanesAfter = 0.0;
    double concentrationBefore = 0.0;
    double concentrationAfter = 0.0;
    double sourceConcentrationBefore = 0.0;
    double sourceConcentrationAfter = 0.0;
};

// Relanes a completed E chart only. Head times, LN tails, identities and note
// count are fixed. Short E repeat chains move together; chains longer than 2s
// may change lanes at 2s boundaries, subject to the original 500ms safety guard.
// Only the requested 7K, 8K and 10K output layouts are enabled.
EFullFieldStats spreadEFullField(const Chart& source, Chart& completedE, int targetKeyCount = 10);

}  // namespace keyweaver_eplus::nk2
