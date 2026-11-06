#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gameplay/GameplayChart.h"
#include "gameplay/ModeSettings.h"

namespace tenriff::gameplay {

struct ModeApplyContext {
    double base_bpm = 0.0;
    int sample_rate = 0;
    int key_conversion_version = 3;
};

struct ModeApplyResult {
    GameplayChart chart;
    std::vector<std::string> warnings;
    std::optional<Nk2Preset> replay_nk2_preset;
};

ModeApplyResult apply_mode_settings(const GameplayChart& chart,
                                    const ModeSettings& settings,
                                    const ModeApplyContext& context = {});

}  // namespace tenriff::gameplay
