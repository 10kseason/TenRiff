#pragma once

#include "config/Config.h"

namespace tenriff::app {

// Apply only session-scoped scoring rules. Player calibration and presentation
// settings remain local so the comparison is fair without breaking either setup.
inline void apply_peer_battle_rules(config::RuntimeConfig& config) {
    const config::RuntimeConfig defaults{};
    config.judge = defaults.judge;
    config.gauge = defaults.gauge;
    config.speed.rate = 1.0;
    config.mode.key_mode = "none";
    config.mode.gauge = "normal";
    config.mode.random = "off";
    config.mode.random_seed = 0;
    config.mode.mods.clear();
    config.mode.ghost_battle_enabled = false;
    config.mode.autoplay_enabled = false;
    config.mode.practice_no_fail_enabled = false;
    config.mode.one_miss_fail_enabled = false;
}

}  // namespace tenriff::app
