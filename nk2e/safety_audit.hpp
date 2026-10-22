#pragma once

#include "nk2e/chart.hpp"

namespace keyweaver_e::nk2 {

// Output inspection is independent of the placement solver and its counters.
// Timing changes and dropped notes are reported, but are not safety failures.
struct SafetyAudit {
    int invalidLanes = 0;
    int invalidHolds = 0;
    int sameTimeCollisions = 0;
    int longNoteConflicts = 0;
    int createdLaneRepeats = 0;
    int preservedLaneRepeats = 0;
    int retainedSourceNotes = 0;
    int droppedSourceNotes = 0;
    int addedNotes = 0;
    int retimedSourceNotes = 0;
    int changedHoldDurations = 0;
    int unknownSourceNotes = 0;
    bool sourceLnCoverageRequired = false;
    int generatedHoldsOutsideSourceLn = 0;

    bool passed() const;
};

// Repeats compare chronological neighbours on each output lane, including
// pairs separated by notes on other lanes. Original note IDs establish intent;
// output sourceLane hints are never trusted as evidence of source repetition.
// Generated LN coverage always counts missing containment in a single original
// valid LN interval, regardless of lane. The flag only makes it a pass condition.
SafetyAudit auditSafety(const Chart& source,
                        const Chart& output,
                        int targetKeyCount,
                        int jackWindowMs = 500,
                        bool requireSourceLnCoverage = false);

}  // namespace keyweaver_e::nk2
