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

bool is_valid_key_mode(std::string_view value) {
    const std::string token = to_lower_copy(value);
    return token.empty() || token == "auto" || token == "none" || token == "4k" || token == "5k" || token == "6k" ||
           token == "7k" || token == "8k" || token == "9k" || token == "10k" || token == "12k" ||
           token == "14k" || token == "16k";
}

constexpr double kLegacyBadWindowMs = 200.0;
constexpr double kPreviousBadWindowMs = 95.0;
constexpr double kPreviousCurrentBadWindowMs = 210.0;
constexpr double kLegacyGoodWindowMs = 55.0;
constexpr double kCurrentGoodWindowMs = 75.0;
constexpr double kCurrentBadWindowMs = 340.0;
constexpr double kLegacyHoldGraceMs = 20.0;
constexpr double kPreviousCurrentHoldGraceMs = 45.0;
constexpr double kCurrentHoldGraceMs = 80.0;
constexpr double kLegacyHoldBreakMs = 50.0;
constexpr double kPreviousCurrentHoldBreakMs = 120.0;
constexpr double kCurrentHoldBreakMs = 200.0;
constexpr double kLegacyInputDebounceMs = 5.0;
constexpr double kCurrentInputDebounceMs = 8.0;
constexpr double kLegacyResultTailMs = 500.0;
constexpr double kCurrentResultTailMs = 3000.0;
constexpr int kLegacyGraphicsRefreshHz = 1050;
constexpr int kCurrentDefaultGraphicsRefreshHz = 300;
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
constexpr GaugeDeltaTable kFormerPenaltyHardGauge{0.03666667, 0.02444444, 0.00611111, -5.50000, -7.50000};

constexpr GaugeDeltaTable kPreviousCurrentHardGauge{0.03666667, 0.02444444, 0.00611111, -5.50000, -5.50000};
constexpr GaugeDeltaTable kPreviousCurrentNormalGauge{0.05238095, 0.03492063, 0.00873016, -2.75000, -2.75000};
constexpr GaugeDeltaTable kPreviousCurrentEasyGauge{0.10000000, 0.06666667, 0.01666667, -2.06250, -2.06250};

constexpr GaugeDeltaTable kPreviousReleaseHardGauge{0.03666667, 0.02444444, 0.00000000, -7.04000, -7.04000};
constexpr GaugeDeltaTable kPreviousReleaseNormalGauge{0.05238095, 0.03492063, 0.00000000, -3.52000, -3.52000};
constexpr GaugeDeltaTable kPreviousReleaseEasyGauge{0.10000000, 0.06666667, 0.00000000, -2.64000, -2.64000};

constexpr GaugeDeltaTable kImmediatePreviousHardGauge{0.03666667, 0.02444444, 0.00000000, -15.48800, -15.48800};
constexpr GaugeDeltaTable kImmediatePreviousNormalGauge{0.05238095, 0.03492063, 0.00000000, -7.74400, -7.74400};
constexpr GaugeDeltaTable kImmediatePreviousEasyGauge{0.10000000, 0.06666667, 0.00000000, -5.80800, -5.80800};

constexpr GaugeDeltaTable kPreGreatRetuneHardGauge{0.03666667, 0.02444444, 0.00000000, -15.48800, -15.48800};
constexpr GaugeDeltaTable kPreGreatRetuneNormalGauge{0.05238095, 0.03492063, 0.00000000, -9.68000, -9.68000};
constexpr GaugeDeltaTable kPreGreatRetuneEasyGauge{0.10000000, 0.06666667, 0.00000000, -6.82440, -6.82440};

constexpr GaugeDeltaTable kPrePgRetuneHardGauge{0.03666667, 0.01333333, 0.00000000, -15.48800, -15.48800};
constexpr GaugeDeltaTable kPrePgRetuneNormalGauge{0.05238095, 0.02000000, 0.00000000, -9.68000, -9.68000};
constexpr GaugeDeltaTable kPrePgRetuneEasyGauge{0.10000000, 0.03333333, 0.00000000, -6.82440, -6.82440};

constexpr GaugeDeltaTable kFormerCurrentBaselineHardGauge{0.00500000, 0.01333333, 0.00000000, -15.48800, -15.48800};
constexpr GaugeDeltaTable kFormerCurrentBaselineNormalGauge{0.01000000, 0.02000000, 0.00000000, -9.68000, -9.68000};
constexpr GaugeDeltaTable kFormerCurrentBaselineEasyGauge{0.01500000, 0.03333333, 0.00000000, -6.82440, -6.82440};

