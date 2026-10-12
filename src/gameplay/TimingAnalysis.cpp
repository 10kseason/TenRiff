#include "gameplay/TimingAnalysis.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

double TimingDistribution::stddev_delta_ms() const noexcept {
    return measured_count > 1
               ? std::sqrt(std::max(0.0, m2_delta_ms) / static_cast<double>(measured_count - 1))
               : 0.0;
}

void TimingDistribution::record(game::Judgement judgement,
                                std::optional<double> measured_delta_ms,
                                bool unmeasured_miss) noexcept {
    ++event_count;
    if (!measured_delta_ms.has_value() || !std::isfinite(*measured_delta_ms)) {
        if (unmeasured_miss) ++unmeasured_miss_count;
        return;
    }

    const double value = *measured_delta_ms;
    ++measured_count;
    if (value < -0.05) {
        ++early_count;
        if (judgement != game::Judgement::PG) ++fast_count;
    } else if (value > 0.05) {
        ++late_count;
        if (judgement != game::Judgement::PG) ++slow_count;
    } else {
        ++center_count;
    }
    const double delta = value - mean_delta_ms;
    mean_delta_ms += delta / static_cast<double>(measured_count);
    m2_delta_ms += delta * (value - mean_delta_ms);
    mean_abs_delta_ms += (std::abs(value) - mean_abs_delta_ms) / static_cast<double>(measured_count);

    std::size_t bin = 0;
    if (value >= kTimingHistogramMaxMs) {
        bin = kTimingHistogramBinCount - 1;
    } else if (value >= kTimingHistogramMinMs) {
        bin = 1 + static_cast<std::size_t>((value - kTimingHistogramMinMs) / kTimingHistogramBinWidthMs);
        bin = std::min(bin, kTimingHistogramBinCount - 2);
    }
    ++histogram[bin];
}

double TimingAccuracyBucket::accuracy_percent() const noexcept {
    return accuracy_weight > 0.0
               ? std::clamp(accuracy_points / accuracy_weight * 100.0, 0.0, 100.0)
               : 0.0;
}

void TimingAccuracyBucket::record(game::Judgement judgement, double weight) noexcept {
    // Mirror ResultStats' native categorical accuracy, including split LN weights
    // and empty POORs. This is deliberately unrelated to detailed timing accuracy.
    const double safe_weight = std::isfinite(weight) && weight > 0.0 ? weight : 1.0;
    double credit = 0.0;
    switch (judgement) {
        case game::Judgement::PG: credit = 1.0; break;
        case game::Judgement::GR: credit = 0.8; break;
        case game::Judgement::GD: credit = 0.5; break;
        case game::Judgement::BD: credit = 0.2; break;
        case game::Judgement::PR: break;
    }
    ++event_count;
    accuracy_points += credit * safe_weight;
    accuracy_weight += safe_weight;
}

void TimingAnalysis::initialize(int lanes, int rate, int64_t duration) noexcept {
    *this = TimingAnalysis{};
    lane_count = std::clamp(lanes, 0, static_cast<int>(kTimingAnalysisMaxLanes));
    sample_rate = std::max(0, rate);
    duration_samples = std::max<int64_t>(0, duration);
    available = lanes > 0 && lanes <= static_cast<int>(kTimingAnalysisMaxLanes) && sample_rate > 0;
}

void TimingAnalysis::observe(TimingEventKind kind, int lane, int64_t target_sample,
                             game::Judgement judgement,
                             std::optional<double> measured_delta_ms, double weight) noexcept {
    const auto kind_index = static_cast<std::size_t>(kind);
    if (!available || kind_index >= by_kind.size() || lane <= 0 || lane > lane_count) return;
    const bool unmeasured_kind = kind == TimingEventKind::AutoCompletion || kind == TimingEventKind::EmptyPoor;
    if (unmeasured_kind || (measured_delta_ms && !std::isfinite(*measured_delta_ms))) {
        measured_delta_ms.reset();
    }
    const bool unmeasured_miss = !unmeasured_kind && !measured_delta_ms.has_value();
    total.record(judgement, measured_delta_ms, unmeasured_miss);
    by_lane[static_cast<std::size_t>(lane - 1)].record(judgement, measured_delta_ms, unmeasured_miss);
    by_kind[kind_index].record(judgement, measured_delta_ms, unmeasured_miss);

    std::size_t segment = 0;
    if (target_sample > 0 && duration_samples > 0) {
        // Divide first to avoid overflowing int64_t for long or adversarial charts.
        const double position = static_cast<double>(target_sample) / static_cast<double>(duration_samples);
        segment = position >= 1.0 ? segments.size() - 1
                                 : std::min(static_cast<std::size_t>(position * segments.size()), segments.size() - 1);
    }
    segments[segment].record(judgement, weight);
}

}  // namespace tenriff::gameplay
