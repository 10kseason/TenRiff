#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace tenriff::render {

inline constexpr uint32_t gameplay_judgement_rgb(std::string_view judgement) {
    if (judgement == "PG") return 0xFFE18A;
    if (judgement == "GR") return 0x6EE7F2;
    if (judgement == "G") return 0xAEB5BF;
    if (judgement == "BAD") return 0xFF9F43;
    if (judgement == "POOR") return 0xFF6B6B;
    return 0xE8ECF1;
}

inline std::wstring gameplay_timing_feedback_text(double delta_ms) {
    if (!std::isfinite(delta_ms)) {
        return {};
    }

    if (delta_ms == 0.0) {
        return {};
    }

    // Round magnitude outward: the first sample outside a 22 ms PG window
    // must read 22.1, never 22.0. Ignore one multiplication-rounding ULP.
    const double tenths = std::ceil(std::nextafter(std::abs(delta_ms) * 10.0, 0.0));
    if (!std::isfinite(tenths) ||
        tenths >= static_cast<double>((std::numeric_limits<std::int64_t>::max)())) {
        return {};
    }
    const auto magnitude = (std::max)(std::int64_t{1}, static_cast<std::int64_t>(tenths));
    const std::wstring value = std::to_wstring(magnitude / 10) + L"." +
                              std::to_wstring(magnitude % 10) + L" ms";
    return (delta_ms < 0.0 ? L"FAST -" : L"SLOW +") + value;
}

// The session selects the retained error independently of the current grade.
inline std::wstring gameplay_timing_feedback_text(double delta_ms,
                                                  std::string_view /*judgement*/) {
    return gameplay_timing_feedback_text(delta_ms);
}

}  // namespace tenriff::render
