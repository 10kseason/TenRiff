#pragma once

#include <string>
#include <vector>

#include "gameplay/GameplayChart.h"
#include "gameplay/ModeSettings.h"

namespace tenriff::gameplay {

struct ModeApplyResult {
    GameplayChart chart;
    std::vector<std::string> warnings;
};

ModeApplyResult apply_mode_settings(const GameplayChart& chart, const ModeSettings& settings);

}  // namespace tenriff::gameplay
