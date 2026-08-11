#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gameplay/GameplayChart.h"

namespace tenriff::app {

enum class ChartFormat {
    Unknown,
    Bms,
};

struct ChartLoadResult {
    gameplay::GameplayChart chart;
    ChartFormat format = ChartFormat::Unknown;
    double base_bpm = 0.0;
    std::vector<std::string> messages;
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

class ChartLoader {
public:
    [[nodiscard]] ChartLoadResult load(const std::string& path,
                                       int sample_rate,
                                       double rate,
                                       std::string_view bms_keysound_policy = "ignore",
                                       uint32_t bms_random_seed = 0) const;
};

}  // namespace tenriff::app
