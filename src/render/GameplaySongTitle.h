#pragma once

#include <algorithm>
#include <cmath>

namespace tenriff::render {

struct GameplaySongTitleLayout {
    float left = 84.0f;
    float top = 52.0f;
    float right = 344.0f;
    float bottom = 86.0f;
};

// Keep the title outside the lanes and graph. If dragging leaves only a sliver
// on the left, the free area below the right-hand HUD is more readable. This
// moves only the title; the player's field and authored skin geometry stay put.
inline GameplaySongTitleLayout compute_gameplay_song_title_layout(
    float field_left, float field_right, float left_safe_right, bool studio) {
    const float content_right = studio ? 1856.0f : 1836.0f;
    const float left_right = std::min(left_safe_right, field_left - 32.0f);
    // Classic has a 46px gauge at field_right + 60; leave its edge clear too.
    const float right_left = field_right + (studio ? 100.0f : 124.0f);
    const float left_width = left_right - 84.0f;
    const float right_width = content_right - right_left;
    const float height = studio ? 34.0f : 52.0f;
    GameplaySongTitleLayout result{84.0f, studio ? 52.0f : 42.0f, left_right, 0.0f};
    if (left_width < 160.0f && right_width > std::max(0.0f, left_right - 24.0f)) {
        result.left = right_left;
        result.right = content_right;
        result.top = 408.0f;
    } else if (left_width < 160.0f) {
        result.left = 24.0f;
    }
    result.bottom = result.top + height;
    return result;
}

// No lower font-size clamp: it would wrap or cut the end of sufficiently long
// titles. Use the measured glyph bounds, including overhang, rather than a
// character-count estimate so Korean, Latin and mixed titles fit consistently.
inline float gameplay_song_title_fit_scale(float text_width, float text_height,
                                           float available_width, float available_height) {
    if (!std::isfinite(text_width) || !std::isfinite(text_height) ||
        !std::isfinite(available_width) || !std::isfinite(available_height) ||
        text_width <= 0 || text_height <= 0 || available_width <= 0 || available_height <= 0)
        return 0.0f;
    return std::min({1.0f, available_width / text_width, available_height / text_height});
}

}  // namespace tenriff::render
