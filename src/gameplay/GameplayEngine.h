#pragma once

#include <cstdint>
#include <optional>
#include <vector>

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
};

struct LaneState {
    std::vector<NoteEvent> notes;
    std::size_t next_index = 0;
    int64_t mask_until = 0;
    bool key_down = false;
    std::optional<HoldState> hold;
};

struct GameplayConfig {
    int sample_rate = 48000;
    double rate = 1.0;
    config::JudgeConfig judge;
    game::GaugeConfig gauge;
    game::GaugeType initial_gauge = game::GaugeType::Normal;
    double input_offset_ms = 0.0;
};

struct LiveJudgementFeedback {
    bool has_value = false;
    game::Judgement judgement = game::Judgement::PR;
    double delta_ms = 0.0;
    int64_t sample = 0;
};

struct ActiveHoldView {
    int lane = 0;
    int64_t end_sample = 0;
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
    void collect_active_holds(std::vector<ActiveHoldView>& out) const;

    [[nodiscard]] int lane_count() const { return lane_count_; }
    [[nodiscard]] int64_t duration_samples() const { return duration_samples_; }

private:
    void apply_judgement(game::Judgement judgement, double delta_ms, int64_t sample, double weight, bool breaks_combo);
    [[nodiscard]] std::optional<NoteEvent> try_hit_note(LaneState& lane, int64_t input_sample);
    void apply_poor(int64_t sample);
    void update_miss(LaneState& lane, int64_t current_sample);
    void update_hold(LaneState& lane, int64_t current_sample);
    void finalize_if_done(int64_t current_sample);
    void update_lane_input_state(LaneState& lane, input::InputState state, int64_t input_sample);

    [[nodiscard]] double samples_to_ms(int64_t samples) const;
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

    ResultStats stats_;
    ReplayTrace replay_;
    LiveJudgementFeedback live_feedback_;

    bool finished_ = false;
    bool game_over_ = false;
};

}  // namespace tenriff::gameplay
