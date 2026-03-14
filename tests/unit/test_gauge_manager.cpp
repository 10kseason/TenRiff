#include <cmath>

#include "doctest/doctest.h"

#include "game/GaugeManager.h"

using tenriff::game::GaugeManager;
using tenriff::game::GaugeState;
using tenriff::game::GaugeType;
using tenriff::game::Judgement;

namespace {
bool almost_equal(double lhs, double rhs, double eps = 1e-9) {
    return std::abs(lhs - rhs) <= eps;
}
}  // namespace

TEST_CASE("gauge applies judgement deltas and clamps") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);

    auto result = manager.applyJudgement(state, Judgement::PG, 0.0);
    CHECK_FALSE(result.downshifted);
    CHECK(almost_equal(state.value, 50.23123));

    result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(almost_equal(state.value, 48.6854));
}

TEST_CASE("gauge downshifts at thresholds without refill and does not upshift") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 66.1;

    auto result = manager.applyJudgement(state, Judgement::PR, 1000.0);
    CHECK(result.downshifted);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 62.2115));

    result = manager.applyJudgement(state, Judgement::PG, 2000.0);
    CHECK_FALSE(result.downshifted);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 62.44273));
}

TEST_CASE("gauge normal downshifts to easy at the lower threshold") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 33.1;

    auto result = manager.applyJudgement(state, Judgement::PR, 0.0);
    CHECK(result.downshifted);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 29.98975));
}

TEST_CASE("gauge downshifts by at most one step per judgement") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 34.0;

    auto result = manager.applyJudgement(state, Judgement::PR, 0.0);
    CHECK(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 30.1115));
}

TEST_CASE("easy gauge triggers game over on empty") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 0.05;

    auto result = manager.applyJudgement(state, Judgement::PR, 5000.0);
    CHECK(result.game_over);
    CHECK(state.game_over);
}

TEST_CASE("auto shift can be disabled to fail immediately") {
    tenriff::game::GaugeConfig config;
    config.auto_shift = false;
    GaugeManager manager(config);

    auto state = manager.initialState(GaugeType::Hard);
    state.value = 0.1;

    auto result = manager.applyJudgement(state, Judgement::PR, 0.0);
    CHECK(result.game_over);
    CHECK(state.game_over);
    CHECK(state.type == GaugeType::Hard);
}
