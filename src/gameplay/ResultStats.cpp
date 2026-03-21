#include "gameplay/ResultStats.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

namespace {

int judgement_score(game::Judgement judgement) {
    switch (judgement) {
        case game::Judgement::PG: return 1000;
        case game::Judgement::GR: return 700;
        case game::Judgement::GD: return 300;
        case game::Judgement::BD: return -200;
        case game::Judgement::PR:
        default: return 0;
    }
}

}  // namespace

void ResultStats::record_judgement(game::Judgement judgement, double delta_ms, ComboImpact combo_impact, double weight) {
    switch (judgement) {
        case game::Judgement::PG: ++counts.pg; break;
        case game::Judgement::GR: ++counts.gr; break;
        case game::Judgement::GD: ++counts.gd; break;
        case game::Judgement::BD: ++counts.bd; break;
        case game::Judgement::PR: ++counts.pr; break;
    }

    const double safe_weight = (std::isfinite(weight) && weight > 0.0) ? weight : 1.0;
    const int64_t delta_score = static_cast<int64_t>(std::llround(static_cast<double>(judgement_score(judgement)) *
                                                                  safe_weight));
    raw_score_accumulator += delta_score;
    raw_score = std::max<int64_t>(0, raw_score_accumulator);

    switch (combo_impact) {
    case ComboImpact::Break:
        combo = 0;
        break;
    case ComboImpact::Increment:
        ++combo;
        if (combo > max_combo) {
            max_combo = combo;
        }
        break;
    case ComboImpact::Preserve:
        break;
    }

    if (std::isfinite(delta_ms)) {
        ++delta_samples;
        double delta = delta_ms - mean_delta_ms;
        mean_delta_ms += delta / static_cast<double>(delta_samples);
        double delta2 = delta_ms - mean_delta_ms;
        m2_delta_ms += delta * delta2;
    }
}

void ResultStats::record_note_total(int count) {
    total_notes = count;
    raw_score = 0;
    raw_score_accumulator = 0;
    gauge_history.clear();
    shifts.clear();
    const std::size_t reserve_count = static_cast<std::size_t>(std::max(0, count)) * 2u;
    gauge_history.reserve((std::max)(reserve_count, static_cast<std::size_t>(32)));
    shifts.reserve(4);
}

void ResultStats::record_gauge_sample(int64_t sample, double value) {
    gauge_history.push_back(GaugeSample{sample, value});
}

void ResultStats::record_shift(int64_t sample, game::GaugeType from, game::GaugeType to) {
    shifts.push_back(ShiftEvent{sample, from, to});
}

double ResultStats::stddev_delta_ms() const {
    if (delta_samples <= 1) {
        return 0.0;
    }
    return std::sqrt(m2_delta_ms / static_cast<double>(delta_samples - 1));
}

}  // namespace tenriff::gameplay
