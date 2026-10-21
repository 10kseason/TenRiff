#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::render {

inline std::uint32_t gameplay_gauge_color(std::string_view gauge_token) {
    if (gauge_token == "EX-HARD") {
        return 0x292C31;
    }
    if (gauge_token == "HARD") {
        return 0xFF4D6D;
    }
    if (gauge_token == "EASY") {
        return 0x89D185;
    }
    return 0xFFB703;
}

inline std::uint32_t song_select_gauge_text_color(std::string_view gauge_tier) {
    if (gauge_tier == "ex_hard") {
        return 0xFF4D6D;
    }
    if (gauge_tier == "hard") {
        return 0xFF9F43;
    }
    if (gauge_tier == "easy") {
        return 0x5EE59A;
    }
    return 0xFFE45E;
}

struct SongSelectLampPresentation {
    std::string label;
    std::optional<std::uint32_t> color;
    bool tint_card = false;
};

inline SongSelectLampPresentation song_select_lamp_presentation(std::string_view status) {
    SongSelectLampPresentation result{std::string(status), std::nullopt, false};
    constexpr std::string_view shift = "GAUGE SHIFT ";
    // Presentation only: stored statuses still distinguish Gauge Shift records.
    // Keep assisted-play prefixes visible and never tint them as a normal clear.
    const bool assisted = status.substr(0, 7) == "ASSIST ";
    const auto shift_at = result.label.find(shift);
    if (shift_at == 0 || (assisted && shift_at != std::string::npos)) {
        result.label.erase(shift_at, shift.size());
    }
    const std::string_view label(result.label);
    if (label == "EX CLEAR" || label == "EX-HARD CLEAR") {
        result.color = song_select_gauge_text_color("ex_hard");
    } else if (label == "HARD CLEAR") {
        result.color = song_select_gauge_text_color("hard");
    } else if (label == "EASY CLEAR") {
        result.color = song_select_gauge_text_color("easy");
    } else if (label == "CLEAR" || label == "NORMAL CLEAR") {
        result.color = song_select_gauge_text_color("normal");
    }
    result.tint_card = result.color.has_value();
    return result;
}

}  // namespace tenriff::render
