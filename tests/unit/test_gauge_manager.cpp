#include <cmath>

#include "doctest/doctest.h"

#include "game/GaugeManager.h"

using tenriff::game::GaugeManager;
using tenriff::game::GaugeRuntimePolicy;
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
    state.value = 40.0;

    auto result = manager.applyJudgement(state, Judgement::PG, 0.0);
    CHECK_FALSE(result.downshifted);
    CHECK(almost_equal(state.value, 40.19));

    result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK_FALSE(result.downshifted);
    CHECK(almost_equal(state.value, 33.94));
}

TEST_CASE("all gauge types now start at their cap") {
    GaugeManager manager;

    const auto ex_hard = manager.initialState(GaugeType::ExHard);
    CHECK(ex_hard.type == GaugeType::ExHard);
    CHECK(almost_equal(ex_hard.value, 100.0));
    CHECK_FALSE(ex_hard.game_over);

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
    GaugeManager manager;

    GaugeState ex_hard{GaugeType::ExHard, 0.0, false};
    manager.applyJudgement(ex_hard, Judgement::PG, 0.0);
    CHECK(almost_equal(ex_hard.value, 0.08, 1e-9));

    GaugeState hard{GaugeType::Hard, 0.0, false};
    manager.applyJudgement(hard, Judgement::PG, 0.0);
    CHECK(almost_equal(hard.value, 0.16, 1e-9));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    manager.applyJudgement(normal, Judgement::PG, 0.0);
    CHECK(almost_equal(normal.value, 0.19, 1e-9));

    GaugeState easy{GaugeType::Easy, 0.0, false};
    manager.applyJudgement(easy, Judgement::PG, 0.0);
    CHECK(almost_equal(easy.value, 0.25, 1e-9));
}

TEST_CASE("default GR recovery uses the requested per-gauge values") {
    GaugeManager manager;

    GaugeState ex_hard{GaugeType::ExHard, 0.0, false};
    manager.applyJudgement(ex_hard, Judgement::GR, 0.0);
    CHECK(almost_equal(ex_hard.value, 0.04, 1e-9));

    GaugeState hard{GaugeType::Hard, 0.0, false};
    manager.applyJudgement(hard, Judgement::GR, 0.0);
    CHECK(almost_equal(hard.value, 0.09, 1e-9));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    manager.applyJudgement(normal, Judgement::GR, 0.0);
    CHECK(almost_equal(normal.value, 0.15, 1e-9));

    GaugeState easy{GaugeType::Easy, 0.0, false};
    manager.applyJudgement(easy, Judgement::GR, 0.0);
    CHECK(almost_equal(easy.value, 0.20, 1e-9));
}

TEST_CASE("default GD recovery uses the requested per-gauge values") {
    GaugeManager manager;

    GaugeState ex_hard{GaugeType::ExHard, 0.0, false};
    manager.applyJudgement(ex_hard, Judgement::GD, 0.0);
    CHECK(almost_equal(ex_hard.value, 0.0, 1e-9));

    GaugeState hard{GaugeType::Hard, 0.0, false};
    manager.applyJudgement(hard, Judgement::GD, 0.0);
    CHECK(almost_equal(hard.value, 0.01, 1e-9));

    GaugeState normal{GaugeType::Normal, 0.0, false};
    manager.applyJudgement(normal, Judgement::GD, 0.0);
    CHECK(almost_equal(normal.value, 0.01, 1e-9));

    GaugeState easy{GaugeType::Easy, 0.0, false};
    manager.applyJudgement(easy, Judgement::GD, 0.0);
    CHECK(almost_equal(easy.value, 0.01, 1e-9));
}

TEST_CASE("default gauge penalties keep hard harshest while normal and easy stay below it") {
    GaugeManager manager;
    const auto& config = manager.config();

    CHECK(almost_equal(config.ex_hard.bd, -18.0));
    CHECK(almost_equal(config.hard.bd, -10.0));
    CHECK(almost_equal(config.normal.bd, -6.25));
    CHECK(almost_equal(config.easy.bd, -4.1));
    CHECK(std::abs(config.ex_hard.bd) > std::abs(config.hard.bd));
    CHECK(std::abs(config.hard.bd) > std::abs(config.normal.bd));
    CHECK(std::abs(config.normal.bd) > std::abs(config.easy.bd));
    CHECK(almost_equal(config.ex_hard.pr, -4.0));
    CHECK(almost_equal(config.hard.pr, -2.0));
    CHECK(almost_equal(config.normal.pr, -2.0));
    CHECK(almost_equal(config.easy.pr, -1.6));
}

