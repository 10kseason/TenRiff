#include "gameplay/ResultStats.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

namespace {

double judgement_score_credit(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return 1.0;
        case game::Judgement::GR: return 3.0 / 6.0;
        case game::Judgement::GD: return 1.0 / 6.0;
        case game::Judgement::BD: return 0.0;
        case game::Judgement::PR:
        default: return 0.0;
    }
}

int detail_score_credit(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return 5;
        case game::Judgement::GR: return 3;
        case game::Judgement::GD: return 1;
        case game::Judgement::BD:
        case game::Judgement::PR:
        default: return 0;
    }
}

double categorical_accuracy_credit(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return 1.0;
        case game::Judgement::GR: return 0.8;
        case game::Judgement::GD: return 0.5;
        case game::Judgement::BD: return 0.2;
        case game::Judgement::PR:
        default: return 0.0;
    }
}


}  // namespace

void ResultStats::record_judgement(game::Judgement judgement,
                                   double delta_ms,
                                   ComboImpact combo_impact,
                                   double weight,
                                   double detailed_accuracy_credit) {
    switch (judgement) {
        case game::Judgement::PG: ++counts.pg; break;
        case game::Judgement::GR: ++counts.gr; break;
        case game::Judgement::GD: ++counts.gd; break;
        case game::Judgement::BD: ++counts.bd; break;
        case game::Judgement::PR: ++counts.pr; break;
    }

    const double safe_weight = (std::isfinite(weight) && weight > 0.0) ? weight : 1.0;
    judgement_score_points += judgement_score_credit(judgement) * safe_weight;
    detail_score += detail_score_credit(judgement);
    accuracy_points += categorical_accuracy_credit(judgement) * safe_weight;
    accuracy_weight += safe_weight;

    const double resolved_detail_credit =
        std::isfinite(detailed_accuracy_credit) && detailed_accuracy_credit >= 0.0
            ? std::clamp(detailed_accuracy_credit, 0.0, 1.0)
            : categorical_accuracy_credit(judgement);
    detailed_accuracy_points += resolved_detail_credit * safe_weight;
    detailed_accuracy_weight += safe_weight;

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
    raw_score = static_cast<int64_t>(std::llround(judgement_ratio * static_cast<double>(kNativeScoreMaximum)));
    raw_score = std::clamp<int64_t>(raw_score, 0, kNativeScoreMaximum);
    raw_score_accumulator = raw_score;

    if (std::isfinite(delta_ms)) {
        ++delta_samples;
        // FAST/SLOW is meant to explain judgements outside the top ±20 ms PG
        // window. Counting PG timing here makes an otherwise perfect play look
        // biased, so only GR and lower judgements contribute to the aggregate.
        if (judgement != game::Judgement::PG) {
            if (delta_ms > 0.05) {
                ++positive_delta_count;
            } else if (delta_ms < -0.05) {
                ++negative_delta_count;
            }
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
    detail_score = 0;
    accuracy_points = 0.0;
    accuracy_weight = 0.0;
    detailed_accuracy_points = 0.0;
    detailed_accuracy_weight = 0.0;
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
    if (accuracy_weight > 0.0 && std::isfinite(accuracy_points)) {
        return std::clamp(accuracy_points / accuracy_weight * 100.0, 0.0, 100.0);
    }

    const int judged = counts.pg + counts.gr + counts.gd + counts.bd + counts.pr;
    if (judged <= 0) {
        return 0.0;
    }
    const double points = static_cast<double>(counts.pg) +
                          static_cast<double>(counts.gr) * 0.80 +
                          static_cast<double>(counts.gd) * 0.50 +
                          static_cast<double>(counts.bd) * 0.20;
    return std::clamp(points / static_cast<double>(judged) * 100.0, 0.0, 100.0);
}

double ResultStats::detailed_accuracy_percent() const {
    if (detailed_accuracy_weight <= 0.0 || !std::isfinite(detailed_accuracy_points)) {
        return 0.0;
    }
    return std::clamp(detailed_accuracy_points / detailed_accuracy_weight * 100.0, 0.0, 100.0);
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

int64_t native_score_from_counts(const JudgementCounts& counts, int total_notes) {
    const int judged = counts.pg + counts.gr + counts.gd + counts.bd + counts.pr;
    const int denominator = total_notes > 0 ? total_notes : judged;
    if (denominator <= 0) {
        return 0;
    }
    const int64_t points = static_cast<int64_t>(counts.pg) * 6 +
                           static_cast<int64_t>(counts.gr) * 3 +
                           static_cast<int64_t>(counts.gd);
    const double ratio = std::clamp(
        static_cast<double>(points) / (static_cast<double>(denominator) * 6.0), 0.0, 1.0);
    return static_cast<int64_t>(std::llround(ratio * static_cast<double>(kNativeScoreMaximum)));
}

int64_t normalize_stored_native_score(int64_t score, int score_version) {
    if (score_version >= kNativeScoreVersion) {
        return std::clamp<int64_t>(score, 0, kNativeScoreMaximum);
    }
    constexpr int64_t kLegacyNativeScoreMaximum = 100'000;
    const int64_t legacy_score = std::clamp<int64_t>(score, 0, kLegacyNativeScoreMaximum);
    return static_cast<int64_t>(std::llround(
        static_cast<double>(legacy_score) * static_cast<double>(kNativeScoreMaximum) /
        static_cast<double>(kLegacyNativeScoreMaximum)));
}

}  // namespace tenriff::gameplay
