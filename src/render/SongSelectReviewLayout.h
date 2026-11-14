#pragma once
#include <algorithm>
#include <cmath>

namespace tenriff::render {
inline constexpr float kSongReviewNavLeft = 551.0f;
inline constexpr float kSongReviewNavStep = 166.0f;
inline constexpr float kSongReviewNavWidth = 154.0f;
inline constexpr double kSongReviewMarqueeDwell = 2.0;
inline constexpr double kSongReviewMarqueePixelsPerSecond = 14.0;
// Records show one selected play; Songs show the selected card's strongest
// clear. Never borrow full-combo status from a different highest-EX play.
inline bool song_review_all_combo_badge(bool has_clear_badge, bool showing_records,
                                      bool selected_record_full_combo, bool selected_card_full_combo) {
    return has_clear_badge && (showing_records ? selected_record_full_combo : selected_card_full_combo);
}
// Ping-pong with a dwell at both complete endpoints. The period seam is at
// offset zero, so long titles never jump from the end straight to the start.
inline float song_review_marquee_offset(double seconds, float overflow, bool reduced_motion) {
    if (reduced_motion || !std::isfinite(seconds) || !std::isfinite(overflow) || overflow <= 0) return 0;
    const double travel = overflow / kSongReviewMarqueePixelsPerSecond;
    const double phase = std::fmod(std::max(0.0, seconds), 2 * (travel + kSongReviewMarqueeDwell));
    if (phase <= kSongReviewMarqueeDwell) return 0;
    if (phase < kSongReviewMarqueeDwell + travel)
        return static_cast<float>((phase - kSongReviewMarqueeDwell) * kSongReviewMarqueePixelsPerSecond);
    if (phase <= 2 * kSongReviewMarqueeDwell + travel) return overflow;
    return static_cast<float>(std::max(0.0, overflow -
        (phase - 2 * kSongReviewMarqueeDwell - travel) * kSongReviewMarqueePixelsPerSecond));
}
} // namespace tenriff::render
