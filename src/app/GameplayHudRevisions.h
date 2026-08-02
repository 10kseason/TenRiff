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
    bool pending = false;
};

struct GameplayHudRevisionInput {
    bool active = false;
    bool finished = false;
    bool game_over = false;
    bool spectating_peer = false;
    bool user_aborted = false;
    bool paused = false;
    int pause_menu_cursor = 0;
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
    int64_t score = 0;
    bool osu_od8_score_available = false;
    int64_t osu_od8_score = 0;

    double gauge = 0.0;
    game::GaugeType gauge_type = game::GaugeType::Normal;

    double rate = 1.0;
    double hispeed = 3.0;

    bool has_feedback = false;
    game::Judgement feedback = game::Judgement::BD;
    double feedback_delta_ms = 0.0;
    std::uint64_t peer_revision = 0;
    std::size_t timing_history_count = 0;
    std::array<double, kGameplayTimingHistoryMaxEntries> timing_history_delta_ms{};

    std::size_t lane_activity_count = 0;
    std::array<float, kGameplayHudMaxLanes> lane_activity{};
    std::size_t lane_pressed_count = 0;
    std::array<uint8_t, kGameplayHudMaxLanes> lane_pressed{};
    std::size_t note_count = 0;
    std::array<GameplayHudRevisionNote, kGameplayHudMaxNotes> notes{};

