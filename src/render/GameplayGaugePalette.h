#pragma once

#include <cstdint>
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

}  // namespace tenriff::render
