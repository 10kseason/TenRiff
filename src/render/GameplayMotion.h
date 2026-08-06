#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

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

struct GameplayNoteShapeExtents {
    float half_width = 0.0f;
    float half_height = 0.0f;
};
struct GameplayTextPopAnimation {
    float scale = 1.0f;
    float offset_y = 0.0f;
    float opacity = 1.0f;
};

inline GameplayTextPopAnimation compute_gameplay_text_pop_animation(
    double age_ms,
    double duration_ms,
    float initial_scale,
    float initial_offset_y) {
    if (!std::isfinite(age_ms) || !std::isfinite(duration_ms) || duration_ms <= 0.0) {
        return {};
    }
    const double progress = std::clamp(age_ms / duration_ms, 0.0, 1.0);
    const float remaining = static_cast<float>(1.0 - progress);
    const float eased = remaining * remaining;
    GameplayTextPopAnimation animation;
    animation.scale = 1.0f + (std::max(1.0f, initial_scale) - 1.0f) * eased;
    animation.offset_y = initial_offset_y * eased;
    animation.opacity = 1.0f - 0.12f * eased;
    return animation;
}

inline float gameplay_playfield_scale(double note_width_scale) {
    constexpr double kMinScale = 0.50;
    constexpr double kMaxScale = 1.40;
    if (!std::isfinite(note_width_scale)) {
        return 1.0f;
    }
    return static_cast<float>(std::clamp(note_width_scale, kMinScale, kMaxScale));
}

inline float compute_gameplay_playfield_width(float baseline_width, double note_width_scale) {
    return std::max(1.0f, baseline_width) * gameplay_playfield_scale(note_width_scale);
}

// Clearance between a note edge and the lane divider line, in base-space pixels at
// 100% note size. The default of 12 per side reproduces the historic 24px inset.
inline constexpr double kGameplayNoteDividerGapPxDefault = 12.0;
inline constexpr double kGameplayNoteDividerGapPxMin = 0.0;
inline constexpr double kGameplayNoteDividerGapPxMax = 40.0;

// The divider line sits half a lane gap outside the lane box, so lane width plus
// the adjacent gap is the slot a note may fill. `divider_gap_px` takes clearance
// off both edges of that slot; an imported skin's authored ratio then fills what
// remains, which is why LR2 art can still render narrower than the setting.
inline float compute_gameplay_note_draw_width(float lane_width,
                                              double note_width_scale,
                                              double imported_note_width_ratio = 1.0,
                                              double divider_gap_px = kGameplayNoteDividerGapPxDefault,
                                              float adjacent_lane_gap_px = 0.0f) {
    const float field_scale = gameplay_playfield_scale(note_width_scale);
    const float scaled_minimum_width = 16.0f * field_scale;
    const float safe_lane_width = std::max(scaled_minimum_width, lane_width);
    const float safe_adjacent_gap =
        (std::isfinite(adjacent_lane_gap_px) && adjacent_lane_gap_px > 0.0f) ? adjacent_lane_gap_px : 0.0f;
    const float slot_width = safe_lane_width + safe_adjacent_gap;
    const double clamped_gap_px =
        std::isfinite(divider_gap_px)
            ? std::clamp(divider_gap_px, kGameplayNoteDividerGapPxMin, kGameplayNoteDividerGapPxMax)
            : kGameplayNoteDividerGapPxDefault;
    const float base_note_width = std::max(
        scaled_minimum_width, slot_width - 2.0f * static_cast<float>(clamped_gap_px) * field_scale);
    const float imported_ratio =
        (!std::isfinite(imported_note_width_ratio) || imported_note_width_ratio <= 0.0)
            ? 1.0f
            : static_cast<float>(std::clamp(imported_note_width_ratio, 0.25, 4.0));
    return std::clamp(base_note_width * imported_ratio, scaled_minimum_width, slot_width);
}

inline GameplayNoteShapeExtents gameplay_note_shape_extents(float width,
                                                            float height,
                                                            std::string_view shape) {
    std::string normalized(shape);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    const float safe_width = std::max(2.0f, width);
    const float safe_height = std::max(2.0f, height);
    // These shapes are drawn inside a square box so they stay regular, taking the
    // lane width as their side rather than the note rect's height.
    const bool lane_width_shape = normalized == "circle" || normalized == "triangle" ||
                                  normalized == "pentagon" || normalized == "hexagon" ||
                                  normalized == "square" || normalized == "diamond" ||
                                  normalized == "arrow";
    return GameplayNoteShapeExtents{
        safe_width * 0.5f,
        (lane_width_shape ? safe_width : safe_height) * 0.5f,
    };
}

