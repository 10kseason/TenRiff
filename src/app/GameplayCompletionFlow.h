#pragma once

#include <algorithm>
#include <cstdint>

namespace tenriff::app {

[[nodiscard]] inline constexpr int64_t gameplay_result_transition_sample(
    int64_t notes_finished_sample,
    int64_t chart_audio_end_sample,
    int64_t minimum_tail_samples) {
    const int64_t safe_finished = std::max<int64_t>(0, notes_finished_sample);
    const int64_t safe_audio_end = std::max<int64_t>(0, chart_audio_end_sample);
    const int64_t safe_tail = std::max<int64_t>(0, minimum_tail_samples);
    return std::max(safe_audio_end, safe_finished + safe_tail);
}

[[nodiscard]] inline constexpr bool should_skip_post_note_audio(
    bool notes_finished,
    bool result_transition_pending,
    bool replay_playback,
    bool autoplay,
    bool lane_pressed) {
    return notes_finished && result_transition_pending && !replay_playback && !autoplay &&
           lane_pressed;
}

}  // namespace tenriff::app
