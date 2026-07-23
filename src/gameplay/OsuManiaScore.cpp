#include "gameplay/OsuManiaScore.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

namespace {

constexpr double kPerfectWindowMs = 16.0;
constexpr double kGreatWindowMs = 40.0;  // 64 - 3 * OD8
constexpr double kGoodWindowMs = 73.0;   // 97 - 3 * OD8
constexpr double kOkWindowMs = 103.0;    // 127 - 3 * OD8
constexpr double kMehWindowMs = 127.0;   // 151 - 3 * OD8
constexpr double kMaxScore = 1'000'000.0;
constexpr double kMaxHitValue = 320.0;

struct ScoreValues {
    double hit_value = 0.0;
    double hit_bonus_value = 0.0;
    double hit_bonus = 0.0;
    double hit_punishment = 0.0;
    bool resets_bonus = false;
};

ScoreValues score_values(OsuManiaJudgement judgement) {
    switch (judgement) {
        case OsuManiaJudgement::Perfect: return {320.0, 32.0, 2.0, 0.0, false};
        case OsuManiaJudgement::Great: return {300.0, 32.0, 1.0, 0.0, false};
        case OsuManiaJudgement::Good: return {200.0, 16.0, 0.0, 8.0, false};
        case OsuManiaJudgement::Ok: return {100.0, 8.0, 0.0, 24.0, false};
        case OsuManiaJudgement::Meh: return {50.0, 4.0, 0.0, 44.0, false};
        case OsuManiaJudgement::Miss:
        default: return {0.0, 0.0, 0.0, 0.0, true};
    }
}

void increment_count(OsuManiaJudgementCounts& counts, OsuManiaJudgement judgement) {
    switch (judgement) {
        case OsuManiaJudgement::Perfect: ++counts.perfect; break;
        case OsuManiaJudgement::Great: ++counts.great; break;
        case OsuManiaJudgement::Good: ++counts.good; break;
        case OsuManiaJudgement::Ok: ++counts.ok; break;
        case OsuManiaJudgement::Meh: ++counts.meh; break;
        case OsuManiaJudgement::Miss: ++counts.miss; break;
    }
}

}  // namespace

OsuManiaJudgement classify_osu_mania_od8_tap(double delta_ms) {
    if (!std::isfinite(delta_ms)) {
        return OsuManiaJudgement::Miss;
    }

    // Stable mania permits the early 50 window, while a hit later than the
    // late 100 boundary is already a miss. osu! rounds hit error before
    // comparing it with the integer OD window.
    const double rounded_delta_ms = std::round(delta_ms);
    if (rounded_delta_ms > kOkWindowMs || rounded_delta_ms < -kMehWindowMs) {
        return OsuManiaJudgement::Miss;
    }

    const double absolute_delta = std::abs(rounded_delta_ms);
    if (absolute_delta <= kPerfectWindowMs) return OsuManiaJudgement::Perfect;
    if (absolute_delta <= kGreatWindowMs) return OsuManiaJudgement::Great;
    if (absolute_delta <= kGoodWindowMs) return OsuManiaJudgement::Good;
    if (absolute_delta <= kOkWindowMs) return OsuManiaJudgement::Ok;
    return OsuManiaJudgement::Meh;
}

OsuManiaJudgement classify_osu_mania_od8_hold(double head_delta_ms,
                                               double tail_delta_ms,
                                               bool released_during_body,
                                               bool forced_miss) {
    if (forced_miss || !std::isfinite(head_delta_ms) || !std::isfinite(tail_delta_ms) ||
        classify_osu_mania_od8_tap(head_delta_ms) == OsuManiaJudgement::Miss ||
        std::round(tail_delta_ms) > kOkWindowMs || std::round(tail_delta_ms) < -kMehWindowMs) {
        return OsuManiaJudgement::Miss;
    }

    const double head_error = std::abs(std::round(head_delta_ms));
    const double combined_error = head_error + std::abs(std::round(tail_delta_ms));
    OsuManiaJudgement judgement = OsuManiaJudgement::Meh;
    if (head_error <= kPerfectWindowMs * 1.2 && combined_error <= kPerfectWindowMs * 2.4) {
        judgement = OsuManiaJudgement::Perfect;
    } else if (head_error <= kGreatWindowMs * 1.1 && combined_error <= kGreatWindowMs * 2.2) {
        judgement = OsuManiaJudgement::Great;
    } else if (head_error <= kGoodWindowMs && combined_error <= kGoodWindowMs * 2.0) {
        judgement = OsuManiaJudgement::Good;
    } else if (head_error <= kOkWindowMs && combined_error <= kOkWindowMs * 2.0) {
        judgement = OsuManiaJudgement::Ok;
    }

    // Releasing and re-pressing during the body cannot recover above a 50 in
    // stable mania, even when the final release itself is well timed.
    if (released_during_body && judgement != OsuManiaJudgement::Miss) {
        judgement = OsuManiaJudgement::Meh;
    }
    return judgement;
}

void initialize_osu_mania_od8_score(OsuManiaScoreV1& state, int total_objects) {
    state = {};
    state.total_objects = std::max(0, total_objects);
    state.available = state.total_objects > 0;
    state.bonus = 100.0;
}

void record_osu_mania_od8_judgement(OsuManiaScoreV1& state, OsuManiaJudgement judgement) {
    if (!state.available || state.total_objects <= 0 || state.judged_objects >= state.total_objects) {
        return;
    }

    const ScoreValues values = score_values(judgement);
    if (values.resets_bonus) {
        state.bonus = 0.0;
    } else {
        state.bonus = std::clamp(state.bonus + values.hit_bonus - values.hit_punishment, 0.0, 100.0);
    }

    const double score_unit = kMaxScore * 0.5 / static_cast<double>(state.total_objects);
    const double base_score = score_unit * (values.hit_value / kMaxHitValue);
    const double bonus_score =
        score_unit * (values.hit_bonus_value * std::sqrt(state.bonus) / kMaxHitValue);
    state.score_accumulator += base_score + bonus_score;
    state.score = std::clamp<int64_t>(static_cast<int64_t>(std::llround(state.score_accumulator)),
                                      0,
                                      static_cast<int64_t>(kMaxScore));
    ++state.judged_objects;
    increment_count(state.counts, judgement);
}

}  // namespace tenriff::gameplay