inline double compute_gameplay_note_y_normalized(int64_t sample,
                                                 int64_t display_sample,
                                                 int64_t lookahead_samples,
                                                 int64_t past_samples,
                                                 double judgement_line_position) {
    constexpr double kFutureEntryOvershoot = 0.12;
    const double clamped_judgement_line = std::clamp(judgement_line_position, 0.0, 1.0);
    const double delta = static_cast<double>(sample - display_sample);
    if (delta >= 0.0) {
        if (lookahead_samples <= 0) {
            return clamped_judgement_line;
        }
        const double t = std::clamp(delta / static_cast<double>(lookahead_samples), 0.0, 1.0);
        return std::clamp(clamped_judgement_line - t * (clamped_judgement_line + kFutureEntryOvershoot),
                          -kFutureEntryOvershoot,
                          1.0);
    }

    if (past_samples <= 0) {
        return clamped_judgement_line;
    }
    const double t = std::clamp((-delta) / static_cast<double>(past_samples), 0.0, 1.0);
    return std::clamp(clamped_judgement_line + t * (1.0 - clamped_judgement_line), 0.0, 1.0);
}

inline double compute_gameplay_visual_y_normalized(double position,
                                                   double display_position,
                                                   double future_span,
                                                   double past_span,
                                                   double judgement_line_position) {
    constexpr double kFutureEntryOvershoot = 0.12;
    const double line = std::clamp(judgement_line_position, 0.0, 1.0);
    const double delta = position - display_position;
    if (delta >= 0.0) {
        const double span = std::max(1e-9, std::abs(future_span));
        const double t = std::clamp(delta / span, 0.0, 1.0);
        return std::clamp(line - t * (line + kFutureEntryOvershoot),
                          -kFutureEntryOvershoot, 1.0);
    }
    const double span = std::max(1e-9, std::abs(past_span));
    const double t = std::clamp((-delta) / span, 0.0, 1.0);
    return std::clamp(line + t * (1.0 - line), 0.0, 1.0);
}

inline bool should_render_gameplay_note(int64_t start_sample,
                                        int64_t tail_sample,
                                        bool hold,
                                        bool head_visible,
                                        bool pending,
                                        int64_t snapshot_sample,
                                        int64_t display_sample,
                                        int64_t handoff_grace_samples) {
    if (!hold) {
        return head_visible && start_sample >= display_sample;
    }
    if (!head_visible) {
        return tail_sample >= display_sample;
    }
    if (start_sample >= display_sample) {
        return true;
    }
    if (pending && tail_sample >= display_sample) {
        return true;
    }

    // Bridge only the stale pre-hit snapshot into the next active-hold HUD
    // update. Keeping the original body until its tail would make a missed LN
    // look held even though no headless active-hold snapshot was produced.
    return snapshot_sample <= start_sample &&
           tail_sample >= display_sample &&
           display_sample - start_sample <= std::max<int64_t>(0, handoff_grace_samples);
}

inline bool should_render_gameplay_note_head(int64_t start_sample,
                                             bool head_visible,
                                             int64_t display_sample) {
    return head_visible && start_sample >= display_sample;
}

inline bool gameplay_note_anchors_to_judgement_line(bool hold, bool head_visible) {
    return hold && !head_visible;
}

inline int64_t gameplay_note_render_sample(int64_t start_sample,
                                           bool hold,
                                           bool head_visible,
                                           int64_t display_sample) {
    return gameplay_note_anchors_to_judgement_line(hold, head_visible) ? display_sample : start_sample;
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

inline int64_t gameplay_hold_handoff_grace_samples(int sample_rate,
                                                   int64_t extrapolation_limit_samples) {
    if (sample_rate <= 0) {
        return std::max<int64_t>(0, extrapolation_limit_samples);
    }
    constexpr double kHudRefreshMarginMs = 8.0;
    constexpr double kMaxHandoffGraceMs = 64.0;
    const int64_t hud_refresh_margin = static_cast<int64_t>(std::llround(
        kHudRefreshMarginMs * static_cast<double>(sample_rate) / 1000.0));
    const int64_t max_grace = static_cast<int64_t>(std::llround(
        kMaxHandoffGraceMs * static_cast<double>(sample_rate) / 1000.0));
    const int64_t extrapolation_grace = std::clamp<int64_t>(
        extrapolation_limit_samples, 0, max_grace);
    return std::min(max_grace, extrapolation_grace + hud_refresh_margin);
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
