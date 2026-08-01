#include "app/ModeResolver.h"

#include <string>

namespace tenriff::app {

ModeResolveResult resolve_mode_settings(const config::ModeConfig& config) {
    ModeResolveResult result;

    if (auto parsed = gameplay::parse_key_mode(config.key_mode)) {
        result.settings.key_mode = parsed.value();
    } else if (!config.key_mode.empty()) {
        result.warnings.push_back("mode.key_mode not recognized; using AUTO.");
    }

    if (auto parsed = gameplay::parse_key_mode_conversion_algorithm(config.key_conversion_algorithm)) {
        result.settings.key_conversion_algorithm = parsed.value();
    } else if (!config.key_conversion_algorithm.empty()) {
        result.warnings.push_back(
            "mode.key_conversion_algorithm not recognized; using KRRCREAM.");
    }

    if (auto parsed = gameplay::parse_gauge_mode(config.gauge)) {
        result.settings.gauge = parsed.value();
    } else if (!config.gauge.empty()) {
        result.warnings.push_back("mode.gauge not recognized; using NORMAL.");
    }

    if (auto parsed = gameplay::parse_random_mode(config.random)) {
        result.settings.random = parsed.value();
    } else if (!config.random.empty()) {
        result.warnings.push_back("mode.random not recognized; using OFF.");
    }

    result.settings.random_seed = config.random_seed;
    return result;
}

}  // namespace tenriff::app
