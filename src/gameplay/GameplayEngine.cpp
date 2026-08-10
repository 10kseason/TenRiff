#include "gameplay/GameplayEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tenriff::gameplay {

namespace {

double clamp_rate(double rate) {
    if (!std::isfinite(rate) || rate <= 0.0) {
        return 1.0;
    }
    return rate;
}

constexpr std::array<game::GaugeType, 4> kGaugeShiftPriority{
    game::GaugeType::ExHard,
    game::GaugeType::Hard,
    game::GaugeType::Normal,
    game::GaugeType::Easy,
};

}  // namespace

GameplayEngine::GameplayEngine(const GameplayChart& chart, const GameplayConfig& config)
    : lane_count_(chart.lane_count),
      sample_rate_(config.sample_rate),
      rate_(clamp_rate(config.rate)),
      duration_samples_(chart.duration_samples),
      windows_(build_windows(config.judge, config.rate)),
      gauge_manager_(config.gauge, config.gauge_shift_enabled ? game::GaugeRuntimePolicy{} : config.gauge_policy),
      gauge_shift_enabled_(config.gauge_shift_enabled),
      practice_no_fail_enabled_(config.practice_no_fail_enabled),
      one_miss_fail_enabled_(config.one_miss_fail_enabled) {
    if (lane_count_ <= 0) {
        lane_count_ = 10;
    }
    lanes_.resize(static_cast<std::size_t>(lane_count_));

    int playable_note_count = 0;
    int total_combo_steps = 0;
    for (const auto& note : chart.notes) {
        if (note.lane <= 0 || note.lane > lane_count_) {
            continue;
        }
        lanes_[static_cast<std::size_t>(note.lane - 1)].notes.push_back(note);
        ++playable_note_count;
        total_combo_steps += note.end_sample.has_value() ? 2 : 1;
    }
    for (const auto& mine : chart.mines) {
        if (mine.lane <= 0 || mine.lane > lane_count_) {
            continue;
        }
        lanes_[static_cast<std::size_t>(mine.lane - 1)].mines.push_back(mine);
    }


    for (auto& lane : lanes_) {
        std::stable_sort(lane.notes.begin(), lane.notes.end(), [](const NoteEvent& lhs, const NoteEvent& rhs) {
            return lhs.start_sample < rhs.start_sample;
        });
        std::stable_sort(lane.mines.begin(), lane.mines.end(), [](const MineEvent& lhs, const MineEvent& rhs) {
            return lhs.sample < rhs.sample;
        });
    }

    stats_.record_note_total(playable_note_count, total_combo_steps);
    if (gauge_shift_enabled_) {
        for (std::size_t i = 0; i < kGaugeShiftPriority.size(); ++i) {
            gauge_shift_states_[i] = gauge_manager_.initialState(kGaugeShiftPriority[i]);
        }
        const auto start = std::find(
            kGaugeShiftPriority.begin(), kGaugeShiftPriority.end(), config.initial_gauge);
        gauge_shift_start_index_ = start == kGaugeShiftPriority.end()
                                       ? 2u
                                       : static_cast<std::size_t>(
                                             std::distance(kGaugeShiftPriority.begin(), start));
        gauge_state_ = gauge_shift_states_[gauge_shift_start_index_];
    } else {
        gauge_state_ = gauge_manager_.initialState(config.initial_gauge);
        if (config.initial_gauge_value.has_value()) {
            const double carried_value = std::isfinite(config.initial_gauge_value.value())
                                             ? config.initial_gauge_value.value()
                                             : 100.0;
            gauge_state_.value = std::clamp(carried_value, 0.0, 100.0);
            gauge_state_.game_over = gauge_state_.value <= 0.0;
            game_over_ = gauge_state_.game_over;
        }
    }

    replay_.sample_rate = sample_rate_;
    replay_.rate = rate_;
    replay_.lane_count = lane_count_;
    replay_.duration_samples = duration_samples_;
    replay_.events.reserve(chart.notes.size() * 2);
}