constexpr GaugeDeltaTable kPreviousCurrentHardGaugeV2{0.00500000, 0.01333333, 0.00000000, -14.24896, -14.24896};
constexpr GaugeDeltaTable kPreviousCurrentNormalGaugeV2{0.01000000, 0.02000000, 0.00000000, -8.90560, -8.90560};
constexpr GaugeDeltaTable kPreviousCurrentEasyGaugeV2{0.01500000, 0.03333333, 0.00000000, -6.27845, -6.27845};

constexpr GaugeDeltaTable kPreviousCurrentHardGaugeV3{0.00500000, 0.01333333, 0.00000000, -8.00000, -8.00000};
constexpr GaugeDeltaTable kPreviousCurrentNormalGaugeV3{0.01000000, 0.02000000, 0.00000000, -6.00000, -6.00000};
constexpr GaugeDeltaTable kPreviousCurrentEasyGaugeV3{0.01500000, 0.03333333, 0.00000000, -4.00000, -4.00000};

constexpr GaugeDeltaTable kPreviousCurrentHardGaugeV4{0.00100000, 1.0 / 20.0, 1.0 / 65.0, -8.00000, -8.00000};
constexpr GaugeDeltaTable kPreviousCurrentNormalGaugeV4{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -6.00000, -6.00000};
constexpr GaugeDeltaTable kPreviousCurrentEasyGaugeV4{0.13500000, 1.0 / 20.0, 1.0 / 65.0, -4.00000, -4.00000};

constexpr GaugeDeltaTable kPreviousCurrentHardGaugeV5{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -8.00000, -8.00000};
constexpr GaugeDeltaTable kPreviousCurrentNormalGaugeV5{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -6.00000, -6.00000};
constexpr GaugeDeltaTable kPreviousCurrentEasyGaugeV5{0.13500000, 1.0 / 20.0, 1.0 / 65.0, -4.00000, -4.00000};

constexpr GaugeDeltaTable kPreviousCurrentHardGaugeV6{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -4.00000, -4.00000};
constexpr GaugeDeltaTable kPreviousCurrentNormalGaugeV6{0.01000000, 1.0 / 20.0, 1.0 / 65.0, -2.00000, -2.00000};
constexpr GaugeDeltaTable kPreviousCurrentEasyGaugeV6{0.03200000, 0.03200000 / 20.0, 0.03200000 / 50.0, -2.00000, -2.00000};

constexpr GaugeDeltaTable kPreviousCurrentHardGaugeV7{0.16000000, 0.09000000, 0.01000000, -10.00000, -10.00000};
constexpr GaugeDeltaTable kPreviousCurrentNormalGaugeV7{0.19000000, 0.15000000, 0.01000000, -6.00000, -6.00000};
constexpr GaugeDeltaTable kPreviousCurrentEasyGaugeV7{0.25000000, 0.20000000, 0.01000000, -4.00000, -4.00000};

constexpr GaugeDeltaTable kCurrentHardGauge{0.16000000, 0.09000000, 0.01000000, -10.00000, -2.00000};
constexpr GaugeDeltaTable kCurrentNormalGauge{0.19000000, 0.15000000, 0.01000000, -6.25000, -2.00000};
constexpr GaugeDeltaTable kCurrentEasyGauge{0.25000000, 0.20000000, 0.01000000, -4.10000, -1.60000};

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

bool matches_legacy_default_off_vsync_graphics(const config::RuntimeConfig& config) {
    return to_lower_copy(config.graphics.display_mode) == "borderless" &&
           to_lower_copy(config.graphics.resolution) == "native" &&
           !config.graphics.vsync &&
           config.graphics.refresh_hz == kLegacyGraphicsRefreshHz &&
           !config.graphics.performance_overlay;
}

}  // namespace

