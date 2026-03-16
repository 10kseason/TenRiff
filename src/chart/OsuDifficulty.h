#pragma once

#include <string>

#include "chart/OsuManiaLoader.h"

namespace tenriff::chart {

enum class DifficultyPreset {
    OsuInterpolated,
    QwilightBmsEz,
};

struct ManiaDifficultyOptions {
    DifficultyPreset preset = DifficultyPreset::OsuInterpolated;
    std::string mode_name;
};

struct OsuDifficultyMetrics {
    double circus_rating = 0.0;
    int revive_level = 0;
    double peak_nps = 0.0;
    double average_nps = 0.0;
    int note_count = 0;
};

[[nodiscard]] OsuDifficultyMetrics calculate_osu_10k_difficulty(const OsuManiaChart& chart);
[[nodiscard]] OsuDifficultyMetrics calculate_osu_mania_difficulty(const OsuManiaChart& chart,
                                                                  const ManiaDifficultyOptions& options);
[[nodiscard]] OsuDifficultyMetrics calculate_osu_mania_difficulty(const OsuManiaChart& chart);

}  // namespace tenriff::chart
