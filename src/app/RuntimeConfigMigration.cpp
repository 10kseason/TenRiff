#include "app/RuntimeConfigMigration.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <string_view>

namespace tenriff::app {

namespace {

std::string to_lower_copy(std::string_view value) {
    std::string token(value);
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return token;
}

bool is_valid_bms_keysound_policy(std::string_view value) {
    const std::string token = to_lower_copy(value);
    return token == "follow" || token == "autoplay" || token == "ignore" || token == "off";
}

bool is_valid_chart_filter(std::string_view value) {
    const std::string token = to_lower_copy(value);
    return token.empty() || token == "auto" || token == "bms" || token == "osu";
}

bool is_valid_osu_key_mode(std::string_view value) {
    const std::string token = to_lower_copy(value);
    return token.empty() || token == "auto" || token == "4k" || token == "5k" || token == "6k" ||
           token == "7k" || token == "8k" || token == "9k" || token == "10k" || token == "16k";
}

constexpr double kLegacyBadWindowMs = 80.0;
constexpr double kCurrentBadWindowMs = 200.0;
constexpr double kJudgeWindowToleranceMs = 0.001;

}  // namespace

bool migrate_bms_first_runtime_config(config::RuntimeConfig& config) {
    bool changed = false;

    if (config.mode.enable_osu_charts) {
        if (!is_valid_chart_filter(config.mode.format)) {
            config.mode.format = "auto";
            changed = true;
        }
        if (!is_valid_osu_key_mode(config.mode.key_mode)) {
            config.mode.key_mode = "auto";
            changed = true;
        }
    } else {
        if (to_lower_copy(config.mode.format) != "bms") {
            config.mode.format = "bms";
            changed = true;
        }
        if (to_lower_copy(config.mode.key_mode) != "10k") {
            config.mode.key_mode = "10k";
            changed = true;
        }
    }
    if (!is_valid_bms_keysound_policy(config.audio_ui.bms_keysound_policy)) {
        config.audio_ui.bms_keysound_policy = "follow";
        changed = true;
    }
    if (std::abs(config.judge.bd_ms - kLegacyBadWindowMs) <= kJudgeWindowToleranceMs) {
        config.judge.bd_ms = kCurrentBadWindowMs;
        changed = true;
    }

    return changed;
}

}  // namespace tenriff::app
