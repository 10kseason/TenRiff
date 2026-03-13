#pragma once

#include <string>
#include <vector>

#include "config/Config.h"
#include "gameplay/ModeSettings.h"

namespace tenriff::app {

struct ModeResolveResult {
    gameplay::ModeSettings settings;
    std::vector<std::string> warnings;
};

ModeResolveResult resolve_mode_settings(const config::ModeConfig& config);

}  // namespace tenriff::app
