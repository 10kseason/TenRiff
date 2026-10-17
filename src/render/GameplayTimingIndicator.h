#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tenriff::render {

inline constexpr double kTimingIndicatorRangeMs = 80.0;
inline constexpr std::uint32_t kTimingFastColor = 0x22B8F0;
inline constexpr std::uint32_t kTimingMaximumColor = 0x35D46F;
inline constexpr std::uint32_t kTimingLateColor = 0xFF4058;

[[nodiscard]] inline float timing_maximum_fraction(double maximum_ms) {
    return std::isfinite(maximum_ms)
        ? static_cast<float>(std::clamp(maximum_ms / kTimingIndicatorRangeMs, 0.0, 1.0)) : 0.0f;
}

// The window is supplied by the actual engine, after policy/rate/sample rounding.
// Keep an awarded maximum green even when a special judgement rule overrides timing.
[[nodiscard]] inline std::uint32_t timing_marker_color(double delta_ms, double maximum_ms,
                                                       bool awarded_maximum = false) {
    if (awarded_maximum || (std::isfinite(delta_ms) && std::isfinite(maximum_ms) &&
                           std::abs(delta_ms) <= std::max(0.0, maximum_ms)))
        return kTimingMaximumColor;
    return delta_ms < 0.0 ? kTimingFastColor : kTimingLateColor;
}

}  // namespace tenriff::render
