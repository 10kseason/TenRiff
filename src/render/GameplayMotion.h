#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tenriff::render {

struct GameplayMotionState {
    int64_t current_sample = 0;
    int64_t duration_samples = 0;
    int sample_rate = 48000;
    int64_t audio_sample_time_ns = 0;
    int64_t hud_publish_time_ns = 0;
    uint32_t audio_buffer_frames = 0;
    double visual_offset_ms = 0.0;
    bool finished = false;
    bool game_over = false;
};

struct GameplayMotionDiagnostics {
    int64_t display_sample = 0;
    int64_t extrapolated_samples = 0;
    int64_t extrapolation_limit_samples = 0;
    double audio_age_ms = 0.0;
    double hud_delta_ms = 0.0;
    double extrapolated_ms = 0.0;
    double buffer_ms = 0.0;
};

inline double compute_gameplay_note_y_normalized(int64_t sample,
                                                 int64_t display_sample,
                                                 int64_t lookahead_samples,
                                                 int64_t past_samples,
                                                 double judgement_line_position) {
    const double clamped_judgement_line = std::clamp(judgement_line_position, 0.0, 1.0);
    const double delta = static_cast<double>(sample - display_sample);
    if (delta >= 0.0) {
        if (lookahead_samples <= 0) {
            return clamped_judgement_line;
        }
        const double t = std::clamp(delta / static_cast<double>(lookahead_samples), 0.0, 1.0);
        return std::clamp(clamped_judgement_line * (1.0 - t), 0.0, 1.0);
    }

    if (past_samples <= 0) {
        return clamped_judgement_line;
    }
    const double t = std::clamp((-delta) / static_cast<double>(past_samples), 0.0, 1.0);
    return std::clamp(clamped_judgement_line + t * (1.0 - clamped_judgement_line), 0.0, 1.0);
}

inline int64_t gameplay_extrapolation_limit_samples(int sample_rate, uint32_t audio_buffer_frames) {
    if (sample_rate <= 0) {
        return 0;
    }
    const int64_t min_limit = static_cast<int64_t>(std::llround(
        24.0 * static_cast<double>(sample_rate) / 1000.0));
    const int64_t buffer_limit = static_cast<int64_t>(audio_buffer_frames) * 2;
    return std::max<int64_t>(min_limit, buffer_limit);
}

inline GameplayMotionDiagnostics compute_gameplay_motion_diagnostics(const GameplayMotionState& state,
                                                                    int64_t now_ns) {
    GameplayMotionDiagnostics diagnostics;
    diagnostics.display_sample = std::max<int64_t>(0, state.current_sample);

    if (state.sample_rate > 0) {
        diagnostics.buffer_ms =
            static_cast<double>(state.audio_buffer_frames) * 1000.0 / static_cast<double>(state.sample_rate);
    }

    if (state.audio_sample_time_ns > 0) {
        diagnostics.audio_age_ms =
            static_cast<double>(std::max<int64_t>(0, now_ns - state.audio_sample_time_ns)) / 1'000'000.0;
    }
    if (state.hud_publish_time_ns > 0 && state.audio_sample_time_ns > 0) {
        diagnostics.hud_delta_ms =
            static_cast<double>(std::max<int64_t>(0, state.hud_publish_time_ns - state.audio_sample_time_ns)) /
            1'000'000.0;
    }

    if (state.sample_rate <= 0 || state.audio_sample_time_ns <= 0 || state.finished || state.game_over) {
        if (state.sample_rate > 0 && std::isfinite(state.visual_offset_ms)) {
            diagnostics.display_sample += static_cast<int64_t>(std::llround(
                state.visual_offset_ms * static_cast<double>(state.sample_rate) / 1000.0));
            diagnostics.display_sample = std::max<int64_t>(0, diagnostics.display_sample);
            if (state.duration_samples > 0) {
                diagnostics.display_sample = std::min(diagnostics.display_sample, state.duration_samples);
            }
        }
        return diagnostics;
    }

    diagnostics.extrapolation_limit_samples =
        gameplay_extrapolation_limit_samples(state.sample_rate, state.audio_buffer_frames);

    const int64_t elapsed_ns = std::max<int64_t>(0, now_ns - state.audio_sample_time_ns);
    const int64_t elapsed_samples = static_cast<int64_t>(std::llround(
        static_cast<double>(elapsed_ns) * static_cast<double>(state.sample_rate) / 1'000'000'000.0));
    diagnostics.extrapolated_samples = std::clamp<int64_t>(
        elapsed_samples,
        int64_t{0},
        diagnostics.extrapolation_limit_samples);
    diagnostics.extrapolated_ms =
        static_cast<double>(diagnostics.extrapolated_samples) * 1000.0 / static_cast<double>(state.sample_rate);

    diagnostics.display_sample += diagnostics.extrapolated_samples;
    if (std::isfinite(state.visual_offset_ms)) {
        diagnostics.display_sample += static_cast<int64_t>(std::llround(
            state.visual_offset_ms * static_cast<double>(state.sample_rate) / 1000.0));
    }
    diagnostics.display_sample = std::max<int64_t>(0, diagnostics.display_sample);
    if (state.duration_samples > 0) {
        diagnostics.display_sample = std::min(diagnostics.display_sample, state.duration_samples);
    }
    return diagnostics;
}

}  // namespace tenriff::render
