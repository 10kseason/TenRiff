#pragma once

#include "config/Config.h"
#include "network/PeerProtocol.h"

namespace tenriff::app {

// Apply only session-scoped scoring rules. Player calibration and presentation
// settings remain local so the comparison is fair without breaking either setup.
inline bool apply_peer_battle_rules(config::RuntimeConfig& config,
                                    uint32_t room_rate_milli = network::kPeerRateDefaultMilli) {
    if (!network::peer_rate_milli_is_valid(room_rate_milli)) return false;
    const config::RuntimeConfig defaults{};
    config.judge = defaults.judge;
    config.gauge = defaults.gauge;
    config.speed.rate = static_cast<double>(room_rate_milli) / 1000.0;
    // Key-count conversion is a local accessibility/presentation choice. The
    // room still validates identical source chart bytes before Ready.
    config.mode.gauge = "shift";
    config.mode.random = "off";
    config.mode.random_seed = 0;
    config.mode.mods.clear();
    config.mode.ghost_battle_enabled = false;
    config.mode.autoplay_enabled = false;
    config.mode.practice_no_fail_enabled = false;
    config.mode.one_miss_fail_enabled = false;
    config.mode.pacemaker_mode = "off";
    return true;
}

}  // namespace tenriff::app
