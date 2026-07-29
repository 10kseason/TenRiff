#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tenriff::chart {

// The native BMS difficulty port uses a compact lane/timing chart independent
// of any external chart parser.
struct OsuManiaNote {
    int column = 0;
    int64_t start_time_ms = 0;
    std::optional<int64_t> end_time_ms;
    int hit_sound = 0;
};

struct OsuManiaChart {
    int key_count = 0;
    double base_bpm = 0.0;
    double overall_difficulty = 8.0;
    std::vector<OsuManiaNote> notes;
};

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