std::optional<NoteEvent> GameplayEngine::handle_input(int lane, input::InputState state, int64_t input_sample) {
    if (finished_ || game_over_) {
        return std::nullopt;
    }
    if (lane <= 0 || lane > lane_count_) {
        return std::nullopt;
    }

    replay_.events.push_back(ReplayEvent{lane, state, input_sample});

    auto& lane_state = lanes_[static_cast<std::size_t>(lane - 1)];
    bool mine_detonated = false;
    if (input_sample > (std::numeric_limits<int64_t>::min)()) {
        mine_detonated = process_mines_until(lane_state, lane, input_sample - 1);
    }
    update_lane_input_state(lane_state, state, input_sample);
    mine_detonated = process_mines_until(lane_state, lane, input_sample) || mine_detonated;
    if (mine_detonated || game_over_) {
        return std::nullopt;
    }
    if (state == input::InputState::Pressed) {
        if (lane_state.mask_until > input_sample) {
            return std::nullopt;
        }
        const std::size_t previous_next_index = lane_state.next_index;
        auto hit_note = try_hit_note(lane_state, input_sample);
        if (!hit_note.has_value()) {
            if (previous_next_index == lane_state.next_index &&
                should_apply_early_empty_poor(lane_state, input_sample)) {
                apply_empty_poor(input_sample);
                lane_state.mask_until = input_sample + windows_.mask;
            }
            return std::nullopt;
        }
        return hit_note;
    }
    return std::nullopt;
}

void GameplayEngine::sync_input_state(int lane, input::InputState state, int64_t input_sample) {
    if (finished_ || game_over_) {
        return;
    }
    if (lane <= 0 || lane > lane_count_) {
        return;
    }

    replay_.events.push_back(ReplayEvent{lane, state, input_sample});
    auto& lane_state = lanes_[static_cast<std::size_t>(lane - 1)];
    update_lane_input_state(lane_state, state, input_sample);
}

void GameplayEngine::advance(int64_t current_sample) {
    if (finished_ || game_over_) {
        return;
    }

    for (std::size_t lane_index = 0; lane_index < lanes_.size(); ++lane_index) {
        auto& lane = lanes_[lane_index];
        process_mines_until(lane, static_cast<int>(lane_index) + 1, current_sample);
        if (game_over_) {
            break;
        }
        update_miss(lane, current_sample);
        if (game_over_) {
            break;
        }
        update_hold(lane, current_sample);
        if (game_over_) {
            break;
        }
    }

    finalize_if_done(current_sample);
}

bool GameplayEngine::is_note_pending(int lane, std::size_t note_id) const {
    if (lane <= 0 || lane > lane_count_) {
        return false;
    }
    const auto& lane_state = lanes_[static_cast<std::size_t>(lane - 1)];
    return lane_state.next_index < lane_state.notes.size() &&
           lane_state.notes[lane_state.next_index].note_id == note_id;
}

bool GameplayEngine::is_mine_pending(int lane, std::size_t mine_id) const {
    if (lane <= 0 || lane > lane_count_) {
        return false;
    }
    const auto& lane_state = lanes_[static_cast<std::size_t>(lane - 1)];
    return std::any_of(
        lane_state.mines.begin() + static_cast<std::ptrdiff_t>(lane_state.next_mine_index),
        lane_state.mines.end(),
        [mine_id](const MineEvent& mine) { return mine.mine_id == mine_id; });
}

void GameplayEngine::drain_mine_triggers(std::vector<MineTrigger>& out) {
    out = std::move(pending_mine_triggers_);
    pending_mine_triggers_.clear();
}

void GameplayEngine::collect_active_holds(std::vector<ActiveHoldView>& out) const {
    out.clear();
    out.reserve(lanes_.size());
    for (std::size_t lane_index = 0; lane_index < lanes_.size(); ++lane_index) {
        const auto& lane = lanes_[lane_index];
        if (!lane.hold.has_value()) {
            continue;
        }
        out.push_back(ActiveHoldView{
            static_cast<int>(lane_index) + 1,
            lane.hold->end_sample,
        });
    }
}

void GameplayEngine::collect_recent_timing_deltas(std::array<double, kGameplayTimingHistoryMaxEntries>& out,
                                                  std::size_t* out_count) const {
    const std::size_t count = std::min(recent_timing_delta_count_, recent_timing_deltas_.size());
    if (out_count) {
        *out_count = count;
    }
    out.fill(0.0);
    if (count == 0) {
        return;
    }

    const std::size_t start =
        (recent_timing_delta_head_ + recent_timing_deltas_.size() - count) % recent_timing_deltas_.size();
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = recent_timing_deltas_[(start + i) % recent_timing_deltas_.size()];
    }
}

