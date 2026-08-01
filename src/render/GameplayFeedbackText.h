#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace tenriff::render {

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

}  // namespace tenriff::render