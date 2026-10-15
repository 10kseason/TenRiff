#include "game/GaugeManager.h"

#include <algorithm>
#include <cmath>

namespace tenriff::game {

namespace {
constexpr double kGaugeMax = 100.0;
constexpr double kMinGauge = 0.0;
constexpr double kEasyLowGaugeBadSofteningThreshold = 25.0;
constexpr double kEasyLowGaugeBadSofteningScale = 0.90;
constexpr double kLr2HardPoorLowGaugeThreshold = 30.0;
constexpr double kLr2HardPoorLowGaugeScale = 0.60;
// Original LR2 compares the displayed two-percent cell: int(HP / 2) * 2 <= 30.
// This is HP < 32, not the < 30 approximation used by some compatible players.
constexpr double kLr2CourseLowGaugeThreshold = 32.0;
constexpr double kLr2CourseFailureThreshold = 2.0;

double max_gauge_for(GaugeType type) {
    static_cast<void>(type);
    return kGaugeMax;
}

bool is_easy_softened_bad_judgement(Judgement judgement) {
    return judgement == Judgement::BD;
}

double lr2_note_count_damage_scale(double notes) {
    // LR2oraja's MODIFY_DAMAGE uses both TOTAL and chart size. Use floating
    // divisions: integer division would erase the short-chart corrections.
    if (notes <= 20.0) return 10.0;
    if (notes < 30.0) return 8.0 + 0.2 * (30.0 - notes);
    if (notes < 60.0) return 5.0 + 0.2 * (60.0 - notes) / 3.0;
    if (notes < 125.0) return 4.0 + (125.0 - notes) / 65.0;
    if (notes < 250.0) return 3.0 + 0.008 * (250.0 - notes);
    if (notes < 500.0) return 2.0 + 0.004 * (500.0 - notes);
    if (notes < 1000.0) return 1.0 + 0.002 * (1000.0 - notes);
    return 1.0;
}
}

GaugeManager::GaugeManager(GaugeConfig config, GaugeRuntimePolicy policy)
    : config_(config), policy_(policy) {
    if (policy_.bms_lr2_deltas && !policy_.course_lr2_deltas) {
        const double notes = static_cast<double>(std::max(0, policy_.bms_note_count));
        const double total = std::isfinite(policy_.bms_total) && policy_.bms_total > 0.0
                                 ? policy_.bms_total
                                 : 160.0 + (notes + std::clamp(notes - 400.0, 0.0, 200.0)) * 0.16;
        // An empty chart cannot earn recovery and must never divide by zero.
        bms_recovery_per_note_ = notes > 0.0 ? total / notes : 0.0;
        const double total_damage_scale =
            10.0 / std::clamp(std::floor(total / 16.0) - 5.0, 1.0, 10.0);
        bms_hard_damage_scale_ = std::max(total_damage_scale, lr2_note_count_damage_scale(notes));
    }
}

GaugeState GaugeManager::initialState(GaugeType type) const noexcept {
    GaugeState state;
    state.type = type;
    state.value = max_gauge_for(type);
    state.game_over = false;
    return state;
}

GaugeDeltaTable GaugeManager::tableFor(GaugeType type) const noexcept {
    if (policy_.course_lr2_deltas) {
        // LR2 beta3 courseType 2; GOOD is 0.04, and a consumed POOR is -3.
        // Empty POOR is a separate -2 event, selected in applyJudgementWeighted.
        return GaugeDeltaTable{0.1, 0.1, 0.04, -2.0, -3.0};
    }
    if (policy_.bms_lr2_deltas) {
        // Delta reference: LR2oraja GaugeProperty LR2 + GrooveGauge TOTAL /
        // MODIFY_DAMAGE. This deliberately does not replace survival rules.
        switch (type) {
        case GaugeType::ExHard:
            return {0.1, 0.1, 0.05, -12.0 * bms_hard_damage_scale_, -20.0 * bms_hard_damage_scale_};
        case GaugeType::Hard:
            return {0.1, 0.1, 0.05, -6.0 * bms_hard_damage_scale_, -10.0 * bms_hard_damage_scale_};
        case GaugeType::Normal:
            return {bms_recovery_per_note_, bms_recovery_per_note_, 0.5 * bms_recovery_per_note_, -4.0, -6.0};
        case GaugeType::Easy:
        default:
            return {1.2 * bms_recovery_per_note_, 1.2 * bms_recovery_per_note_,
                    0.6 * bms_recovery_per_note_, -3.2, -4.8};
        }
    }
    switch (type) {
    case GaugeType::ExHard:
        return config_.ex_hard;
    case GaugeType::Hard:
        return config_.hard;
    case GaugeType::Normal:
        return config_.normal;
    case GaugeType::Easy:
    default:
        return config_.easy;
    }
}

GaugeResult GaugeManager::applyJudgement(GaugeState& state, Judgement judgement, double time_ms) const {
    return applyJudgementWeighted(state, judgement, time_ms, 1.0);
}

GaugeResult GaugeManager::applyJudgementWeighted(GaugeState& state, Judgement judgement, double time_ms,
                                                 double weight, bool empty_poor) const {
    static_cast<void>(time_ms);
    GaugeResult result{};
    if (state.game_over || (policy_.course_lr2_deltas && isFailedValue(state.value))) {
        state.game_over = true;
        result.game_over = true;
        return result;
    }

    double delta = deltaFor(state.type, judgement);
    if (judgement == Judgement::PR && empty_poor) {
        if (policy_.course_lr2_deltas) {
            delta = -2.0;
        } else if (policy_.bms_lr2_deltas) {
            delta = state.type == GaugeType::Easy ? -1.6 : -2.0;
            if (state.type == GaugeType::Hard || state.type == GaugeType::ExHard) {
                delta *= bms_hard_damage_scale_;
            }
        }
    }
    delta *= weight;
    if (policy_.course_lr2_deltas && state.value < kLr2CourseLowGaugeThreshold && delta < 0.0) {
        delta *= 0.6;
    }
    if (!policy_.course_lr2_deltas && !policy_.bms_lr2_deltas && state.type == GaugeType::Easy &&
        state.value <= kEasyLowGaugeBadSofteningThreshold &&
        is_easy_softened_bad_judgement(judgement) && delta < 0.0) {
        // Ease the death spiral slightly when the easy gauge is already nearly empty.
        delta *= kEasyLowGaugeBadSofteningScale;
    }
    if (!policy_.course_lr2_deltas && !policy_.bms_lr2_deltas &&
        state.type == GaugeType::Hard && judgement == Judgement::PR &&
        state.value <= kLr2HardPoorLowGaugeThreshold && delta < 0.0) {
        delta *= kLr2HardPoorLowGaugeScale;
    }
    if (!policy_.course_lr2_deltas && policy_.bms_lr2_deltas && state.type == GaugeType::Hard &&
        state.value < kLr2CourseLowGaugeThreshold && delta < 0.0) {
        delta *= 0.6;
    }
    state.value = std::clamp(state.value + delta, kMinGauge, max_gauge_for(state.type));
    if (policy_.course_lr2_deltas) {
        state.game_over = isFailedValue(state.value);
        result.game_over = state.game_over;
        return result;
    }

    const auto shift_to = [&](GaugeType destination) {
        state.type = destination;
        if (policy_.refill_on_shift) {
            state.value = max_gauge_for(destination);
        } else {
            state.value = std::clamp(state.value, kMinGauge, max_gauge_for(destination));
        }
        state.game_over = false;
        result.downshifted = true;
    };

    const double hard_to_normal_threshold =
        std::clamp(policy_.hard_to_normal_threshold, kMinGauge, kGaugeMax);
    if (policy_.hard_to_normal_shift && state.type == GaugeType::Hard &&
        state.value <= hard_to_normal_threshold) {
        shift_to(GaugeType::Normal);
        return result;
    }

    const double normal_to_easy_threshold =
        std::clamp(policy_.normal_to_easy_threshold, kMinGauge, kGaugeMax);
    if (policy_.normal_to_easy_shift && state.type == GaugeType::Normal &&
        state.value <= normal_to_easy_threshold) {
        shift_to(GaugeType::Easy);
        return result;
    }

    if (state.value <= 0.0) {
        state.game_over = true;
        result.game_over = true;
    }

    return result;
}

GaugeResult GaugeManager::applyDamage(GaugeState& state, double damage_percent, double time_ms) const {
    static_cast<void>(time_ms);
    GaugeResult result{};
    if (state.game_over || (policy_.course_lr2_deltas && isFailedValue(state.value))) {
        state.game_over = true;
        result.game_over = true;
        return result;
    }

    const double safe_damage =
        std::isfinite(damage_percent) ? std::max(0.0, damage_percent) : kGaugeMax;
    state.value = std::clamp(state.value - safe_damage, kMinGauge, max_gauge_for(state.type));
    if (policy_.course_lr2_deltas) {
        state.game_over = isFailedValue(state.value);
        result.game_over = state.game_over;
        return result;
    }

    const auto shift_to = [&](GaugeType destination) {
        state.type = destination;
        if (policy_.refill_on_shift) {
            state.value = max_gauge_for(destination);
        } else {
            state.value = std::clamp(state.value, kMinGauge, max_gauge_for(destination));
        }
        state.game_over = false;
        result.downshifted = true;
    };

    const double hard_threshold =
        std::clamp(policy_.hard_to_normal_threshold, kMinGauge, kGaugeMax);
    if (policy_.hard_to_normal_shift && state.type == GaugeType::Hard &&
        state.value <= hard_threshold) {
        shift_to(GaugeType::Normal);
        return result;
    }
    const double normal_threshold =
        std::clamp(policy_.normal_to_easy_threshold, kMinGauge, kGaugeMax);
    if (policy_.normal_to_easy_shift && state.type == GaugeType::Normal &&
        state.value <= normal_threshold) {
        shift_to(GaugeType::Easy);
        return result;
    }
    if (state.value <= 0.0) {
        state.game_over = true;
        result.game_over = true;
    }
    return result;
}

bool GaugeManager::isFailedValue(double value) const noexcept {
    return !std::isfinite(value) || (policy_.course_lr2_deltas
                                       ? value < kLr2CourseFailureThreshold : value <= 0.0);
}

double GaugeManager::deltaFor(GaugeType type, Judgement judgement) const noexcept {
    auto table = tableFor(type);
    switch (judgement) {
    case Judgement::PG:
        return table.pg;
    case Judgement::GR:
        return table.gr;
    case Judgement::GD:
        return table.gd;
    case Judgement::BD:
        return table.bd;
    case Judgement::PR:
        return table.pr;
    }
    return 0.0;
}

}  // namespace tenriff::game