bool GameplayEngine::process_mines_until(LaneState& lane, int lane_number, int64_t sample) {
    bool detonated = false;
    while (lane.next_mine_index < lane.mines.size() &&
           lane.mines[lane.next_mine_index].sample <= sample) {
        const MineEvent& mine = lane.mines[lane.next_mine_index++];
        if (!lane.key_down) {
            continue;
        }
        detonate_mine(mine, lane_number);
        lane.key_down = false;
        detonated = true;
        if (game_over_) {
            break;
        }
    }
    return detonated;
}

void GameplayEngine::detonate_mine(const MineEvent& mine, int lane_number) {
    pending_mine_triggers_.push_back(MineTrigger{
        lane_number, mine.sample, mine.mine_id, mine.audio_asset_id});

    const auto previous_type = gauge_state_.type;
    game::GaugeResult result{};
    if (mine.instant_kill) {
        if (gauge_shift_enabled_) {
            for (std::size_t i = gauge_shift_start_index_; i < gauge_shift_states_.size(); ++i) {
                auto& state = gauge_shift_states_[i];
                state.value = 0.0;
                state.game_over = true;
            }
            gauge_state_ = gauge_shift_states_.back();
        } else {
            gauge_state_.value = 0.0;
            gauge_state_.game_over = true;
        }
        result.game_over = true;
    } else if (gauge_shift_enabled_) {
        bool survivor_found = false;
        for (std::size_t i = gauge_shift_start_index_; i < gauge_shift_states_.size(); ++i) {
            auto& state = gauge_shift_states_[i];
            gauge_manager_.applyDamage(state, mine.damage_percent, samples_to_ms(mine.sample));
            if (!survivor_found && !state.game_over) {
                gauge_state_ = state;
                survivor_found = true;
            }
        }
        if (!survivor_found) {
            gauge_state_ = gauge_shift_states_.back();
        }
        result.downshifted = gauge_state_.type != previous_type;
        result.game_over = !survivor_found;
    } else {
        result = gauge_manager_.applyDamage(
            gauge_state_, mine.damage_percent, samples_to_ms(mine.sample));
    }

    stats_.record_gauge_sample(mine.sample, gauge_state_.value);
    if (result.downshifted) {
        stats_.record_shift(mine.sample, previous_type, gauge_state_.type);
    }
    if (mine.instant_kill || (result.game_over && !practice_no_fail_enabled_)) {
        game_over_ = true;
    }
}

void GameplayEngine::apply_judgement(game::Judgement judgement, double delta_ms, int64_t sample,
                                     double weight, ComboImpact combo_impact, bool osu_miss) {
    live_feedback_.has_value = true;
    live_feedback_.judgement = judgement;
    live_feedback_.delta_ms = std::isfinite(delta_ms) ? delta_ms : 0.0;
    live_feedback_.sample = sample;
    if (std::isfinite(delta_ms)) {
        push_recent_timing_delta(delta_ms);
    }

    const double time_ms = samples_to_ms(sample);
    const auto previous_type = gauge_state_.type;
    game::GaugeResult result{};
    if (gauge_shift_enabled_) {
        bool survivor_found = false;
        for (std::size_t i = gauge_shift_start_index_; i < gauge_shift_states_.size(); ++i) {
            auto& state = gauge_shift_states_[i];
            gauge_manager_.applyJudgementWeighted(state, judgement, time_ms, weight);
            if (!survivor_found && !state.game_over) {
                gauge_state_ = state;
                survivor_found = true;
            }
        }
        if (!survivor_found) {
            // Keep Easy visible at zero when every independently simulated
            // gauge has died so the final failure state remains unambiguous.
            gauge_state_ = gauge_shift_states_.back();
        }
        result.downshifted = gauge_state_.type != previous_type;
        result.game_over = !survivor_found;
    } else {
        result = gauge_manager_.applyJudgementWeighted(gauge_state_, judgement, time_ms, weight);
    }

    // Native BAD includes hittable timing errors, so it is not equivalent to
    // an osu!mania miss. Sudden Death follows the OD8 object judgement that is
    // shown in the result instead of killing valid late/early hold heads.
    const bool sudden_death_triggered = one_miss_fail_enabled_ && osu_miss;
    if (sudden_death_triggered) {
        gauge_state_.value = 0.0;
        gauge_state_.game_over = true;
        result.game_over = true;
    }

    stats_.record_judgement(judgement,
                            delta_ms,
                            combo_impact,
                            weight,
                            detailed_accuracy_credit_for(judgement, delta_ms));
    stats_.record_gauge_sample(sample, gauge_state_.value);

    if (result.downshifted) {
        stats_.record_shift(sample, previous_type, gauge_state_.type);
    }

    if (sudden_death_triggered || (result.game_over && !practice_no_fail_enabled_)) {
        game_over_ = true;
    }
}

