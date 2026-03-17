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
    CHECK(almost_equal(state.value, 50.05238095));

    result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(almost_equal(state.value, 47.30238095));
}

TEST_CASE("default PG recovery matches requested count-to-5.5 ratios") {
    tenriff::game::GaugeConfig config;
    config.auto_shift = false;
    GaugeManager manager(config);

    GaugeState easy{GaugeType::Easy, 0.0, false};
    for (int i = 0; i < 55; ++i) {
        manager.applyJudgement(easy, Judgement::PG, 0.0);
    }
    CHECK(almost_equal(easy.value, 5.5, 1e-6));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    for (int i = 0; i < 105; ++i) {
        manager.applyJudgement(normal, Judgement::PG, 0.0);
    }
    CHECK(almost_equal(normal.value, 5.5, 1e-5));

    GaugeState hard{GaugeType::Hard, 0.0, false};
    for (int i = 0; i < 150; ++i) {
        manager.applyJudgement(hard, Judgement::PG, 0.0);
    }
    CHECK(almost_equal(hard.value, 5.5, 1e-5));
}

TEST_CASE("default gauge penalties follow the requested hard-normal-easy ratios") {
    GaugeManager manager;
    const auto& config = manager.config();

    CHECK(almost_equal(config.normal.bd, config.hard.bd * 0.5));
    CHECK(almost_equal(config.easy.bd, config.normal.bd * 0.75));
    CHECK(almost_equal(config.hard.pr, config.hard.bd));
    CHECK(almost_equal(config.normal.pr, config.normal.bd));
    CHECK(almost_equal(config.easy.pr, config.easy.bd));
}

TEST_CASE("gauge downshifts at thresholds without refill and does not upshift") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 66.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 1000.0);
    CHECK(result.downshifted);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 60.60000));

    result = manager.applyJudgement(state, Judgement::PG, 2000.0);
    CHECK_FALSE(result.downshifted);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 60.65238095));
}

TEST_CASE("gauge normal downshifts to easy at the lower threshold") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 33.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(result.downshifted);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 30.35000));
}

TEST_CASE("gauge downshifts by at most one step per judgement") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 34.0;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 28.50000));
}

TEST_CASE("easy gauge triggers game over on empty") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 0.05;

    auto result = manager.applyJudgement(state, Judgement::BD, 5000.0);
    CHECK(result.game_over);
    CHECK(state.game_over);
}

TEST_CASE("auto shift can be disabled to fail immediately") {
    tenriff::game::GaugeConfig config;
    config.auto_shift = false;
    GaugeManager manager(config);

    auto state = manager.initialState(GaugeType::Hard);
    state.value = 0.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(result.game_over);
    CHECK(state.game_over);
    CHECK(state.type == GaugeType::Hard);
}
