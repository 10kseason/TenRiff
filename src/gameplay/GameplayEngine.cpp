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

}  // namespace

GameplayEngine::GameplayEngine(const GameplayChart& chart, const GameplayConfig& config)
    : lane_count_(chart.lane_count),
      sample_rate_(config.sample_rate),
      rate_(clamp_rate(config.rate)),
      duration_samples_(chart.duration_samples),
      windows_(build_windows(config.judge, config.rate)),
      gauge_manager_(config.gauge) {
    if (lane_count_ <= 0) {
        lane_count_ = 10;
    }
    lanes_.resize(static_cast<std::size_t>(lane_count_));

    for (const auto& note : chart.notes) {
        if (note.lane <= 0 || note.lane > lane_count_) {
            continue;
        }
        lanes_[static_cast<std::size_t>(note.lane - 1)].notes.push_back(note);
    }

    for (auto& lane : lanes_) {
        std::stable_sort(lane.notes.begin(), lane.notes.end(), [](const NoteEvent& lhs, const NoteEvent& rhs) {
            return lhs.start_sample < rhs.start_sample;
        });
    }

    stats_.record_note_total(static_cast<int>(chart.notes.size()));
    gauge_state_ = gauge_manager_.initialState(config.initial_gauge);

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
    update_lane_input_state(lane_state, state, input_sample);
    if (state == input::InputState::Pressed) {
        if (lane_state.mask_until > input_sample) {
            return std::nullopt;
        }
        auto hit_note = try_hit_note(lane_state, input_sample);
        if (!hit_note.has_value()) {
            apply_poor(input_sample);
            lane_state.mask_until = input_sample + windows_.mask;
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

    for (auto& lane : lanes_) {
        update_miss(lane, current_sample);
        update_hold(lane, current_sample);
    }

    finalize_if_done(current_sample);
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

void GameplayEngine::apply_judgement(game::Judgement judgement, double delta_ms, int64_t sample,
                                     double weight, bool breaks_combo) {
    live_feedback_.has_value = true;
    live_feedback_.judgement = judgement;
    live_feedback_.delta_ms = std::isfinite(delta_ms) ? delta_ms : 0.0;
    live_feedback_.sample = sample;

    double time_ms = samples_to_ms(sample);
    auto previous_type = gauge_state_.type;
    auto result = gauge_manager_.applyJudgementWeighted(gauge_state_, judgement, time_ms, weight);

    stats_.record_judgement(judgement, delta_ms, breaks_combo);
    stats_.record_gauge_sample(sample, gauge_state_.value);

    if (result.downshifted) {
        stats_.record_shift(sample, previous_type, gauge_state_.type);
    }

    if (result.game_over) {
        game_over_ = true;
    }
}

std::optional<NoteEvent> GameplayEngine::try_hit_note(LaneState& lane, int64_t input_sample) {
    while (lane.next_index < lane.notes.size()) {
        const auto& note = lane.notes[lane.next_index];
        int64_t delta_samples = input_sample - note.start_sample;

        if (delta_samples > windows_.bd) {
            apply_poor(note.start_sample);
            lane.mask_until = note.start_sample + windows_.mask;
            ++lane.next_index;
            continue;
        }

        if (std::abs(delta_samples) > windows_.bd) {
            return std::nullopt;
        }

        auto judgement = classify_judgement(delta_samples);
        double delta_ms = static_cast<double>(delta_samples) * 1000.0 / static_cast<double>(sample_rate_);
        bool breaks_combo = (judgement == game::Judgement::BD || judgement == game::Judgement::PR);

        double weight = note.end_sample.has_value() ? 0.5 : 1.0;
        apply_judgement(judgement, delta_ms, input_sample, weight, breaks_combo);

        if (note.end_sample.has_value()) {
            HoldState hold;
            hold.end_sample = note.end_sample.value();
            lane.hold = hold;
        }

        NoteEvent hit_note = note;
        ++lane.next_index;
        return hit_note;
    }

    return std::nullopt;
}

void GameplayEngine::apply_poor(int64_t sample) {
    apply_judgement(game::Judgement::PR, std::numeric_limits<double>::quiet_NaN(), sample, 1.0, true);
}

void GameplayEngine::update_miss(LaneState& lane, int64_t current_sample) {
    while (lane.next_index < lane.notes.size()) {
        const auto& note = lane.notes[lane.next_index];
        if (current_sample <= note.start_sample + windows_.bd) {
            break;
        }
        apply_poor(note.start_sample);
        lane.mask_until = note.start_sample + windows_.mask;
        ++lane.next_index;
    }
}

void GameplayEngine::update_hold(LaneState& lane, int64_t current_sample) {
    if (!lane.hold.has_value()) {
        return;
    }

    auto& hold = *lane.hold;

    if (hold.release_active) {
        if (hold.release_sample < hold.end_sample) {
            apply_judgement(game::Judgement::BD, std::numeric_limits<double>::quiet_NaN(), hold.release_sample, 0.5,
                            true);
            lane.hold.reset();
            return;
        }

        int64_t delta_samples = hold.release_sample - hold.end_sample;
        auto judgement = classify_judgement(delta_samples);
        if (judgement == game::Judgement::PR) {
            judgement = game::Judgement::BD;
        }
        double delta_ms = static_cast<double>(delta_samples) * 1000.0 / static_cast<double>(sample_rate_);
        bool breaks_combo = (judgement == game::Judgement::BD);
        apply_judgement(judgement, delta_ms, hold.release_sample, 0.5, breaks_combo);
        lane.hold.reset();
        return;
    }

    if (current_sample >= hold.end_sample) {
        if (!hold.broken) {
            int64_t delta_samples = 0;
            auto judgement = classify_judgement(delta_samples);
            apply_judgement(judgement, 0.0, hold.end_sample, 0.5, false);
        }
        lane.hold.reset();
    }
}

void GameplayEngine::finalize_if_done(int64_t current_sample) {
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

    if (all_done && current_sample >= duration_samples_) {
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
        lane.hold->release_active = true;
        lane.hold->release_sample = input_sample;
    }
}

JudgeWindowSamples GameplayEngine::build_windows(const config::JudgeConfig& judge, double rate) const {
    double scale = 1.0;
    if (std::isfinite(rate) && rate > 0.0) {
        scale = 1.0 / rate;
    }
    auto to_samples = [&](double ms) -> int64_t {
        double scaled = ms * scale * static_cast<double>(sample_rate_) / 1000.0;
        return static_cast<int64_t>(std::llround(scaled));
    };

    JudgeWindowSamples windows;
    windows.pg = to_samples(judge.pg_ms);
    windows.gr = to_samples(judge.gr_ms);
    windows.gd = to_samples(judge.gd_ms);
    windows.bd = to_samples(judge.bd_ms);
    windows.hold_grace = to_samples(judge.hold_grace_ms);
    windows.hold_break = to_samples(judge.hold_break_ms);
    windows.mask = to_samples(judge.mask_ms);
    return windows;
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
    return game::Judgement::PR;
}

double GameplayEngine::samples_to_ms(int64_t samples) const {
    if (sample_rate_ <= 0) {
        return 0.0;
    }
    return static_cast<double>(samples) * 1000.0 / static_cast<double>(sample_rate_);
}

}  // namespace tenriff::gameplay
