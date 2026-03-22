#pragma once

#include <cstdint>
#include <vector>

#include "game/GaugeManager.h"

namespace tenriff::gameplay {

struct JudgementCounts {
    int pg = 0;
    int gr = 0;
    int gd = 0;
    int bd = 0;
    int pr = 0;
};

enum class ComboImpact {
    Increment,
    Break,
    Preserve,
};

struct GaugeSample {
    int64_t sample = 0;
    double value = 0.0;
};

struct ShiftEvent {
    int64_t sample = 0;
    game::GaugeType from = game::GaugeType::Normal;
    game::GaugeType to = game::GaugeType::Normal;
};

struct ResultStats {
    JudgementCounts counts;
    int combo = 0;
    int max_combo = 0;
    int total_notes = 0;
    int64_t raw_score = 0;
    int64_t raw_score_accumulator = 0;

    double mean_delta_ms = 0.0;
    double m2_delta_ms = 0.0;
    int delta_samples = 0;
    int positive_delta_count = 0;
    int negative_delta_count = 0;

    std::vector<GaugeSample> gauge_history;
    std::vector<ShiftEvent> shifts;

    void record_judgement(game::Judgement judgement, double delta_ms, ComboImpact combo_impact, double weight = 1.0);
    void record_note_total(int count);
    void record_gauge_sample(int64_t sample, double value);
    void record_shift(int64_t sample, game::GaugeType from, game::GaugeType to);

    [[nodiscard]] double stddev_delta_ms() const;
};

}  // namespace tenriff::gameplay