TEST_CASE("ex-hard gauge has no low-gauge poor softening and can fail quickly") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::ExHard);
    state.value = 3.5;

    const auto result = manager.applyJudgement(state, Judgement::PR, 0.0);

    CHECK(result.game_over);
    CHECK(state.game_over);
    CHECK(state.type == GaugeType::ExHard);
    CHECK(almost_equal(state.value, 0.0));
}

TEST_CASE("hard gauge LR2 poor penalty softens at or below thirty percent") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 30.0;

    const auto result = manager.applyJudgement(state, Judgement::PR, 0.0);

    CHECK_FALSE(result.game_over);
    CHECK(almost_equal(state.value, 28.8, 1e-9));
}

TEST_CASE("good judgement refills by the configured amount") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 42.0;

    const auto result = manager.applyJudgement(state, Judgement::GD, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK(almost_equal(state.value, 42.01, 1e-9));
}

TEST_CASE("selected gauge type stays fixed while surviving") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 66.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 1000.0);
    CHECK_FALSE(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Hard);
    CHECK(almost_equal(state.value, 56.1));

    result = manager.applyJudgement(state, Judgement::PG, 2000.0);
    CHECK_FALSE(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Hard);
    CHECK(almost_equal(state.value, 56.26));
}

TEST_CASE("threshold runtime policy carries value through Hard Normal and Easy without refills") {
    GaugeRuntimePolicy policy;
    policy.hard_to_normal_shift = true;
    policy.hard_to_normal_threshold = 66.0;
    policy.normal_to_easy_shift = true;
    policy.normal_to_easy_threshold = 33.0;
    policy.refill_on_shift = false;

    tenriff::game::GaugeConfig config;
    config.hard.bd = -40.0;
    config.normal.bd = -30.0;
    GaugeManager manager(config, policy);
    auto state = manager.initialState(GaugeType::Hard);

    const auto normal_shift = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK(normal_shift.downshifted);
    CHECK_FALSE(normal_shift.game_over);
    CHECK(state.type == GaugeType::Normal);
    CHECK(state.value == doctest::Approx(60.0));

    const auto easy_shift = manager.applyJudgement(state, Judgement::BD, 1.0);
    CHECK(easy_shift.downshifted);
    CHECK_FALSE(easy_shift.game_over);
    CHECK(state.type == GaugeType::Easy);
    CHECK(state.value == doctest::Approx(30.0));
}

TEST_CASE("course hybrid gauge uses Ex-Hard recovery and Easy damage") {
    GaugeRuntimePolicy policy;
    policy.course_hybrid_deltas = true;
    GaugeManager manager({}, policy);
    GaugeState state{GaugeType::Normal, 50.0, false};

    manager.applyJudgement(state, Judgement::PG, 0.0);
    CHECK(state.value == doctest::Approx(50.08));
    manager.applyJudgement(state, Judgement::GR, 1.0);
    CHECK(state.value == doctest::Approx(50.12));
    manager.applyJudgement(state, Judgement::GD, 2.0);
    CHECK(state.value == doctest::Approx(50.12));
    manager.applyJudgement(state, Judgement::BD, 3.0);
    CHECK(state.value == doctest::Approx(46.02));
    manager.applyJudgement(state, Judgement::PR, 4.0);
    CHECK(state.value == doctest::Approx(44.42));
    CHECK(state.type == GaugeType::Normal);
}

TEST_CASE("course hybrid policy does not change ordinary Normal gauge") {
    GaugeManager manager;
    GaugeState state{GaugeType::Normal, 50.0, false};

    manager.applyJudgement(state, Judgement::PG, 0.0);
    manager.applyJudgement(state, Judgement::BD, 1.0);

    CHECK(state.value == doctest::Approx(43.94));
}
TEST_CASE("normal gauge remains fixed when the runtime shift policy is disabled") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 34.0;

    const auto result = manager.applyJudgement(state, Judgement::PR, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 32.0));
}

