#include "gameplay/ResultStats.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

namespace {

double judgement_score_credit(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return 1.0;
        case game::Judgement::GR: return 0.7;
        case game::Judgement::GD: return 0.3;
        case game::Judgement::BD: return 0.0;
        case game::Judgement::PR:
        default: return 0.0;
    }
}

double legacy_accuracy_credit(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return 1.0;
        case game::Judgement::GR: return 0.8;
        case game::Judgement::GD: return 0.5;
        case game::Judgement::BD: return 0.2;
        case game::Judgement::PR:
        default: return 0.0;
    }
}

int64_t maximum_combo_units(int total_combo_steps) {
    if (total_combo_steps <= 0) {
        return 0;
    }
    const int64_t steps = static_cast<int64_t>(total_combo_steps);
    return steps * (steps + 1) / 2;
}

}  // namespace

void ResultStats::record_judgement(game::Judgement judgement,
                                   double delta_ms,
                                   ComboImpact combo_impact,
                                   double weight,
                                   double accuracy_credit) {
    switch (judgement) {
        case game::Judgement::PG: ++counts.pg; break;
        case game::Judgement::GR: ++counts.gr; break;
        case game::Judgement::GD: ++counts.gd; break;
        case game::Judgement::BD: ++counts.bd; break;
        case game::Judgement::PR: ++counts.pr; break;
    }

    const double safe_weight = (std::isfinite(weight) && weight > 0.0) ? weight : 1.0;
    if (judgement != game::Judgement::PR) {
        judgement_score_points += judgement_score_credit(judgement) * safe_weight;
        const double resolved_accuracy_credit =
            std::isfinite(accuracy_credit) && accuracy_credit >= 0.0
                ? std::clamp(accuracy_credit, 0.0, 1.0)
                : legacy_accuracy_credit(judgement);
        accuracy_points += resolved_accuracy_credit * safe_weight;
        accuracy_weight += safe_weight;

        if (judgement == game::Judgement::PG && std::isfinite(delta_ms)) {
            if (highest_judgement_timing_weight <= 0.0) {
                highest_judgement_min_delta_ms = delta_ms;
                highest_judgement_max_delta_ms = delta_ms;
            } else {
                highest_judgement_min_delta_ms = std::min(highest_judgement_min_delta_ms, delta_ms);
                highest_judgement_max_delta_ms = std::max(highest_judgement_max_delta_ms, delta_ms);
            }
            highest_judgement_timing_weight += safe_weight;
        }
    }

    switch (combo_impact) {
    case ComboImpact::Break:
        combo = 0;
        break;
    case ComboImpact::Increment:
        ++combo;
        combo_score_units += combo;
        if (combo > max_combo) {
            max_combo = combo;
        }
        break;
    case ComboImpact::Preserve:
        break;
    }

    const double judgement_ratio = total_notes > 0
                                       ? std::clamp(judgement_score_points / static_cast<double>(total_notes), 0.0, 1.0)
                                       : 0.0;
    const int64_t maximum_units = maximum_combo_units(total_combo_steps);
    const double combo_ratio = maximum_units > 0
                                   ? std::clamp(static_cast<double>(combo_score_units) /
                                                    static_cast<double>(maximum_units),
                                                0.0,
                                                1.0)
                                   : 0.0;
    raw_score = static_cast<int64_t>(std::llround(judgement_ratio * 90'000.0 + combo_ratio * 10'000.0));
    raw_score = std::clamp<int64_t>(raw_score, 0, kNativeScoreMaximum);
    raw_score_accumulator = raw_score;

    if (std::isfinite(delta_ms)) {
        ++delta_samples;
        if (delta_ms > 0.05) {
            ++positive_delta_count;
        } else if (delta_ms < -0.05) {
            ++negative_delta_count;
        }
        double delta = delta_ms - mean_delta_ms;
        mean_delta_ms += delta / static_cast<double>(delta_samples);
        double delta2 = delta_ms - mean_delta_ms;
        m2_delta_ms += delta * delta2;
    }
}

void ResultStats::record_note_total(int count, int combo_steps) {
    total_notes = std::max(0, count);
    total_combo_steps = std::max(0, combo_steps > 0 ? combo_steps : count);
    raw_score = 0;
    raw_score_accumulator = 0;
    judgement_score_points = 0.0;
    combo_score_units = 0;
    accuracy_points = 0.0;
    accuracy_weight = 0.0;
    highest_judgement_timing_weight = 0.0;
    highest_judgement_min_delta_ms = 0.0;
    highest_judgement_max_delta_ms = 0.0;
    initialize_osu_mania_od8_score(osu_od8, total_notes);
    gauge_history.clear();
    shifts.clear();
    const std::size_t reserve_count = static_cast<std::size_t>(total_notes) * 2u;
    gauge_history.reserve((std::max)(reserve_count, static_cast<std::size_t>(32)));
    shifts.reserve(4);
}

void ResultStats::record_gauge_sample(int64_t sample, double value) {
    gauge_history.push_back(GaugeSample{sample, value});
}

void ResultStats::record_shift(int64_t sample, game::GaugeType from, game::GaugeType to) {
    shifts.push_back(ShiftEvent{sample, from, to});
}

double ResultStats::accuracy_percent() const {
    double accuracy = 0.0;
    if (accuracy_weight > 0.0 && std::isfinite(accuracy_points)) {
        accuracy = accuracy_points / accuracy_weight * 100.0;
    } else {
        const int judged = counts.pg + counts.gr + counts.gd + counts.bd;
        if (judged <= 0) {
            return 0.0;
        }
        accuracy = (static_cast<double>(counts.pg) +
                    static_cast<double>(counts.gr) * 0.80 +
                    static_cast<double>(counts.gd) * 0.50 +
                    static_cast<double>(counts.bd) * 0.20) /
                   static_cast<double>(judged) * 100.0;
    }

    // A loose top-judgement cluster should not read as near-perfect even when
    // every categorical judgement is PG. Eight milliseconds rewards real
    // consistency without requiring a zero-centered input offset.
    constexpr double kHighestJudgementDenseRangeMs = 8.0;
    constexpr double kLooseHighestJudgementCap = 99.5;
    if (highest_judgement_timing_weight > 1.0 &&
        highest_judgement_max_delta_ms - highest_judgement_min_delta_ms > kHighestJudgementDenseRangeMs) {
        accuracy = std::min(accuracy, kLooseHighestJudgementCap);
    }
    return std::clamp(accuracy, 0.0, 100.0);
}

double ResultStats::stddev_delta_ms() const {
    if (delta_samples <= 1) {
        return 0.0;
    }
    return std::sqrt(m2_delta_ms / static_cast<double>(delta_samples - 1));
}

int64_t scale_native_score(int64_t raw_score, double multiplier) {
    const double safe_multiplier = (std::isfinite(multiplier) && multiplier > 0.0) ? multiplier : 1.0;
    return std::clamp<int64_t>(
        static_cast<int64_t>(std::llround(static_cast<double>(raw_score) * safe_multiplier)),
        0,
        kNativeScoreMaximum);
}

}  // namespace tenriff::gameplay
