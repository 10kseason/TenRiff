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

constexpr double kLegacyBadWindowMs = 200.0;
constexpr double kPreviousBadWindowMs = 95.0;
constexpr double kCurrentBadWindowMs = 210.0;
constexpr double kLegacyHoldGraceMs = 20.0;
constexpr double kCurrentHoldGraceMs = 45.0;
constexpr double kLegacyHoldBreakMs = 50.0;
constexpr double kCurrentHoldBreakMs = 120.0;
constexpr double kLegacyInputDebounceMs = 5.0;
constexpr double kCurrentInputDebounceMs = 8.0;
constexpr double kLegacyResultTailMs = 500.0;
constexpr double kCurrentResultTailMs = 3000.0;
constexpr int kLegacyGraphicsRefreshHz = 1050;
constexpr double kLegacyNoteHeightScale = 1.0;
constexpr double kCurrentNoteHeightScale = 1.8;
constexpr double kJudgeWindowToleranceMs = 0.001;
constexpr double kInputDebounceToleranceMs = 0.001;
constexpr double kResultTailToleranceMs = 0.001;
constexpr double kGaugeDeltaTolerance = 0.00001;
constexpr double kSkinScaleTolerance = 0.00001;

using tenriff::game::GaugeDeltaTable;

constexpr GaugeDeltaTable kLegacyHardGauge{0.13752, 0.09144, 0.02304, -1.94425, -3.88850};
constexpr GaugeDeltaTable kLegacyNormalGauge{0.23123, 0.15438, 0.03877, -1.54583, -3.11025};
constexpr GaugeDeltaTable kLegacyEasyGauge{0.30664, 0.20443, 0.05143, -1.16116, -2.32232};

constexpr GaugeDeltaTable kPreviousHardGauge{0.06120, 0.04069, 0.01025, -2.33310, -4.27735};
constexpr GaugeDeltaTable kPreviousNormalGauge{0.10290, 0.06870, 0.01725, -1.85500, -3.42128};
constexpr GaugeDeltaTable kPreviousEasyGauge{0.13645, 0.09097, 0.02289, -1.39339, -2.55455};

constexpr GaugeDeltaTable kPriorHardGauge{0.01576, 0.01048, 0.00264, -3.84962, -7.05763};
constexpr GaugeDeltaTable kPriorNormalGauge{0.02650, 0.01769, 0.00444, -3.06075, -5.64511};
constexpr GaugeDeltaTable kPriorEasyGauge{0.03514, 0.02342, 0.00589, -2.29909, -4.21501};

constexpr GaugeDeltaTable kFormerCurrentHardGauge{0.01576, 0.01048, 0.00264, -8.84962, -7.05763};
constexpr GaugeDeltaTable kFormerCurrentNormalGauge{0.02650, 0.01769, 0.00444, -5.56075, -5.64511};
constexpr GaugeDeltaTable kFormerCurrentEasyGauge{0.03514, 0.02342, 0.00589, -4.04909, -4.21501};

constexpr GaugeDeltaTable kInterimHardGauge{0.01576, 0.01048, 0.00264, -5.50000, -7.50000};
constexpr GaugeDeltaTable kInterimNormalGauge{0.02650, 0.01769, 0.00444, -5.50000, -7.50000};
constexpr GaugeDeltaTable kInterimEasyGauge{0.03514, 0.02342, 0.00589, -5.50000, -7.50000};

constexpr GaugeDeltaTable kFormerPenaltyNormalGauge{0.05238095, 0.03492063, 0.00873016, -5.50000, -7.50000};
constexpr GaugeDeltaTable kFormerPenaltyEasyGauge{0.10000000, 0.06666667, 0.01666667, -5.50000, -7.50000};

constexpr GaugeDeltaTable kCurrentHardGauge{0.03666667, 0.02444444, 0.00611111, -5.50000, -7.50000};
constexpr GaugeDeltaTable kCurrentNormalGauge{0.05238095, 0.03492063, 0.00873016, -2.75000, -3.75000};
constexpr GaugeDeltaTable kCurrentEasyGauge{0.10000000, 0.06666667, 0.01666667, -2.06250, -2.81250};