    bool ghost_visible = false;
    int64_t ghost_score = 0;
    bool ghost_osu_od8_score_available = false;
    int64_t ghost_osu_od8_score = 0;
    int ghost_combo = 0;
    int ghost_max_combo = 0;
    gameplay::JudgementCounts ghost_counts;
    double ghost_gauge = 0.0;
    game::GaugeType ghost_gauge_type = game::GaugeType::Normal;
    bool ghost_has_feedback = false;
    game::Judgement ghost_feedback = game::Judgement::BD;
    double ghost_feedback_delta_ms = 0.0;
    std::size_t ghost_timing_history_count = 0;
    std::array<double, kGameplayTimingHistoryMaxEntries> ghost_timing_history_delta_ms{};
    bool ghost_finished = false;
    bool ghost_game_over = false;
    std::size_t ghost_lane_activity_count = 0;
    std::array<float, kGameplayHudMaxLanes> ghost_lane_activity{};
    std::size_t ghost_lane_pressed_count = 0;
    std::array<uint8_t, kGameplayHudMaxLanes> ghost_lane_pressed{};
    std::size_t ghost_note_count = 0;
    std::array<GameplayHudRevisionNote, kGameplayHudMaxNotes> ghost_notes{};
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
                                 left.head_visible == right.head_visible &&
                                 left.pending == right.pending;
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

inline bool gameplay_hud_lane_pressed_equal(const GameplayHudRevisionInput& lhs,
                                            const GameplayHudRevisionInput& rhs) {
    if (lhs.lane_pressed_count != rhs.lane_pressed_count) {
        return false;
    }
    return std::equal(lhs.lane_pressed.begin(),
                      lhs.lane_pressed.begin() + static_cast<std::ptrdiff_t>(lhs.lane_pressed_count),
                      rhs.lane_pressed.begin());
}

inline bool gameplay_hud_timing_history_equal(const GameplayHudRevisionInput& lhs,
                                              const GameplayHudRevisionInput& rhs) {
    if (lhs.timing_history_count != rhs.timing_history_count) {
        return false;
    }
    return std::equal(lhs.timing_history_delta_ms.begin(),
                      lhs.timing_history_delta_ms.begin() +
                          static_cast<std::ptrdiff_t>(lhs.timing_history_count),
                      rhs.timing_history_delta_ms.begin());
}

inline bool gameplay_hud_ghost_notes_equal(const GameplayHudRevisionInput& lhs,
                                           const GameplayHudRevisionInput& rhs) {
    if (lhs.ghost_note_count != rhs.ghost_note_count) {
        return false;
    }
    return std::equal(lhs.ghost_notes.begin(),
                      lhs.ghost_notes.begin() + static_cast<std::ptrdiff_t>(lhs.ghost_note_count),
                      rhs.ghost_notes.begin(),
                      [](const GameplayHudRevisionNote& left, const GameplayHudRevisionNote& right) {
                          return left.lane == right.lane &&
                                 left.start_sample == right.start_sample &&
                                 left.tail_sample == right.tail_sample &&
                                 left.hold == right.hold &&
                                 left.head_visible == right.head_visible &&
                                 left.pending == right.pending;
                      });
}

inline bool gameplay_hud_ghost_lane_activity_equal(const GameplayHudRevisionInput& lhs,
                                                   const GameplayHudRevisionInput& rhs) {
    if (lhs.ghost_lane_activity_count != rhs.ghost_lane_activity_count) {
        return false;
    }
    return std::equal(lhs.ghost_lane_activity.begin(),
                      lhs.ghost_lane_activity.begin() +
                          static_cast<std::ptrdiff_t>(lhs.ghost_lane_activity_count),
                      rhs.ghost_lane_activity.begin());
}

inline bool gameplay_hud_ghost_lane_pressed_equal(const GameplayHudRevisionInput& lhs,
                                                  const GameplayHudRevisionInput& rhs) {
    if (lhs.ghost_lane_pressed_count != rhs.ghost_lane_pressed_count) {
        return false;
    }
    return std::equal(lhs.ghost_lane_pressed.begin(),
                      lhs.ghost_lane_pressed.begin() +
                          static_cast<std::ptrdiff_t>(lhs.ghost_lane_pressed_count),
                      rhs.ghost_lane_pressed.begin());
}

inline bool gameplay_hud_ghost_timing_history_equal(const GameplayHudRevisionInput& lhs,
                                                    const GameplayHudRevisionInput& rhs) {
    if (lhs.ghost_timing_history_count != rhs.ghost_timing_history_count) {
        return false;
    }
    return std::equal(lhs.ghost_timing_history_delta_ms.begin(),
                      lhs.ghost_timing_history_delta_ms.begin() +
                          static_cast<std::ptrdiff_t>(lhs.ghost_timing_history_count),
                      rhs.ghost_timing_history_delta_ms.begin());
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
        previous.counts.pr != next.counts.pr ||
        previous.score != next.score ||
        previous.osu_od8_score_available != next.osu_od8_score_available ||
        previous.osu_od8_score != next.osu_od8_score ||
        previous.gauge != next.gauge ||
        previous.gauge_type != next.gauge_type ||
        previous.rate != next.rate ||
        previous.hispeed != next.hispeed ||
        previous.has_feedback != next.has_feedback ||
        previous.feedback != next.feedback ||
        previous.feedback_delta_ms != next.feedback_delta_ms ||
        previous.paused != next.paused ||
        previous.pause_menu_cursor != next.pause_menu_cursor ||
        previous.spectating_peer != next.spectating_peer ||
        previous.peer_revision != next.peer_revision ||
        previous.ghost_visible != next.ghost_visible ||
        previous.ghost_score != next.ghost_score ||
        previous.ghost_osu_od8_score_available != next.ghost_osu_od8_score_available ||
        previous.ghost_osu_od8_score != next.ghost_osu_od8_score ||
        previous.ghost_combo != next.ghost_combo ||
        previous.ghost_max_combo != next.ghost_max_combo ||
        previous.ghost_counts.pg != next.ghost_counts.pg ||
        previous.ghost_counts.gr != next.ghost_counts.gr ||
        previous.ghost_counts.gd != next.ghost_counts.gd ||
        previous.ghost_counts.bd != next.ghost_counts.bd ||
        previous.ghost_counts.pr != next.ghost_counts.pr ||
        previous.ghost_gauge != next.ghost_gauge ||
        previous.ghost_gauge_type != next.ghost_gauge_type ||
        previous.ghost_has_feedback != next.ghost_has_feedback ||
        previous.ghost_feedback != next.ghost_feedback ||
        previous.ghost_feedback_delta_ms != next.ghost_feedback_delta_ms;

    flags.motion_changed =
        previous.active != next.active ||
        previous.finished != next.finished ||
        previous.game_over != next.game_over ||
        previous.spectating_peer != next.spectating_peer ||
        previous.user_aborted != next.user_aborted ||
        previous.paused != next.paused ||
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
        !gameplay_hud_timing_history_equal(previous, next) ||
        !gameplay_hud_lane_activity_equal(previous, next) ||
        !gameplay_hud_lane_pressed_equal(previous, next) ||
        !gameplay_hud_notes_equal(previous, next) ||
        previous.ghost_finished != next.ghost_finished ||
        previous.ghost_game_over != next.ghost_game_over ||
        previous.ghost_feedback_delta_ms != next.ghost_feedback_delta_ms ||
        !gameplay_hud_ghost_timing_history_equal(previous, next) ||
        !gameplay_hud_ghost_lane_activity_equal(previous, next) ||
        !gameplay_hud_ghost_lane_pressed_equal(previous, next) ||
        !gameplay_hud_ghost_notes_equal(previous, next);

    return flags;
}

}  // namespace tenriff::app
