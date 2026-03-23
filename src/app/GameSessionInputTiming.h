#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace tenriff::app {

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

    const long double delta_ns =
        static_cast<long double>(input_time_ns) - static_cast<long double>(anchor.callback_time_ns);
    const long double delta_samples =
        delta_ns * static_cast<long double>(sample_rate) / 1'000'000'000.0L;
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

}  // namespace tenriff::app
