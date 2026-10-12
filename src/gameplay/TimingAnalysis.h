#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "game/GaugeManager.h"

namespace tenriff::gameplay {

// Replay evidence accepts up to 32 lanes even though the current menu offers 16.
inline constexpr std::size_t kTimingAnalysisMaxLanes = 32;
inline constexpr std::size_t kTimingAnalysisSegmentCount = 12;
inline constexpr double kTimingHistogramMinMs = -100.0;
inline constexpr double kTimingHistogramMaxMs = 100.0;
inline constexpr double kTimingHistogramBinWidthMs = 5.0;
// Underflow, 40 half-open 5ms intervals in [-100, 100), and overflow.
inline constexpr std::size_t kTimingHistogramBinCount = 42;

enum class TimingEventKind : std::size_t {
    Tap,
    LnHead,
    LnRelease,
    HoldFailure,  // A measured release graded BAD; timeout is LnRelease without a delta.
    AutoCompletion,
    EmptyPoor,
    Count,
};
inline constexpr std::size_t kTimingEventKindCount =
    static_cast<std::size_t>(TimingEventKind::Count);

struct TimingDistribution {
    uint64_t event_count = 0;
    uint64_t measured_count = 0;
    uint64_t unmeasured_miss_count = 0;
    uint64_t early_count = 0;
    uint64_t late_count = 0;
    uint64_t center_count = 0;
    // Match the existing FAST/SLOW contract: exclude PG; deadband is +/-0.05ms.
    uint64_t fast_count = 0;
    uint64_t slow_count = 0;
    double mean_delta_ms = 0.0;
    double mean_abs_delta_ms = 0.0;
    double m2_delta_ms = 0.0;
    std::array<uint64_t, kTimingHistogramBinCount> histogram{};

    [[nodiscard]] double stddev_delta_ms() const noexcept;
    void record(game::Judgement judgement, std::optional<double> measured_delta_ms,
                bool unmeasured_miss) noexcept;
};

struct TimingAccuracyBucket {
    uint64_t event_count = 0;
    double accuracy_points = 0.0;
    double accuracy_weight = 0.0;

    [[nodiscard]] double accuracy_percent() const noexcept;
    void record(game::Judgement judgement, double weight) noexcept;
};

// Observational data only: never used by scoring, judgement, gauges, or replay validation.
// Fixed storage keeps every observation allocation-free on the audio/input path.
struct TimingAnalysis {
    bool available = false;
    int lane_count = 0;
    int sample_rate = 0;
    int64_t duration_samples = 0;
    TimingDistribution total;
    std::array<TimingDistribution, kTimingAnalysisMaxLanes> by_lane{};
    std::array<TimingDistribution, kTimingEventKindCount> by_kind{};
    std::array<TimingAccuracyBucket, kTimingAnalysisSegmentCount> segments{};

    void initialize(int lanes, int rate, int64_t duration) noexcept;
    // target_sample is the chart head/tail position, or the actual press for EmptyPoor.
    // A missing head retains Tap/LnHead with no delta; a release timeout is LnRelease
    // with no delta. Automatic tails and empty presses never create timing samples.
    void observe(TimingEventKind kind, int lane, int64_t target_sample,
                 game::Judgement judgement, std::optional<double> measured_delta_ms,
                 double weight) noexcept;
};

}  // namespace tenriff::gameplay
