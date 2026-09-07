#pragma once

#include <cmath>
#include <cstdint>
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

    const auto rounded_ms = static_cast<std::int64_t>(std::llround(delta_ms));
    if (rounded_ms == 0) {
        return {};
    }

    if (rounded_ms < 0) {
        return L"FAST " + std::to_wstring(rounded_ms) + L" ms";
    }
    return L"SLOW +" + std::to_wstring(rounded_ms) + L" ms";
}

// The highest judgement is already the target, so reporting how far off it was is
// noise. FAST/SLOW milliseconds stay reserved for judgements worth correcting.
inline std::wstring gameplay_timing_feedback_text(double delta_ms,
                                                  std::string_view judgement) {
    if (judgement == "PG") {
        return {};
    }
    return gameplay_timing_feedback_text(delta_ms);
}

}  // namespace tenriff::render
