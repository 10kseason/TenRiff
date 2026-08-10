#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "GameplayHudLimits.h"
#include "config/Config.h"
#include "game/GaugeManager.h"
#include "gameplay/GameplayChart.h"
#include "gameplay/Replay.h"
#include "gameplay/ResultStats.h"

namespace tenriff::gameplay {

struct JudgeWindowSamples {
    int64_t pg = 0;
    int64_t gr = 0;
    int64_t gd = 0;
    int64_t bd = 0;
    int64_t indirect_miss = 0;
    int64_t pr_early = 0;
    bool indirect_miss_enabled = false;
    int64_t hold_grace = 0;
    int64_t hold_break = 0;
    int64_t mask = 0;
};

struct HoldState {
    int64_t end_sample = 0;
    bool release_required = false;
    bool broken = false;
    bool release_active = false;
    int64_t release_sample = 0;
    double osu_head_delta_ms = 0.0;
    bool osu_released_during_body = false;
};

struct LaneState {
    std::vector<NoteEvent> notes;
    std::vector<MineEvent> mines;
    std::size_t next_index = 0;
    std::size_t next_mine_index = 0;
    int64_t mask_until = 0;
    bool key_down = false;
    std::optional<HoldState> hold;
};

struct GameplayConfig {
    int sample_rate = 48000;
    double rate = 1.0;
    config::JudgeConfig judge;
    game::GaugeConfig gauge;
    game::GaugeRuntimePolicy gauge_policy;
    game::GaugeType initial_gauge = game::GaugeType::Normal;
    std::optional<double> initial_gauge_value;
    bool gauge_shift_enabled = false;
    double input_offset_ms = 0.0;
    bool practice_no_fail_enabled = false;
    bool one_miss_fail_enabled = false;
};

struct LiveJudgementFeedback {
    bool has_value = false;
    game::Judgement judgement = game::Judgement::BD;
    double delta_ms = 0.0;
    int64_t sample = 0;
};

struct ActiveHoldView {
    int lane = 0;
    int64_t end_sample = 0;
};

struct MineTrigger {
    int lane = 0;
    int64_t sample = 0;
    std::size_t mine_id = 0;
    std::size_t audio_asset_id = kInvalidAudioAssetId;
};

class GameplayEngine {
public:
    GameplayEngine(const GameplayChart& chart, const GameplayConfig& config);

    [[nodiscard]] std::optional<NoteEvent> handle_input(int lane, input::InputState state, int64_t input_sample);
    void sync_input_state(int lane, input::InputState state, int64_t input_sample);
    void advance(int64_t current_sample);

    [[nodiscard]] bool is_finished() const { return finished_; }
    [[nodiscard]] bool is_game_over() const { return game_over_; }

    [[nodiscard]] const ResultStats& stats() const { return stats_; }
    [[nodiscard]] const game::GaugeState& gauge_state() const { return gauge_state_; }
    [[nodiscard]] const ReplayTrace& replay() const { return replay_; }
    [[nodiscard]] const LiveJudgementFeedback& live_feedback() const { return live_feedback_; }
    [[nodiscard]] bool is_note_pending(int lane, std::size_t note_id) const;
    [[nodiscard]] bool is_mine_pending(int lane, std::size_t mine_id) const;
    void drain_mine_triggers(std::vector<MineTrigger>& out);
    void collect_active_holds(std::vector<ActiveHoldView>& out) const;
    void collect_recent_timing_deltas(std::array<double, kGameplayTimingHistoryMaxEntries>& out,
                                      std::size_t* out_count) const;

    [[nodiscard]] int lane_count() const { return lane_count_; }
    [[nodiscard]] int64_t duration_samples() const { return duration_samples_; }

private:
    void apply_judgement(game::Judgement judgement,
                         double delta_ms,
                         int64_t sample,
                         double weight,
                         ComboImpact combo_impact,
                         bool osu_miss);
    [[nodiscard]] std::optional<NoteEvent> try_hit_note(LaneState& lane, int64_t input_sample);
    void apply_missed_note(const NoteEvent& note, int64_t sample);
    void apply_bad_miss(const NoteEvent& note, int64_t sample);
    void apply_indirect_miss(const NoteEvent& note, int64_t sample);
    void apply_empty_poor(int64_t sample);
    bool process_mines_until(LaneState& lane, int lane_number, int64_t sample);
    void detonate_mine(const MineEvent& mine, int lane_number);
    [[nodiscard]] OsuManiaJudgement record_osu_hold(const HoldState& hold,
                                                     int64_t release_sample,
                                                     bool forced_miss = false);
    void update_miss(LaneState& lane, int64_t current_sample);
    void update_hold(LaneState& lane, int64_t current_sample);
    void finalize_if_done(int64_t current_sample);
    void update_lane_input_state(LaneState& lane, input::InputState state, int64_t input_sample);
    [[nodiscard]] bool should_apply_early_empty_poor(const LaneState& lane, int64_t input_sample) const;
    void push_recent_timing_delta(double delta_ms);

    [[nodiscard]] double samples_to_ms(int64_t samples) const;
    [[nodiscard]] double detailed_accuracy_credit_for(game::Judgement judgement, double delta_ms) const;
    [[nodiscard]] JudgeWindowSamples build_windows(const config::JudgeConfig& judge, double rate) const;
    [[nodiscard]] game::Judgement classify_judgement(int64_t delta_samples) const;

    int lane_count_ = 0;
    int sample_rate_ = 48000;
    double rate_ = 1.0;
    int64_t duration_samples_ = 0;

    JudgeWindowSamples windows_;
    std::vector<LaneState> lanes_;

    game::GaugeManager gauge_manager_;
    game::GaugeState gauge_state_;
    std::array<game::GaugeState, 4> gauge_shift_states_{};
    std::size_t gauge_shift_start_index_ = 0;
    bool gauge_shift_enabled_ = false;

    ResultStats stats_;
    ReplayTrace replay_;
    LiveJudgementFeedback live_feedback_;
    std::vector<MineTrigger> pending_mine_triggers_;
    std::array<double, kGameplayTimingHistoryMaxEntries> recent_timing_deltas_{};
    std::size_t recent_timing_delta_count_ = 0;
    std::size_t recent_timing_delta_head_ = 0;

    bool finished_ = false;
    bool game_over_ = false;
    bool practice_no_fail_enabled_ = false;
    bool one_miss_fail_enabled_ = false;
};

}  // namespace tenriff::gameplay
