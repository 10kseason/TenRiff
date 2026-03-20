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
    state.value = 50.0;

    auto result = manager.applyJudgement(state, Judgement::PG, 0.0);
    CHECK_FALSE(result.downshifted);
    CHECK(almost_equal(state.value, 50.01));

    result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(almost_equal(state.value, 48.01));
}

TEST_CASE("all gauge types now start at full value") {
    GaugeManager manager;

    const auto hard = manager.initialState(GaugeType::Hard);
    CHECK(hard.type == GaugeType::Hard);
    CHECK(almost_equal(hard.value, 100.0));
    CHECK_FALSE(hard.game_over);

    const auto normal = manager.initialState(GaugeType::Normal);
    CHECK(normal.type == GaugeType::Normal);
    CHECK(almost_equal(normal.value, 100.0));
    CHECK_FALSE(normal.game_over);

    const auto easy = manager.initialState(GaugeType::Easy);
    CHECK(easy.type == GaugeType::Easy);
    CHECK(almost_equal(easy.value, 100.0));
    CHECK_FALSE(easy.game_over);
}

TEST_CASE("default PG recovery matches requested per-hit values") {
    tenriff::game::GaugeConfig config;
    config.auto_shift = false;
    GaugeManager manager(config);

    GaugeState hard{GaugeType::Hard, 0.0, false};
    manager.applyJudgement(hard, Judgement::PG, 0.0);
    CHECK(almost_equal(hard.value, 0.01, 1e-9));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    manager.applyJudgement(normal, Judgement::PG, 0.0);
    CHECK(almost_equal(normal.value, 0.01, 1e-9));

    GaugeState easy{GaugeType::Easy, 0.0, false};
    manager.applyJudgement(easy, Judgement::PG, 0.0);
    CHECK(almost_equal(easy.value, 0.032, 1e-9));
}

TEST_CASE("default GR recovery uses the requested per-gauge values") {
    tenriff::game::GaugeConfig config;
    config.auto_shift = false;
    GaugeManager manager(config);

    GaugeState hard{GaugeType::Hard, 0.0, false};
    manager.applyJudgement(hard, Judgement::GR, 0.0);
    CHECK(almost_equal(hard.value, 1.0 / 20.0, 1e-9));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    manager.applyJudgement(normal, Judgement::GR, 0.0);
    CHECK(almost_equal(normal.value, 1.0 / 20.0, 1e-9));

    GaugeState easy{GaugeType::Easy, 0.0, false};
    manager.applyJudgement(easy, Judgement::GR, 0.0);
    CHECK(almost_equal(easy.value, 0.032 / 20.0, 1e-9));
}

TEST_CASE("default GD recovery uses the requested per-gauge values") {
    tenriff::game::GaugeConfig config;
    config.auto_shift = false;
    GaugeManager manager(config);

    GaugeState hard{GaugeType::Hard, 0.0, false};
    manager.applyJudgement(hard, Judgement::GD, 0.0);
    CHECK(almost_equal(hard.value, 1.0 / 65.0, 1e-9));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    manager.applyJudgement(normal, Judgement::GD, 0.0);
    CHECK(almost_equal(normal.value, 1.0 / 65.0, 1e-9));

    GaugeState easy{GaugeType::Easy, 0.0, false};
    manager.applyJudgement(easy, Judgement::GD, 0.0);
    CHECK(almost_equal(easy.value, 0.032 / 50.0, 1e-9));
}

TEST_CASE("default gauge penalties keep hard harshest while normal and easy share the lower loss") {
    GaugeManager manager;
    const auto& config = manager.config();

    CHECK(almost_equal(config.hard.bd, -4.0));
    CHECK(almost_equal(config.normal.bd, -2.0));
    CHECK(almost_equal(config.easy.bd, -2.0));
    CHECK(std::abs(config.hard.bd) > std::abs(config.normal.bd));
    CHECK(almost_equal(config.normal.bd, config.easy.bd));
    CHECK(almost_equal(config.hard.pr, config.hard.bd));
    CHECK(almost_equal(config.normal.pr, config.normal.bd));
    CHECK(almost_equal(config.easy.pr, config.easy.bd));
}

TEST_CASE("good judgement refills by the shared amount") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 42.0;

    const auto result = manager.applyJudgement(state, Judgement::GD, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK(almost_equal(state.value, 42.0 + (1.0 / 65.0), 1e-9));
}

TEST_CASE("gauge downshifts at thresholds without refill and does not upshift") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 66.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 1000.0);
    CHECK(result.downshifted);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 62.1));

    result = manager.applyJudgement(state, Judgement::PG, 2000.0);
    CHECK_FALSE(result.downshifted);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 62.11));
}

TEST_CASE("gauge normal downshifts to easy at the lower threshold") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 33.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(result.downshifted);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 31.1));
}

TEST_CASE("gauge downshifts by at most one step per judgement") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 34.0;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 30.0));
}

TEST_CASE("easy gauge bad penalty softens slightly at or below twenty five percent") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 25.0;

    const auto result = manager.applyJudgement(state, Judgement::BD, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(almost_equal(state.value, 23.2, 1e-7));
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
