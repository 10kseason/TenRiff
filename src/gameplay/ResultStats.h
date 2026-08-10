#pragma once

#include <cstdint>
#include <vector>

#include "game/GaugeManager.h"
#include "gameplay/OsuManiaScore.h"

namespace tenriff::gameplay {

inline constexpr int kNativeScoreVersion = 2;
inline constexpr int64_t kNativeScoreMaximum = 10'000;

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
    int total_combo_steps = 0;
    int64_t raw_score = 0;
    int64_t raw_score_accumulator = 0;
    double judgement_score_points = 0.0;
    int64_t combo_score_units = 0;
    int64_t detail_score = 0;
    double accuracy_points = 0.0;
    double accuracy_weight = 0.0;
    double detailed_accuracy_points = 0.0;
    double detailed_accuracy_weight = 0.0;
    double highest_judgement_timing_weight = 0.0;
    double highest_judgement_min_delta_ms = 0.0;
    double highest_judgement_max_delta_ms = 0.0;
    OsuManiaScoreV1 osu_od8;

    double mean_delta_ms = 0.0;
    double m2_delta_ms = 0.0;
    int delta_samples = 0;
    int positive_delta_count = 0;
    int negative_delta_count = 0;

    std::vector<GaugeSample> gauge_history;
    std::vector<ShiftEvent> shifts;

    void record_judgement(game::Judgement judgement,
                          double delta_ms,
                          ComboImpact combo_impact,
                          double weight = 1.0,
                          double detailed_accuracy_credit = -1.0);
    void record_note_total(int count, int combo_steps = 0);
    void record_gauge_sample(int64_t sample, double value);
    void record_shift(int64_t sample, game::GaugeType from, game::GaugeType to);

    [[nodiscard]] double accuracy_percent() const;
    [[nodiscard]] double detailed_accuracy_percent() const;
    [[nodiscard]] double stddev_delta_ms() const;
};

[[nodiscard]] int64_t scale_native_score(int64_t raw_score, double multiplier);
[[nodiscard]] int64_t native_score_from_counts(const JudgementCounts& counts, int total_notes);
[[nodiscard]] int64_t normalize_stored_native_score(int64_t score, int score_version);
[[nodiscard]] constexpr int64_t maximum_detail_score(int total_notes) {
    return total_notes > 0 ? static_cast<int64_t>(total_notes) * 5 : 0;
}

}  // namespace tenriff::gameplay
