#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include "mania_strain.h"

namespace pattern {

enum class RatingProfile { Generic, Osu4, Osu7, Osu8, Osu10, Aery5 };
RatingProfile ProfileForKeys(int keys);

struct Snapshot {
    double score = 0.0;
    RatingProfile profile = RatingProfile::Generic;
    double stars = 0.0;
    double rating = 0.0;
    double ratingError = 0.0;
    double currentNps = 0.0;
    int outsideCalibration = 0;
    double nps = 0.0;
    double burstNps = 0.0;
    double chordPercent = 0.0;
    double jackPercent = 0.0;
    double holdOverlapPercent = 0.0;
    // 10K BMS comparison features; independent of hold weighting and NPS display.
    double patternVariety = 0.0;
    double rhythmComplexity = 0.0;
    double columnRepeatLoad = 0.0;
    double adjacentRepeatLoad = 0.0;
    std::size_t samples = 0;
    int maxChord = 0;
    int heldKeys = 0;
    bool ready = false;
    bool idle = true;
    bool limited = false;
    bool retained = false; // display-only last valid rating during an input gap
};

// UI continuity is separate from the short-window estimator used by calibration.
// NPS/metrics stay live; only the rating is retained until input resumes or ends.
class LiveContinuity {
public:
    static constexpr double FinishDelayMs = 10000.0;
    void Reset() { lastInputMs_ = -1.0; lastReady_ = {}; }
    void RecordActivity(double timeMs);
    Snapshot Update(const Snapshot& raw, double nowMs);
private:
    double lastInputMs_ = -1.0;
    Snapshot lastReady_;
};

// UI session summary; separate from the estimator so offline calibration and live
// ratings retain the same calculation. Only an idle transition publishes a result.
class PatternSummary {
public:
    void Observe(const Snapshot& current);
    void Reset() { peak_ = {}; completed_ = {}; blocked_ = false; }
    const Snapshot& Completed() const { return completed_; }
private:
    Snapshot peak_;
    Snapshot completed_;
    bool blocked_ = false;
};

// A recent-input approximation: osu! strain or locally calibrated BMS table levels.
// It cannot recover a whole chart's rating or measure player skill from key input.
// Timestamps are monotonic milliseconds. Only registered physical input transitions
// should be supplied; animation previews and layout recording are not samples.
class DifficultyEstimator {
public:
    static constexpr double WindowMs = 8000.0;
    static constexpr double IdleMs = 3000.0;
    static constexpr std::size_t MaxEvents = 4096;

    explicit DifficultyEstimator(int lanes = 8, double holdWeight = DefaultHoldWeight);
    void Reset(int lanes);
    void ReleaseAll(double timeMs = -1.0);
    void KeyDown(int lane, double timeMs);
    void KeyUp(int lane, double timeMs);
    Snapshot Evaluate(double nowMs, bool includeIdleMetrics = false) const;
    bool IsHeld(int lane) const { return lane >= 0 && lane < lanes_ && heldSince_[static_cast<std::size_t>(lane)] >= 0.0; }
    std::size_t BufferedEvents() const { return events_.size(); }

private:
    struct Press { double time; int lane; bool holdOverlap; double end = -1.0; };
    std::deque<Press> events_;
    std::array<double, 256> heldSince_{};
    int lanes_ = 8;
    double holdWeight_ = DefaultHoldWeight;
    double startedMs_ = -1.0;
    double lastDownMs_ = -1.0;
    double lastEventMs_ = -1.0;
    double overflowMs_ = -1.0;
};

} // namespace pattern