bool migrate_gauge_delta(double& value, double legacy_value, double current_value) {
    if (std::abs(value - current_value) <= kGaugeDeltaTolerance) {
        return false;
    }
    if (std::abs(value - legacy_value) > kGaugeDeltaTolerance) {
        return false;
    }
    value = current_value;
    return true;
}

bool migrate_gauge_table(GaugeDeltaTable& value, const GaugeDeltaTable& legacy, const GaugeDeltaTable& current) {
    bool changed = false;
    changed = migrate_gauge_delta(value.pg, legacy.pg, current.pg) || changed;
    changed = migrate_gauge_delta(value.gr, legacy.gr, current.gr) || changed;
    changed = migrate_gauge_delta(value.gd, legacy.gd, current.gd) || changed;
    changed = migrate_gauge_delta(value.bd, legacy.bd, current.bd) || changed;
    changed = migrate_gauge_delta(value.pr, legacy.pr, current.pr) || changed;
    return changed;
}

bool matches_legacy_default_graphics(const config::RuntimeConfig& config) {
    return to_lower_copy(config.graphics.display_mode) == "borderless" &&
           to_lower_copy(config.graphics.resolution) == "native" &&
           config.graphics.vsync &&
           config.graphics.refresh_hz == kLegacyGraphicsRefreshHz &&
           !config.graphics.performance_overlay;
}

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
    if (std::abs(config.judge.bd_ms - kLegacyBadWindowMs) <= kJudgeWindowToleranceMs ||
        std::abs(config.judge.bd_ms - kPreviousBadWindowMs) <= kJudgeWindowToleranceMs) {
        config.judge.bd_ms = kCurrentBadWindowMs;
        changed = true;
    }
    if (std::abs(config.judge.hold_grace_ms - kLegacyHoldGraceMs) <= kJudgeWindowToleranceMs) {
        config.judge.hold_grace_ms = kCurrentHoldGraceMs;
        changed = true;
    }
    if (std::abs(config.judge.hold_break_ms - kLegacyHoldBreakMs) <= kJudgeWindowToleranceMs) {
        config.judge.hold_break_ms = kCurrentHoldBreakMs;
        changed = true;
    }
    if (std::abs(config.input.debounce_ms - kLegacyInputDebounceMs) <= kInputDebounceToleranceMs) {
        config.input.debounce_ms = kCurrentInputDebounceMs;
        changed = true;
    }
    if (std::abs(config.ui.result_tail_ms - kLegacyResultTailMs) <= kResultTailToleranceMs) {
        config.ui.result_tail_ms = kCurrentResultTailMs;
        changed = true;
    }
    if (std::abs(config.skin.note_height_scale - kLegacyNoteHeightScale) <= kSkinScaleTolerance) {
        config.skin.note_height_scale = kCurrentNoteHeightScale;
        changed = true;
    }
    for (auto& [mode, value] : config.skin.note_height_scales) {
        static_cast<void>(mode);
        if (std::abs(value - kLegacyNoteHeightScale) <= kSkinScaleTolerance) {
            value = kCurrentNoteHeightScale;
            changed = true;
        }
    }
    if (matches_legacy_default_graphics(config)) {
        config.graphics.vsync = false;
        changed = true;
    }
    changed = migrate_gauge_table(config.gauge.hard, kLegacyHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kLegacyNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kLegacyEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPriorHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPriorNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kPriorEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kFormerCurrentHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kFormerCurrentNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kFormerCurrentEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kInterimHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kInterimNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kInterimEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kFormerPenaltyNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kFormerPenaltyEasyGauge, kCurrentEasyGauge) || changed;
    if (std::abs(config.judge.indirect_miss_ms - config.judge.bd_ms) > kJudgeWindowToleranceMs) {
        config.judge.indirect_miss_ms = config.judge.bd_ms;
        changed = true;
    }

    return changed;
}

}  // namespace tenriff::app
