#include "game/GaugeManager.h"

#include <algorithm>
#include <cmath>

namespace tenriff::game {

namespace {
constexpr double kHardMaxGauge = 100.0;
constexpr double kNormalMaxGauge = 50.0;
constexpr double kEasyMaxGauge = 40.0;
constexpr double kMinGauge = 0.0;
constexpr double kEasyLowGaugeBadSofteningThreshold = 25.0;
constexpr double kEasyLowGaugeBadSofteningScale = 0.90;

double max_gauge_for(GaugeType type) {
    switch (type) {
    case GaugeType::Hard:
        return kHardMaxGauge;
    case GaugeType::Normal:
        return kNormalMaxGauge;
    case GaugeType::Easy:
    default:
        return kEasyMaxGauge;
    }
}

bool is_bad_judgement(Judgement judgement) {
    return judgement == Judgement::BD || judgement == Judgement::PR;
}
}

GaugeManager::GaugeManager(GaugeConfig config) : config_(config) {}

GaugeState GaugeManager::initialState(GaugeType type) const noexcept {
    GaugeState state;
    state.type = type;
    state.value = max_gauge_for(type);
    state.game_over = false;
    return state;
}

GaugeDeltaTable GaugeManager::tableFor(GaugeType type) const noexcept {
    switch (type) {
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
        is_bad_judgement(judgement) && delta < 0.0) {
        // Ease the death spiral slightly when the easy gauge is already nearly empty.
        delta *= kEasyLowGaugeBadSofteningScale;
    }
    state.value = std::clamp(state.value + delta, kMinGauge, max_gauge_for(state.type));

    if (!config_.auto_shift) {
        if (state.value <= 0.0) {
            state.game_over = true;
            result.game_over = true;
        }
        return result;
    }

    if (state.type == GaugeType::Hard && state.value <= config_.hard_to_normal_threshold) {
        state.type = GaugeType::Normal;
        state.value = max_gauge_for(state.type);
        result.downshifted = true;
    } else if (state.type == GaugeType::Normal && state.value <= config_.normal_to_easy_threshold) {
        state.type = GaugeType::Easy;
        state.value = max_gauge_for(state.type);
        result.downshifted = true;
    } else if (state.type == GaugeType::Easy && state.value <= 0.0) {
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
        return table.bd;
    }
    return 0.0;
}

}  // namespace tenriff::game
