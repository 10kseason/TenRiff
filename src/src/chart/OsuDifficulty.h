#pragma once

#include "chart/OsuManiaLoader.h"

namespace tenriff::chart {

struct OsuDifficultyMetrics {
    double circus_rating = 0.0;
    int revive_level = 0;
    double peak_nps = 0.0;
    double average_nps = 0.0;
    int note_count = 0;
};

[[nodiscard]] OsuDifficultyMetrics calculate_osu_10k_difficulty(const OsuManiaChart& chart);

}  // namespace tenriff::chart