double GameplayEngine::detailed_accuracy_credit_for(game::Judgement judgement, double delta_ms) const {
    if (judgement == game::Judgement::PR) {
        return 0.0;
    }
    if (!std::isfinite(delta_ms)) {
        return 0.0;
    }

    const double pg_window_ms = std::max(0.001, std::abs(samples_to_ms(windows_.pg)));
    const double normalized_error = std::abs(delta_ms) / pg_window_ms;
    // Detail accuracy is independent from the categorical label. Averaging
    // this smooth precision curve rewards smaller, steadier timing errors even
    // when two plays receive the same highest judgement counts.
    return std::clamp(std::exp(-0.125 * normalized_error * normalized_error), 0.0, 1.0);
}

void GameplayEngine::push_recent_timing_delta(double delta_ms) {
    if (!std::isfinite(delta_ms) || recent_timing_deltas_.empty()) {
        return;
    }

    recent_timing_deltas_[recent_timing_delta_head_] = delta_ms;
    recent_timing_delta_head_ = (recent_timing_delta_head_ + 1) % recent_timing_deltas_.size();
    if (recent_timing_delta_count_ < recent_timing_deltas_.size()) {
        ++recent_timing_delta_count_;
    }
}

std::optional<NoteEvent> GameplayEngine::try_hit_note(LaneState& lane, int64_t input_sample) {
    while (lane.next_index < lane.notes.size()) {
        const auto& note = lane.notes[lane.next_index];
        int64_t delta_samples = input_sample - note.start_sample;

        if (delta_samples > windows_.bd) {
            apply_missed_note(note, note.start_sample);
            lane.mask_until = std::max(lane.mask_until, note.start_sample + windows_.mask);
            ++lane.next_index;
            if (game_over_) {
                return std::nullopt;
            }
            continue;
        }

        if (std::abs(delta_samples) > windows_.bd) {
            return std::nullopt;
        }

        auto judgement = classify_judgement(delta_samples);
        if (judgement == game::Judgement::BD && lane.next_index + 1 < lane.notes.size()) {
            const auto& next_note = lane.notes[lane.next_index + 1];
            const int64_t next_delta_samples = input_sample - next_note.start_sample;

            // A wide BAD window must not let one missed note steal every later exact press in a dense stream.
            // Recover only when the immediate next note is unambiguously inside the non-BAD window.
            if (std::abs(next_delta_samples) <= windows_.gd) {
                apply_missed_note(note, note.start_sample);
                lane.mask_until = std::max(lane.mask_until, note.start_sample + windows_.mask);
                ++lane.next_index;
                if (game_over_) {
                    return std::nullopt;
                }
                continue;
            }
        }

        double delta_ms = static_cast<double>(delta_samples) * 1000.0 / static_cast<double>(sample_rate_);
        const ComboImpact combo_impact =
            (judgement == game::Judgement::BD) ? ComboImpact::Break : ComboImpact::Increment;

        const auto osu_head_judgement = classify_osu_mania_od8_tap(delta_ms);
        const bool osu_head_miss = osu_head_judgement == OsuManiaJudgement::Miss;
        double weight = note.end_sample.has_value() ? 0.5 : 1.0;
        if (!note.end_sample.has_value()) {
            record_osu_mania_od8_judgement(stats_.osu_od8, osu_head_judgement);
        }
        apply_judgement(judgement, delta_ms, input_sample, weight, combo_impact, osu_head_miss);

        if (note.end_sample.has_value()) {
            if (game_over_) {
                record_osu_mania_od8_judgement(stats_.osu_od8, OsuManiaJudgement::Miss);
            } else {
                HoldState hold;
                hold.end_sample = note.end_sample.value();
                hold.release_required = note.release_required;
                hold.osu_head_delta_ms = delta_ms;
                lane.hold = hold;
            }
        }

        NoteEvent hit_note = note;
        ++lane.next_index;
        return hit_note;
    }

    return std::nullopt;
}

void GameplayEngine::apply_missed_note(const NoteEvent& note, int64_t sample) {
    if (windows_.indirect_miss_enabled) {
        apply_indirect_miss(note, sample);
    } else {
        apply_bad_miss(note, sample);
    }
}

