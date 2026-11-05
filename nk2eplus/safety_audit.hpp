#pragma once

#include "chart.hpp"
#include <map>
#include <string>

namespace keyweaver_eplus::nk2 {

// Explicit IDs produced by the current tap-boost pass only; never infer
// authorization from a generated-note prefix or source-lane hint alone.
using TapRepeatGuards = std::map<std::string, int>;

// Output inspection is independent of the placement solver and its counters.
// Timing changes and dropped notes are reported, but are not safety failures.
struct SafetyAudit {
    int invalidLanes = 0;
    int invalidHolds = 0;
    int sameTimeCollisions = 0;
    int longNoteConflicts = 0;
    int createdLaneRepeats = 0;
    int tempoAllowedTapRepeats = 0;
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
// Optional tap guards relax only pairs involving explicitly listed new taps;
// collisions, LN conflicts, provenance and pairs of base notes stay unchanged.
// At a 500ms window, createdLaneRepeats + tempoAllowedTapRepeats is the strict
// legacy repeat count. An omitted map preserves the original audit behavior.
SafetyAudit auditSafety(const Chart& source,
                        const Chart& output,
                        int targetKeyCount,
                        int jackWindowMs = 500,
                        bool requireSourceLnCoverage = false,
                        const TapRepeatGuards* tapRepeatGuards = nullptr);

}  // namespace keyweaver_eplus::nk2
