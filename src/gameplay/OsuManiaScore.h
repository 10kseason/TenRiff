#pragma once

#include <cstdint>

namespace tenriff::gameplay {

inline constexpr double kOsuManiaReferenceOd = 8.0;

enum class OsuManiaJudgement {
    Perfect,
    Great,
    Good,
    Ok,
    Meh,
    Miss,
};

struct OsuManiaJudgementCounts {
    int perfect = 0;
    int great = 0;
    int good = 0;
    int ok = 0;
    int meh = 0;
    int miss = 0;
};

struct OsuManiaScoreV1 {
    bool available = false;
    int total_objects = 0;
    int judged_objects = 0;
    OsuManiaJudgementCounts counts;
    double bonus = 100.0;
    double score_accumulator = 0.0;
    int64_t score = 0;
};

// Reference conversion uses osu!mania's stable OD8 timing windows and the
// unmodified 1,000,000-point ScoreV1 formula. It is intentionally independent
// from TenRiff's native score and score multipliers.
[[nodiscard]] OsuManiaJudgement classify_osu_mania_od8_tap(double delta_ms);
[[nodiscard]] OsuManiaJudgement classify_osu_mania_od8_hold(double head_delta_ms,
                                                             double tail_delta_ms,
                                                             bool released_during_body,
                                                             bool forced_miss = false);

void initialize_osu_mania_od8_score(OsuManiaScoreV1& state, int total_objects);
void record_osu_mania_od8_judgement(OsuManiaScoreV1& state, OsuManiaJudgement judgement);

}  // namespace tenriff::gameplay
