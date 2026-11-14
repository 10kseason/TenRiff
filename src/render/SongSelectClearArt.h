#pragma once
#include "render/GameplayGaugePalette.h"

namespace tenriff::render {
// Row decoration only: badge colors, scoring and stored result semantics stay unchanged.
inline constexpr float kSongSelectClearArtLeft = 800.0f;
inline constexpr float kSongSelectClearArtSaturation = 1.2f;
// Row-vector 5x4 color matrix: boost chroma, keep gray and alpha unchanged.
inline constexpr std::array<float, 20> song_select_clear_art_color_matrix() {
    constexpr float s = kSongSelectClearArtSaturation;
    constexpr float r = .2126f * (1 - s), g = .7152f * (1 - s), b = .0722f * (1 - s);
    return {r+s, r, r, 0, g, g+s, g, 0, b, b, b+s, 0, 0, 0, 0, 1, 0, 0, 0, 0};
}
inline constexpr std::array<const char*, 5> kSongSelectClearArtFiles{
    "TENRIFF_RESULT_EASY_green.png", "TENRIFF_RESULT_NORMAL_blue.png",
    "TENRIFF_RESULT_HARD_red.png", "TENRIFF_RESULT_EX_galaxy.png",
    "TENRIFF_RESULT_FULL_COMBO_gold.png"};

inline std::optional<std::size_t> song_select_clear_art_index(std::string_view lamp, bool verified_full_combo) {
    const auto clear = song_select_lamp_presentation(lamp);
    // Neither a supplied FC flag nor a clear-like label promotes failed, assist,
    // autoplay, imported FC text, or unknown results to a clear decoration.
    if (clear.badge_label.empty()) return std::nullopt;
    if (verified_full_combo) return 4;
    if (clear.badge_label == "EZ CLEAR") return 0;
    if (clear.badge_label == "NM CLEAR") return 1;
    if (clear.badge_label == "HD CLEAR") return 2;
    return 3;
}
} // namespace tenriff::render
