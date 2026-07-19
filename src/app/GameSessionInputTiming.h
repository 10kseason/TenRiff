#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace tenriff::app {

inline constexpr double kGameplayInputBacklogCatchupFloorMs = 96.0;

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