void GameplayEngine::apply_bad_miss(const NoteEvent& note, int64_t sample) {
    static_cast<void>(note);
    record_osu_mania_od8_judgement(stats_.osu_od8, OsuManiaJudgement::Miss);
    apply_judgement(game::Judgement::BD,
                    std::numeric_limits<double>::quiet_NaN(),
                    sample,
                    1.0,
                    ComboImpact::Break,
                    true);
}

void GameplayEngine::apply_indirect_miss(const NoteEvent& note, int64_t sample) {
    static_cast<void>(note);
    record_osu_mania_od8_judgement(stats_.osu_od8, OsuManiaJudgement::Miss);
    apply_judgement(game::Judgement::PR,
                    std::numeric_limits<double>::quiet_NaN(),
                    sample,
                    1.0,
                    ComboImpact::Break,
                    true);
}

void GameplayEngine::apply_empty_poor(int64_t sample) {
    apply_judgement(game::Judgement::PR,
                    std::numeric_limits<double>::quiet_NaN(),
                    sample,
                    1.0,
                    ComboImpact::Preserve,
                    false);
}

OsuManiaJudgement GameplayEngine::record_osu_hold(const HoldState& hold,
                                                   int64_t release_sample,
                                                   bool forced_miss) {
    const double tail_delta_ms = samples_to_ms(release_sample - hold.end_sample);
    const auto judgement = classify_osu_mania_od8_hold(hold.osu_head_delta_ms,
                                                        tail_delta_ms,
                                                        hold.osu_released_during_body,
                                                        forced_miss);
    record_osu_mania_od8_judgement(stats_.osu_od8, judgement);
    return judgement;
}

void GameplayEngine::update_miss(LaneState& lane, int64_t current_sample) {
    while (lane.next_index < lane.notes.size()) {
        const auto& note = lane.notes[lane.next_index];
        const int64_t miss_window = windows_.indirect_miss_enabled ? windows_.indirect_miss : windows_.bd;
        if (current_sample <= note.start_sample + miss_window) {
            break;
        }
        apply_missed_note(note, note.start_sample);
        lane.mask_until = note.start_sample + windows_.mask;
        ++lane.next_index;
        if (game_over_) {
            break;
        }
    }
}

void GameplayEngine::update_hold(LaneState& lane, int64_t current_sample) {
    if (!lane.hold.has_value()) {
        return;
    }

    auto& hold = *lane.hold;
    const int64_t hold_timeout = windows_.hold_break;

    if (hold.release_active) {
        if (hold.release_required) {
            const int64_t delta_samples = hold.release_sample - hold.end_sample;
            const auto judgement = classify_judgement(delta_samples);
            double delta_ms = static_cast<double>(delta_samples) * 1000.0 / static_cast<double>(sample_rate_);
            const ComboImpact combo_impact =
                (judgement == game::Judgement::BD) ? ComboImpact::Break : ComboImpact::Increment;
            const auto osu_judgement = record_osu_hold(hold, hold.release_sample);
            apply_judgement(judgement,
                            delta_ms,
                            hold.release_sample,
                            0.5,
                            combo_impact,
                            osu_judgement == OsuManiaJudgement::Miss);
            lane.hold.reset();
            return;
        }

        if (current_sample >= hold.end_sample) {
            const int64_t delta_samples = hold.release_sample - hold.end_sample;
            const auto judgement = classify_judgement(delta_samples);
            double delta_ms = static_cast<double>(delta_samples) * 1000.0 / static_cast<double>(sample_rate_);
            const ComboImpact combo_impact =
                (judgement == game::Judgement::BD) ? ComboImpact::Break : ComboImpact::Increment;
            const auto osu_judgement = record_osu_hold(hold, hold.release_sample);
            apply_judgement(judgement,
                            delta_ms,
                            hold.release_sample,
                            0.5,
                            combo_impact,
                            osu_judgement == OsuManiaJudgement::Miss);
            lane.hold.reset();
            return;
        }
    }

    if (hold.release_required) {
        if (current_sample > hold.end_sample + hold_timeout) {
            const auto osu_judgement = record_osu_hold(hold, hold.end_sample + hold_timeout, true);
            apply_judgement(game::Judgement::BD,
                            std::numeric_limits<double>::quiet_NaN(),
                            hold.end_sample + hold_timeout,
                            0.5,
                            ComboImpact::Break,
                            osu_judgement == OsuManiaJudgement::Miss);
            lane.hold.reset();
        }
        return;
    }

    if (current_sample >= hold.end_sample) {
        if (!hold.broken) {
            const int64_t delta_samples =
                hold.release_active ? (hold.release_sample - hold.end_sample) : 0;
            const auto judgement = classify_judgement(delta_samples);
            const double delta_ms = static_cast<double>(delta_samples) * 1000.0 / static_cast<double>(sample_rate_);
            const ComboImpact combo_impact =
                (judgement == game::Judgement::BD) ? ComboImpact::Break : ComboImpact::Increment;
            const int64_t osu_release_sample = hold.release_active ? hold.release_sample : hold.end_sample;
            const auto osu_judgement = record_osu_hold(hold, osu_release_sample);
            apply_judgement(judgement,
                            delta_ms,
                            hold.end_sample,
                            0.5,
                            combo_impact,
                            osu_judgement == OsuManiaJudgement::Miss);
        }
        lane.hold.reset();
    }
}