bool migrate_bms_first_runtime_config(config::RuntimeConfig& config) {
    bool changed = false;

    if (!is_valid_key_mode(config.mode.key_mode)) {
        config.mode.key_mode = "none";
        changed = true;
    }
    const std::string key_conversion_algorithm =
        to_lower_copy(config.mode.key_conversion_algorithm);
    if (key_conversion_algorithm == "nk2" || key_conversion_algorithm == "keyweaver" ||
        key_conversion_algorithm == "keyweaver_nk2" || key_conversion_algorithm == "nativek2") {
        if (config.mode.key_conversion_algorithm != "nk2") {
            config.mode.key_conversion_algorithm = "nk2";
            changed = true;
        }
    } else if (key_conversion_algorithm == "krr" || key_conversion_algorithm == "krrcream" ||
               key_conversion_algorithm == "legacy" || key_conversion_algorithm == "n2nc") {
        if (config.mode.key_conversion_algorithm != "krrcream") {
            config.mode.key_conversion_algorithm = "krrcream";
            changed = true;
        }
    } else {
        config.mode.key_conversion_algorithm = "krrcream";
        changed = true;
    }
    if (!is_valid_bms_keysound_policy(config.audio_ui.bms_keysound_policy)) {
        config.audio_ui.bms_keysound_policy = "follow";
        changed = true;
    }
    if (std::abs(config.judge.bd_ms - kLegacyBadWindowMs) <= kJudgeWindowToleranceMs ||
        std::abs(config.judge.bd_ms - kPreviousBadWindowMs) <= kJudgeWindowToleranceMs ||
        std::abs(config.judge.bd_ms - kPreviousCurrentBadWindowMs) <= kJudgeWindowToleranceMs) {
        config.judge.bd_ms = kCurrentBadWindowMs;
        changed = true;
    }
    if (std::abs(config.judge.gd_ms - kLegacyGoodWindowMs) <= kJudgeWindowToleranceMs) {
        config.judge.gd_ms = kCurrentGoodWindowMs;
        changed = true;
    }
    if (std::abs(config.judge.hold_grace_ms - kLegacyHoldGraceMs) <= kJudgeWindowToleranceMs ||
        std::abs(config.judge.hold_grace_ms - kPreviousCurrentHoldGraceMs) <= kJudgeWindowToleranceMs) {
        config.judge.hold_grace_ms = kCurrentHoldGraceMs;
        changed = true;
    }
    if (std::abs(config.judge.hold_break_ms - kLegacyHoldBreakMs) <= kJudgeWindowToleranceMs ||
        std::abs(config.judge.hold_break_ms - kPreviousCurrentHoldBreakMs) <= kJudgeWindowToleranceMs) {
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
    if (matches_legacy_default_off_vsync_graphics(config)) {
        config.graphics.refresh_hz = kCurrentDefaultGraphicsRefreshHz;
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
    changed = migrate_gauge_table(config.gauge.hard, kFormerPenaltyHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kFormerPenaltyNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kFormerPenaltyEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousReleaseHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousReleaseNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousReleaseEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kImmediatePreviousHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kImmediatePreviousNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kImmediatePreviousEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreGreatRetuneHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreGreatRetuneNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreGreatRetuneEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPrePgRetuneHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPrePgRetuneNormalGauge, kCurrentNormalGauge) || changed;
    changed = migrate_gauge_table(config.gauge.easy, kPrePgRetuneEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kFormerCurrentBaselineHardGauge, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kFormerCurrentBaselineNormalGauge, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kFormerCurrentBaselineEasyGauge, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGaugeV2, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGaugeV2, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGaugeV2, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGaugeV3, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGaugeV3, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGaugeV3, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGaugeV4, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGaugeV4, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGaugeV4, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGaugeV5, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGaugeV5, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGaugeV5, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGaugeV6, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGaugeV6, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGaugeV6, kCurrentEasyGauge) || changed;
    changed = migrate_gauge_table(config.gauge.hard, kPreviousCurrentHardGaugeV7, kCurrentHardGauge) || changed;
    changed = migrate_gauge_table(config.gauge.normal, kPreviousCurrentNormalGaugeV7, kCurrentNormalGauge) ||
              changed;
    changed = migrate_gauge_table(config.gauge.easy, kPreviousCurrentEasyGaugeV7, kCurrentEasyGauge) || changed;
    if (std::abs(config.judge.indirect_miss_ms - config.judge.bd_ms) > kJudgeWindowToleranceMs) {
        config.judge.indirect_miss_ms = config.judge.bd_ms;
        changed = true;
    }

    return changed;
}

}  // namespace tenriff::app
