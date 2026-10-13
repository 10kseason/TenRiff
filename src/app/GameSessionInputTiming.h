#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace tenriff::app {

inline constexpr double kGameplayInputBacklogCatchupFloorMs = 96.0;

// A continuous presentation/input clock follows the robust device fit without
// moving an already displayed instant backwards. Corrections change velocity,
// never phase. The 500 ms target and +/-2% bound are explicit engineering limits,
// not a measured optimal setting for every physical driver.
class GameplayInputClock {
public:
    void reset(std::optional<int64_t> resume_sample = std::nullopt) {
        start_ = 0;
        count_ = 0;
        resume_sample_ = resume_sample;
    }

    int64_t observe(int64_t time_ns, int64_t fitted_sample, int sample_rate) {
        const double nominal = static_cast<double>(sample_rate) / 1'000'000'000.0;
        if (sample_rate <= 0) return fitted_sample;
        double sample = static_cast<double>(resume_sample_.value_or(fitted_sample));
        if (count_ > 0) {
            const auto& last = segment(count_ - 1);
            // A duplicate/delayed observation must not disorder the history
            // used to map queued input. The next fresh observation can refit.
            if (time_ns <= last.time_ns) return static_cast<int64_t>(std::llround(last.sample));
            sample = project(last, time_ns);
        }
        const double phase_error = static_cast<double>(fitted_sample) - sample;
        const double rate = std::clamp(1.0 + phase_error / (sample_rate * 0.5), 0.98, 1.02);
        Segment next{time_ns, sample, nominal * rate, rate};
        if (count_ == kCapacity) {
            start_ = (start_ + 1) % kCapacity;
            --count_;
        }
        segments_[(start_ + count_) % kCapacity] = next;
        ++count_;
        resume_sample_.reset();
        return static_cast<int64_t>(std::llround(sample));
    }

    [[nodiscard]] std::optional<int64_t> sample_at(int64_t time_ns) const {
        if (count_ == 0) return std::nullopt;
        // Keep enough fixed history for fresh queued input at high callback
        // rates. Looking up an old event never mutates the render-time cursor.
        std::size_t lower = 0;
        std::size_t upper = count_;
        while (lower < upper) {
            const auto middle = lower + (upper - lower) / 2;
            if (segment(middle).time_ns <= time_ns) lower = middle + 1;
            else upper = middle;
        }
        return static_cast<int64_t>(std::llround(project(segment(lower == 0 ? 0 : lower - 1), time_ns)));
    }

    [[nodiscard]] double rate() const { return count_ ? segment(count_ - 1).rate : 1.0; }

private:
    struct Segment {
        int64_t time_ns = 0;
        double sample = 0.0;
        double samples_per_ns = 0.0;
        double rate = 1.0;
    };
    static constexpr std::size_t kCapacity = 4096;
    [[nodiscard]] const Segment& segment(std::size_t index) const {
        return segments_[(start_ + index) % kCapacity];
    }
    [[nodiscard]] static double project(const Segment& value, int64_t time_ns) {
        return value.sample + static_cast<double>(time_ns - value.time_ns) * value.samples_per_ns;
    }
    std::array<Segment, kCapacity> segments_{};
    std::size_t start_ = 0;
    std::size_t count_ = 0;
    std::optional<int64_t> resume_sample_;
};

struct StartupInputTimingAnchor {
    int64_t playback_sample = 0;
    int64_t callback_time_ns = 0;
    bool valid = false;
};

[[nodiscard]] inline std::optional<int64_t> estimate_input_sample_from_startup_anchor(
    int64_t input_time_ns,
    const StartupInputTimingAnchor& anchor,
    int sample_rate) {
    if (!anchor.valid || sample_rate <= 0) {
        return std::nullopt;
    }

    // Keep the subtraction in integer space. MSVC gives long double the same
    // precision as double, so subtracting multi-day absolute QPC values after
    // conversion can erase the callback-scale delta.
    const int64_t delta_ns = input_time_ns - anchor.callback_time_ns;
    const long double delta_samples =
        static_cast<long double>(delta_ns) *
        static_cast<long double>(sample_rate) / 1'000'000'000.0L;
    const int64_t estimated_sample = anchor.playback_sample +
                                     static_cast<int64_t>(std::llround(delta_samples));
    return (std::max)(int64_t{0}, estimated_sample);
}

[[nodiscard]] inline int64_t resolve_startup_gameplay_input_sample(
    std::optional<int64_t> clock_sync_sample,
    int64_t input_time_ns,
    const StartupInputTimingAnchor& anchor,
    int sample_rate,
    int64_t fallback_sample) {
    if (clock_sync_sample.has_value()) {
        return *clock_sync_sample;
    }

    if (auto startup_sample =
            estimate_input_sample_from_startup_anchor(input_time_ns, anchor, sample_rate);
        startup_sample.has_value()) {
        return *startup_sample;
    }

    return fallback_sample;
}

[[nodiscard]] inline double gameplay_input_backlog_stale_window_ms(double bad_window_ms) {
    return std::max(kGameplayInputBacklogCatchupFloorMs, bad_window_ms);
}

[[nodiscard]] inline bool gameplay_input_event_is_stale(int64_t input_time_ns,
                                                        int64_t callback_time_ns,
                                                        double bad_window_ms) {
    if (input_time_ns <= 0 || callback_time_ns <= input_time_ns) {
        return false;
    }

    const int64_t event_age_ns = callback_time_ns - input_time_ns;
    const long double stale_window_ns =
        static_cast<long double>(gameplay_input_backlog_stale_window_ms(bad_window_ms)) *
        1'000'000.0L;
    return static_cast<long double>(event_age_ns) > stale_window_ns;
}

[[nodiscard]] inline int64_t reconcile_fresh_gameplay_input_sample(
    int64_t resolved_sample,
    std::optional<int64_t> anchor_sample,
    bool event_is_stale,
    double bad_window_ms,
    int sample_rate) {
    if (event_is_stale || !anchor_sample.has_value() || sample_rate <= 0) {
        return resolved_sample;
    }

    const long double drift_samples = std::abs(
        static_cast<long double>(resolved_sample) -
        static_cast<long double>(*anchor_sample));
    const long double stale_window_samples =
        static_cast<long double>(gameplay_input_backlog_stale_window_ms(bad_window_ms)) *
        static_cast<long double>(sample_rate) / 1000.0L;
    return drift_samples > stale_window_samples ? *anchor_sample : resolved_sample;
}

}  // namespace tenriff::app
