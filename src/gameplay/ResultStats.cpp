#include "gameplay/ResultStats.h"

#include <algorithm>
#include <cmath>

namespace tenriff::gameplay {

void ResultStats::record_judgement(game::Judgement judgement, double delta_ms, bool breaks_combo) {
    switch (judgement) {
        case game::Judgement::PG: ++counts.pg; break;
        case game::Judgement::GR: ++counts.gr; break;
        case game::Judgement::GD: ++counts.gd; break;
        case game::Judgement::BD: ++counts.bd; break;
        case game::Judgement::PR: ++counts.pr; break;
    }

    if (breaks_combo) {
        combo = 0;
    } else {
        ++combo;
        if (combo > max_combo) {
            max_combo = combo;
        }
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
