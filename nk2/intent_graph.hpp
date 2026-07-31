#pragma once

#include <vector>

#include "nk2/chart.hpp"

namespace keyconv::nk2 {

struct IntentGraphSummary {
    int sourceKeyCount = 0;
    int targetKeyCount = 0;
    int totalNotes = 0;
    int tapNotes = 0;
    int holdNotes = 0;
    int slices = 0;
    int chordSlices = 0;
    int jackMotifs = 0;
    int trillMotifs = 0;
    int stairMotifs = 0;
    int streamMotifs = 0;
    int lnAnchors = 0;
    int strongBeatAnchors = 0;
    int mirrorSupportCandidates = 0;
    int recognizabilityAnchors = 0;
    double averageLocalNps = 0.0;
};

IntentGraphSummary buildIntentGraphSummary(const Chart& chart,
                                           int sourceKeyCount,
                                           int targetKeyCount,
                                           int sameTimeEpsilonMs);

}  // namespace keyconv::nk2
