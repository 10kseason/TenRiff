#include "pattern_difficulty.h"
#include "mania_strain.h"
#include "calibration_tables.h"
#include "aery_auxiliary_reference.h"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <vector>

namespace pattern {

RatingProfile ProfileForKeys(int keys) {
    switch (keys) {
    case 4: return RatingProfile::Osu4;
    case 5: return RatingProfile::Aery5;
    case 7: return RatingProfile::Osu7;
    case 8: return RatingProfile::Osu8;
    case 10: return RatingProfile::Osu10;
    default: return RatingProfile::Generic;
    }
}

void LiveContinuity::RecordActivity(double timeMs) {
    if (std::isfinite(timeMs) && timeMs >= 0.0 && timeMs >= lastInputMs_) lastInputMs_ = timeMs;
}

Snapshot LiveContinuity::Update(const Snapshot& raw, double nowMs) {
    if (!std::isfinite(nowMs) || lastInputMs_ < 0.0) return raw;
    if (!raw.heldKeys && nowMs - lastInputMs_ >= FinishDelayMs) {
        Reset();
        Snapshot ended;
        ended.profile = raw.profile;
        return ended;
    }
    Snapshot result = raw;
    result.idle = false;
    if (raw.limited || (lastReady_.ready && lastReady_.profile != raw.profile)) lastReady_ = {};
    const bool fresh = raw.ready && !raw.limited && (raw.currentNps > 0 || raw.heldKeys > 0);
    if (fresh) lastReady_ = raw;
    else if (!raw.limited && lastReady_.ready) {
        result.ready = result.retained = true;
        result.score = lastReady_.score;
        result.stars = lastReady_.stars;
        result.rating = lastReady_.rating;
        result.ratingError = lastReady_.ratingError;
        result.outsideCalibration = lastReady_.outsideCalibration;
    }
    return result;
}

void PatternSummary::Observe(const Snapshot& current) {
    if (current.idle) {
        if (peak_.ready && !blocked_) completed_ = peak_;
        peak_ = {};
        blocked_ = false;
        return;
    }
    if (current.limited) blocked_ = true;
    if (!current.ready || current.retained || blocked_) return;
    const double value = current.profile == RatingProfile::Generic ? current.score : current.rating;
    const double best = peak_.profile == RatingProfile::Generic ? peak_.score : peak_.rating;
    if (!peak_.ready || value > best || (value == best && current.stars > peak_.stars)) peak_ = current;
}

template<std::size_t N>
double CalibratedRating(double stars, const std::array<CalibrationPoint, N>& curve, int& outside) {
    if constexpr (N < 2) return 0.0;
    else {
        if (stars < curve.front().strain) { outside = -1; return curve.front().level; }
        if (stars > curve.back().strain) { outside = 1; return curve.back().level; }
        for (std::size_t i = 1; i < N; ++i) if (stars <= curve[i].strain) {
            const auto& a = curve[i - 1]; const auto& b = curve[i];
            const double t = (stars - a.strain) / std::max(0.000001, b.strain - a.strain);
            return a.level + t * (b.level - a.level);
        }
        return curve.back().level;
    }
}

DifficultyEstimator::DifficultyEstimator(int lanes, double holdWeight)
    : holdWeight_(std::isfinite(holdWeight) ? std::clamp(holdWeight, 0.0, 1.0) : DefaultHoldWeight) {
    Reset(lanes);
}

void DifficultyEstimator::Reset(int lanes) {
    lanes_ = std::clamp(lanes, 1, 256);
    events_.clear();
    heldSince_.fill(-1.0);
    startedMs_ = lastDownMs_ = lastEventMs_ = overflowMs_ = -1.0;
}

void DifficultyEstimator::ReleaseAll(double timeMs) {
    if (!std::isfinite(timeMs) || timeMs < lastEventMs_) timeMs = lastEventMs_;
    for (auto& press : events_) if (press.end < 0.0) press.end = std::max(press.time, timeMs);
    heldSince_.fill(-1.0);
}

void DifficultyEstimator::KeyDown(int lane, double timeMs) {
    if (lane < 0 || lane >= lanes_ || !std::isfinite(timeMs) || timeMs < 0.0 || timeMs < lastEventMs_) return;
    if (heldSince_[static_cast<std::size_t>(lane)] >= 0.0) return; // OS key repeat is not a new note.
    const bool holding = std::any_of(heldSince_.begin(), heldSince_.end(), [](double t) { return t >= 0.0; });
    if (!holding && lastDownMs_ >= 0.0 && timeMs - lastDownMs_ >= IdleMs) Reset(lanes_);
    if (startedMs_ < 0.0) startedMs_ = timeMs;
    bool overlap = false;
    for (double held : heldSince_) if (held >= 0.0 && timeMs - held >= 90.0) overlap = true;
    heldSince_[static_cast<std::size_t>(lane)] = timeMs;
    lastDownMs_ = lastEventMs_ = timeMs;
    // Four seconds of context warms up the decaying strain before the 8 s rating window.
    while (!events_.empty() && events_.front().time <= timeMs - WindowMs - 4000.0) events_.pop_front();
    if (events_.size() >= MaxEvents) {
        events_.pop_front();
        overflowMs_ = timeMs;
    }
    events_.push_back({timeMs, lane, overlap});
}

void DifficultyEstimator::KeyUp(int lane, double timeMs) {
    if (lane < 0 || lane >= lanes_ || !std::isfinite(timeMs) || timeMs < 0.0 || timeMs < lastEventMs_) return;
    heldSince_[static_cast<std::size_t>(lane)] = -1.0;
    for (auto it = events_.rbegin(); it != events_.rend(); ++it) if (it->lane == lane && it->end < 0.0) {
        it->end = timeMs;
        break;
    }
    lastEventMs_ = timeMs;
}

Snapshot DifficultyEstimator::Evaluate(double nowMs, bool includeIdleMetrics) const {
    Snapshot result;
    result.profile = ProfileForKeys(lanes_);
    if (!std::isfinite(nowMs) || startedMs_ < 0.0) return result;
    nowMs = std::max(nowMs, lastEventMs_);
    result.heldKeys = static_cast<int>(std::count_if(heldSince_.begin(), heldSince_.end(), [](double t) { return t >= 0.0; }));
    result.idle = result.heldKeys == 0 && nowMs - lastDownMs_ >= IdleMs;
    if (result.idle && !includeIdleMetrics) return result;
    result.limited = overflowMs_ >= 0.0 && nowMs - overflowMs_ < WindowMs;

    std::vector<TimedNote> ratedNotes;
    std::bitset<256> chordLanes;
    double chordStart = -100000.0;
    for (const auto& press : events_) {
        if (press.time > nowMs || press.time <= nowMs - WindowMs - 4000.0) continue;
        if (press.time - chordStart > 15.0 || chordLanes.test(static_cast<std::size_t>(press.lane))) {
            chordStart = press.time;
            chordLanes.reset();
        }
        chordLanes.set(static_cast<std::size_t>(press.lane));
        double end = press.end < 0.0 ? nowMs : press.end;
        // A tap has a physical release too. Short key dwell must not become an LN.
        if (end - press.time < 90.0) end = chordStart;
        ratedNotes.push_back({chordStart - startedMs_, std::max(chordStart, end) - startedMs_, press.lane});
    }

    struct Row { double time; std::bitset<256> lanes; int size = 0; };
    std::vector<Row> rows;
    std::vector<double> times;
    std::size_t overlapNotes = 0;
    for (const auto& press : events_) {
        if (press.time <= nowMs - WindowMs || press.time > nowMs) continue;
        times.push_back(press.time);
        if (press.time > nowMs - 1000.0) result.currentNps += 1.0;
        if (press.holdOverlap) ++overlapNotes;
        // Anchor each chord to its first press; never chain 15 ms gaps forever.
        // Repeated presses of one lane remain separate rows, even inside 15 ms.
        if (rows.empty() || press.time - rows.back().time > 15.0 || rows.back().lanes.test(static_cast<std::size_t>(press.lane)))
            rows.push_back({press.time, {}, 0});
        rows.back().lanes.set(static_cast<std::size_t>(press.lane));
        ++rows.back().size;
    }
    result.samples = times.size();
    if (times.empty()) return result;
    const double seconds = std::clamp((nowMs - startedMs_) / 1000.0, 2.0, WindowMs / 1000.0);
    result.nps = static_cast<double>(times.size()) / seconds;
    std::size_t left = 0;
    for (std::size_t right = 0; right < times.size(); ++right) {
        while (times[right] - times[left] >= 2000.0) ++left;
        result.burstNps = std::max(result.burstNps, static_cast<double>(right - left + 1) / 2.0);
    }
    int chordNotes = 0, jackNotes = 0;
    double jackLoad = 0.0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        result.maxChord = std::max(result.maxChord, row.size);
        if (row.size > 1) chordNotes += row.size;
        if (i == 0) continue;
        const double gap = row.time - rows[i - 1].time;
        if (gap >= 0.0 && gap < 250.0) {
            const int repeated = static_cast<int>((row.lanes & rows[i - 1].lanes).count());
            jackNotes += repeated;
            jackLoad += repeated * 2.0 * (1.0 - gap / 250.0);
        }
    }
    const double count = static_cast<double>(times.size());
    result.chordPercent = 100.0 * chordNotes / count;
    result.jackPercent = 100.0 * jackNotes / count;
    result.holdOverlapPercent = 100.0 * static_cast<double>(overlapNotes) / count;
    result.ready = !result.idle && times.size() >= 8 && rows.size() >= 4 && nowMs - startedMs_ >= 1500.0 && !result.limited;
    if (result.ready) {
        result.stars = ManiaStars(ratedNotes, lanes_, std::max(0.0, nowMs - startedMs_ - WindowMs), holdWeight_);
        result.rating = result.stars;
        if (result.profile == RatingProfile::Aery5) {
            const double logNps = std::log1p(result.nps);
            const double composite = result.stars + AeryDensityWeight * logNps;
            const double calibrated = CalibratedRating(composite, AeryCalibration, result.outsideCalibration);
            // Live feedback: chart levels 1-10 read about one level too high,
            // corresponding to original estimates 2-11. Preserve the fitted
            // table and taper the correction to zero at 13 without a jump.
            const double lowLevelCorrection = std::clamp((13.0 - calibrated) / 2.0, 0.0, 1.0);
            // The 230 training-only references provide a bounded residual.
            // Keep the manual reduction tied to the original 34-point curve:
            // the auxiliary estimate must not amplify or cancel that reduction.
            result.rating = std::max(1.0, calibrated + AeryAuxiliaryDelta(composite, logNps) - lowLevelCorrection);
            result.ratingError = AeryError80;
        }
        if (result.profile == RatingProfile::Osu10) {
            std::bitset<1024> combinations;
            std::array<double, 10> lastColumn;
            lastColumn.fill(-1000000.0);
            std::vector<double> gaps;
            double repeat = 0.0;
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto& row = rows[i];
                combinations.set(row.lanes.to_ulong()); // Exactly ten lanes in this profile.
                for (int lane = 0; lane < 10; ++lane) if (row.lanes.test(lane)) {
                    repeat += std::max(0.0, 1.0 - (row.time - lastColumn[lane]) / 250.0);
                    lastColumn[lane] = row.time;
                }
                if (i) {
                    const double gap = row.time - rows[i - 1].time;
                    if (gap >= 20.0 && gap <= 1000.0) gaps.push_back(gap);
                }
            }
            // A fixed denominator avoids an initial short sample inflating the
            // rate of distinct combinations, which saturates as repeats accrue.
            result.patternVariety = combinations.count() / (WindowMs / 1000.0);
            result.columnRepeatLoad = repeat / seconds;
            result.adjacentRepeatLoad = jackLoad / (2.0 * seconds);
            std::sort(gaps.begin(), gaps.end());
            double entropy = 0.0;
            for (std::size_t begin = 0; begin < gaps.size();) {
                std::size_t end = begin + 1;
                // Anchor clusters to the first gap: do not chain small timing
                // deviations into one cluster spanning an entire rhythm range.
                const double tolerance = std::max(12.0, gaps[begin] * 0.20);
                while (end < gaps.size() && gaps[end] - gaps[begin] <= tolerance) ++end;
                const double p = static_cast<double>(end - begin) / gaps.size();
                entropy -= p * std::log2(p);
                begin = end;
            }
            result.rhythmComplexity = std::min(2.5, entropy) * std::min(1.0, gaps.size() / 32.0);
            // Reference-calibrated 10K pattern load, not another osu! star rating.
            // The four added terms depend on presses only; the existing 2/3 LN
            // adjustment remains exclusively in stars and is never cancelled.
            result.rating = result.stars + 1.6 * result.patternVariety + 0.5 * result.rhythmComplexity +
                0.4 * result.columnRepeatLoad + 0.1 * result.adjacentRepeatLoad;
        }
        // Keep the v1 index for unsupported key counts, with the same LN adjustment.
        // Offline baseline extraction explicitly selects holdWeight=1.
        // Its arbitrary 0-100 scale must never be labelled as game/table difficulty.
        const double load = result.nps * (1.0 + 0.22 * chordNotes / count + 0.75 * jackLoad / count +
            0.25 * holdWeight_ * static_cast<double>(overlapNotes) / count) + 0.30 * std::max(0.0, result.burstNps - result.nps);
        result.score = std::clamp(100.0 * (1.0 - std::exp(-load / 24.0)), 0.0, 100.0);
    }
    return result;
}

} // namespace pattern