void GameplayEngine::finalize_if_done(int64_t current_sample) {
    static_cast<void>(current_sample);
    bool all_done = true;
    for (const auto& lane : lanes_) {
        if (lane.next_index < lane.notes.size()) {
            all_done = false;
            break;
        }
        if (lane.hold.has_value()) {
            all_done = false;
            break;
        }
    }

    if (all_done) {
        finished_ = true;
    }
}

void GameplayEngine::update_lane_input_state(LaneState& lane, input::InputState state, int64_t input_sample) {
    if (state == input::InputState::Pressed) {
        lane.key_down = true;
        if (lane.hold.has_value()) {
            lane.hold->release_active = false;
        }
        return;
    }

    lane.key_down = false;
    if (lane.hold.has_value()) {
        const int64_t osu_early_meh_window =
            static_cast<int64_t>(std::llround(127.0 * static_cast<double>(sample_rate_) / 1000.0));
        if (input_sample < lane.hold->end_sample - osu_early_meh_window) {
            lane.hold->osu_released_during_body = true;
        }
        lane.hold->release_active = true;
        lane.hold->release_sample = input_sample;
    }
}

JudgeWindowSamples GameplayEngine::build_windows(const config::JudgeConfig& judge, double rate) const {
    static_cast<void>(rate);
    auto to_samples = [&](double ms) -> int64_t {
        // Chart note samples are already rate-adjusted during chart conversion, and input timestamps map to the
        // live playback head. Keep judge windows in real playback milliseconds so rate changes do not tighten or
        // loosen timing a second time here.
        double scaled = ms * static_cast<double>(sample_rate_) / 1000.0;
        return static_cast<int64_t>(std::llround(scaled));
    };

    JudgeWindowSamples windows;
    windows.pg = to_samples(judge.pg_ms);
    windows.gr = to_samples(judge.gr_ms);
    windows.gd = to_samples(judge.gd_ms);
    windows.bd = to_samples(judge.bd_ms);
    windows.indirect_miss = to_samples(std::max(judge.indirect_miss_ms, judge.bd_ms));
    windows.pr_early = to_samples(1000.0);
    windows.indirect_miss_enabled = judge.indirect_miss_enabled;
    windows.hold_grace = to_samples(judge.hold_grace_ms);
    windows.hold_break = to_samples(judge.hold_break_ms);
    windows.mask = to_samples(judge.mask_ms);
    return windows;
}

bool GameplayEngine::should_apply_early_empty_poor(const LaneState& lane, int64_t input_sample) const {
    if (lane.next_index >= lane.notes.size()) {
        return false;
    }
    const auto& note = lane.notes[lane.next_index];
    const int64_t early_distance = note.start_sample - input_sample;
    return early_distance > windows_.bd && early_distance <= windows_.pr_early;
}

game::Judgement GameplayEngine::classify_judgement(int64_t delta_samples) const {
    int64_t abs_delta = std::abs(delta_samples);
    if (abs_delta <= windows_.pg) {
        return game::Judgement::PG;
    }
    if (abs_delta <= windows_.gr) {
        return game::Judgement::GR;
    }
    if (abs_delta <= windows_.gd) {
        return game::Judgement::GD;
    }
    if (abs_delta <= windows_.bd) {
        return game::Judgement::BD;
    }
    return game::Judgement::BD;
}

double GameplayEngine::samples_to_ms(int64_t samples) const {
    if (sample_rate_ <= 0) {
        return 0.0;
    }
    return static_cast<double>(samples) * 1000.0 / static_cast<double>(sample_rate_);
}

}  // namespace tenriff::gameplay
