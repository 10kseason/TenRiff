#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "GameplayHudLimits.h"
#include "game/GaugeManager.h"
#include "gameplay/ResultStats.h"

namespace tenriff::app {

struct GameplayHudRevisionNote {
    int lane = 1;
    int64_t start_sample = 0;
    int64_t tail_sample = 0;
    bool hold = false;
    bool head_visible = true;
};

struct GameplayHudRevisionInput {
    bool active = false;
    bool finished = false;
    bool game_over = false;
    bool user_aborted = false;
    bool loading = false;
    bool countdown_active = false;
    int countdown_value = 0;
    int loading_percent = 0;
    std::string loading_stage;

    int lane_count = 10;
    int64_t current_sample = 0;
    int64_t duration_samples = 0;
    int sample_rate = 48000;
    int64_t audio_sample_time_ns = 0;
    uint32_t audio_buffer_frames = 0;
    int64_t lookahead_samples = 0;
    int64_t past_samples = 0;

    int combo = 0;
    int max_combo = 0;
    gameplay::JudgementCounts counts;

    double gauge = 0.0;
    game::GaugeType gauge_type = game::GaugeType::Normal;

    double rate = 1.0;
    double hispeed = 3.0;

    bool has_feedback = false;
    game::Judgement feedback = game::Judgement::BD;
    double feedback_delta_ms = 0.0;

    std::size_t lane_activity_count = 0;
    std::array<float, kGameplayHudMaxLanes> lane_activity{};
    std::size_t note_count = 0;
    std::array<GameplayHudRevisionNote, kGameplayHudMaxNotes> notes{};
};

struct GameplayHudRevisionFlags {
    bool motion_changed = false;
    bool text_changed = false;
};

inline bool gameplay_hud_notes_equal(const GameplayHudRevisionInput& lhs, const GameplayHudRevisionInput& rhs) {
    if (lhs.note_count != rhs.note_count) {
        return false;
    }
    return std::equal(lhs.notes.begin(),
                      lhs.notes.begin() + static_cast<std::ptrdiff_t>(lhs.note_count),
                      rhs.notes.begin(),
                      [](const GameplayHudRevisionNote& left, const GameplayHudRevisionNote& right) {
                          return left.lane == right.lane &&
                                 left.start_sample == right.start_sample &&
                                 left.tail_sample == right.tail_sample &&
                                 left.hold == right.hold &&
                                 left.head_visible == right.head_visible;
                      });
}

inline bool gameplay_hud_lane_activity_equal(const GameplayHudRevisionInput& lhs,
                                             const GameplayHudRevisionInput& rhs) {
    if (lhs.lane_activity_count != rhs.lane_activity_count) {
        return false;
    }
    return std::equal(lhs.lane_activity.begin(),
                      lhs.lane_activity.begin() + static_cast<std::ptrdiff_t>(lhs.lane_activity_count),
                      rhs.lane_activity.begin());
}

inline GameplayHudRevisionFlags diff_gameplay_hud_revisions(const GameplayHudRevisionInput& previous,
                                                            const GameplayHudRevisionInput& next) {
    GameplayHudRevisionFlags flags;
    flags.text_changed =
        previous.combo != next.combo ||
        previous.max_combo != next.max_combo ||
        previous.counts.pg != next.counts.pg ||
        previous.counts.gr != next.counts.gr ||
        previous.counts.gd != next.counts.gd ||
        previous.counts.bd != next.counts.bd ||
        previous.gauge != next.gauge ||
        previous.gauge_type != next.gauge_type ||
        previous.rate != next.rate ||
        previous.hispeed != next.hispeed ||
        previous.has_feedback != next.has_feedback ||
        previous.feedback != next.feedback;

    flags.motion_changed =
        previous.active != next.active ||
        previous.finished != next.finished ||
        previous.game_over != next.game_over ||
        previous.user_aborted != next.user_aborted ||
        previous.loading != next.loading ||
        previous.countdown_active != next.countdown_active ||
        previous.countdown_value != next.countdown_value ||
        previous.loading_percent != next.loading_percent ||
        previous.loading_stage != next.loading_stage ||
        previous.lane_count != next.lane_count ||
        previous.current_sample != next.current_sample ||
        previous.duration_samples != next.duration_samples ||
        previous.sample_rate != next.sample_rate ||
        previous.audio_sample_time_ns != next.audio_sample_time_ns ||
        previous.audio_buffer_frames != next.audio_buffer_frames ||
        previous.lookahead_samples != next.lookahead_samples ||
        previous.past_samples != next.past_samples ||
        previous.feedback_delta_ms != next.feedback_delta_ms ||
        !gameplay_hud_lane_activity_equal(previous, next) ||
        !gameplay_hud_notes_equal(previous, next);

    return flags;
}

}  // namespace tenriff::app
