#pragma once

#include <string_view>

namespace tenriff::app {

// Authored metadata only. RANK 4 is the nanasigroove VERY EASY extension;
// this label must not silently expand the engine's supported judgement windows.
inline int bms_rank_for_display(std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return 3;
    const auto end = value.find_last_not_of(" \t\r\n");
    return begin == end && value[begin] >= '0' && value[begin] <= '4'
        ? value[begin] - '0' : 3;
}

inline std::string_view bms_rank_label(int rank) {
    constexpr std::string_view labels[]{"VHD", "HD", "NM", "EZ", "VEZ"};
    return rank >= 0 && rank <= 4 ? labels[rank] : std::string_view{};
}

} // namespace tenriff::app