TEST_CASE("runtime normal-to-easy policy shifts at thirty three percent and refills Easy") {
    GaugeRuntimePolicy policy;
    policy.normal_to_easy_shift = true;
    tenriff::game::GaugeConfig config;
    config.normal.gd = 0.0;
    GaugeManager manager(config, policy);

    auto above_threshold = manager.initialState(GaugeType::Normal);
    above_threshold.value = 33.01;
    const auto above = manager.applyJudgement(above_threshold, Judgement::GD, 0.0);
    CHECK_FALSE(above.downshifted);
    CHECK(above_threshold.type == GaugeType::Normal);
    CHECK(almost_equal(above_threshold.value, 33.01));

    auto state = manager.initialState(GaugeType::Normal);
    state.value = 33.0;

    const auto shift = manager.applyJudgement(state, Judgement::GD, 0.0);

    CHECK(shift.downshifted);
    CHECK_FALSE(shift.game_over);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 100.0));
    CHECK_FALSE(state.game_over);

    const auto easy_hit = manager.applyJudgement(state, Judgement::PR, 1.0);
    CHECK_FALSE(easy_hit.downshifted);
    CHECK_FALSE(easy_hit.game_over);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 98.4));
}

TEST_CASE("hard gauge triggers game over on empty") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Hard);
    state.value = 0.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK_FALSE(result.downshifted);
    CHECK(result.game_over);
    CHECK(state.game_over);
    CHECK(state.type == GaugeType::Hard);
    CHECK(almost_equal(state.value, 0.0));
}

TEST_CASE("normal gauge triggers game over on empty") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Normal);
    state.value = 0.1;

    auto result = manager.applyJudgement(state, Judgement::BD, 0.0);
    CHECK_FALSE(result.downshifted);
    CHECK(result.game_over);
    CHECK(state.game_over);
    CHECK(state.type == GaugeType::Normal);
    CHECK(almost_equal(state.value, 0.0));
}

TEST_CASE("easy gauge can still recover up to full") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 99.9;

    const auto result = manager.applyJudgement(state, Judgement::PG, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 100.0));
}

TEST_CASE("easy gauge bad penalty softens slightly at or below twenty five percent") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 25.0;

    const auto result = manager.applyJudgement(state, Judgement::BD, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK_FALSE(result.game_over);
    CHECK(almost_equal(state.value, 21.31, 1e-7));
}

TEST_CASE("easy gauge triggers game over on empty") {
    GaugeManager manager;
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 0.05;

    auto result = manager.applyJudgement(state, Judgement::BD, 5000.0);
    CHECK_FALSE(result.downshifted);
    CHECK(result.game_over);
    CHECK(state.game_over);
}

TEST_CASE("runtime normal-to-easy policy still fails when Easy reaches zero") {
    GaugeRuntimePolicy policy;
    policy.normal_to_easy_shift = true;
    GaugeManager manager({}, policy);
    auto state = manager.initialState(GaugeType::Easy);
    state.value = 0.05;

    const auto result = manager.applyJudgement(state, Judgement::BD, 0.0);

    CHECK_FALSE(result.downshifted);
    CHECK(result.game_over);
    CHECK(state.game_over);
    CHECK(state.type == GaugeType::Easy);
    CHECK(almost_equal(state.value, 0.0));
}

TEST_CASE("fatal hits keep the selected gauge label") {
    GaugeManager manager;
    auto hard = manager.initialState(GaugeType::Hard);
    hard.value = 0.1;

    auto normal = manager.initialState(GaugeType::Normal);
    normal.value = 0.1;

    const auto hard_result = manager.applyJudgement(hard, Judgement::BD, 0.0);
    const auto normal_result = manager.applyJudgement(normal, Judgement::BD, 0.0);

    CHECK(hard_result.game_over);
    CHECK(normal_result.game_over);
    CHECK(hard.type == GaugeType::Hard);
    CHECK(normal.type == GaugeType::Normal);
}
