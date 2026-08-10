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

double max_gauge_for(GaugeType type) {
    static_cast<void>(type);
    return kGaugeMax;
}

bool is_easy_softened_bad_judgement(Judgement judgement) {
    return judgement == Judgement::BD;
}
}

GaugeManager::GaugeManager(GaugeConfig config, GaugeRuntimePolicy policy)
    : config_(config), policy_(policy) {}

GaugeState GaugeManager::initialState(GaugeType type) const noexcept {
    GaugeState state;
    state.type = type;
    state.value = max_gauge_for(type);
    state.game_over = false;
    return state;
}

GaugeDeltaTable GaugeManager::tableFor(GaugeType type) const noexcept {
    if (policy_.course_hybrid_deltas && type == GaugeType::Normal) {
        return GaugeDeltaTable{
            config_.ex_hard.pg,
            config_.ex_hard.gr,
            config_.ex_hard.gd,
            config_.easy.bd,
            config_.easy.pr,
        };
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
                                                 double weight) const {
    static_cast<void>(time_ms);
    GaugeResult result{};
    if (state.game_over) {
        result.game_over = true;
        return result;
    }

    double delta = deltaFor(state.type, judgement) * weight;
    if (state.type == GaugeType::Easy && state.value <= kEasyLowGaugeBadSofteningThreshold &&
        is_easy_softened_bad_judgement(judgement) && delta < 0.0) {
        // Ease the death spiral slightly when the easy gauge is already nearly empty.
        delta *= kEasyLowGaugeBadSofteningScale;
    }
    if (state.type == GaugeType::Hard && judgement == Judgement::PR &&
        state.value <= kLr2HardPoorLowGaugeThreshold && delta < 0.0) {
        delta *= kLr2HardPoorLowGaugeScale;
    }
    state.value = std::clamp(state.value + delta, kMinGauge, max_gauge_for(state.type));

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
    if (state.game_over) {
        result.game_over = true;
        return result;
    }

    const double safe_damage =
        std::isfinite(damage_percent) ? std::max(0.0, damage_percent) : kGaugeMax;
    state.value = std::clamp(state.value - safe_damage, kMinGauge, max_gauge_for(state.type));

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
