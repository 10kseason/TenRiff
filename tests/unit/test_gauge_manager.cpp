#include <cmath>
#include <limits>

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

TEST_CASE("LR2 course gauge uses beta3 grade deltas independently of configured gauges") {
    GaugeRuntimePolicy policy;
    policy.course_lr2_deltas = true;
    tenriff::game::GaugeConfig config;
    config.normal = {9.0, 9.0, 9.0, -99.0, -99.0};
    GaugeManager manager(config, policy);
    GaugeState state{GaugeType::Normal, 50.0, false};

    manager.applyJudgement(state, Judgement::PG, 0.0);
    CHECK(state.value == doctest::Approx(50.1));
    manager.applyJudgement(state, Judgement::GR, 1.0);
    CHECK(state.value == doctest::Approx(50.2));
    manager.applyJudgement(state, Judgement::GD, 2.0);
    CHECK(state.value == doctest::Approx(50.24));
    manager.applyJudgement(state, Judgement::BD, 3.0);
    CHECK(state.value == doctest::Approx(48.24));
    manager.applyJudgement(state, Judgement::PR, 4.0);
    CHECK(state.value == doctest::Approx(45.24));
    manager.applyJudgementWeighted(state, Judgement::PR, 5.0, 1.0, true);
    CHECK(state.value == doctest::Approx(43.24));
    CHECK(state.type == GaugeType::Normal);
}

TEST_CASE("LR2 course policy does not change ordinary Normal gauge") {
    GaugeManager manager;
    GaugeState state{GaugeType::Normal, 50.0, false};

    manager.applyJudgement(state, Judgement::PG, 0.0);
    manager.applyJudgement(state, Judgement::BD, 1.0);

    CHECK(state.value == doctest::Approx(43.94));
}

TEST_CASE("LR2 course low gauge damage uses the displayed thirty-percent cell boundary") {
    GaugeRuntimePolicy policy;
    policy.course_lr2_deltas = true;
    GaugeManager manager({}, policy);
    for (double initial : {29.9, 30.0, 31.999, 32.0, 32.001}) {
        const double scale = initial < 32.0 ? 0.6 : 1.0;
        GaugeState bad{GaugeType::Normal, initial, false};
        manager.applyJudgement(bad, Judgement::BD, 0.0);
        CHECK(bad.value == doctest::Approx(initial - 2.0 * scale));
        GaugeState missed{GaugeType::Normal, initial, false};
        manager.applyJudgement(missed, Judgement::PR, 0.0);
        CHECK(missed.value == doctest::Approx(initial - 3.0 * scale));
        GaugeState empty{GaugeType::Normal, initial, false};
        manager.applyJudgementWeighted(empty, Judgement::PR, 0.0, 1.0, true);
        CHECK(empty.value == doctest::Approx(initial - 2.0 * scale));
    }
    GaugeState crosses{GaugeType::Normal, 33.0, false};
    manager.applyJudgement(crosses, Judgement::PR, 0.0);
    CHECK(crosses.value == doctest::Approx(30.0));
    manager.applyJudgement(crosses, Judgement::PR, 1.0);
    CHECK(crosses.value == doctest::Approx(28.2));
}

TEST_CASE("LR2 course starts full and fails below two percent without revival or downshift") {
    GaugeRuntimePolicy policy;
    policy.course_lr2_deltas = true;
    policy.normal_to_easy_shift = true; // Course survival must take precedence.
    GaugeManager manager({}, policy);
    auto full = manager.initialState(GaugeType::Normal);
    CHECK(full.value == 100.0);
    manager.applyJudgement(full, Judgement::PG, 0.0);
    CHECK(full.value == 100.0);
    GaugeState boundary{GaugeType::Normal, 2.0, false};
    CHECK_FALSE(manager.isFailedValue(boundary.value));
    manager.applyJudgement(boundary, Judgement::PG, 0.0);
    CHECK(boundary.value == doctest::Approx(2.1));
    GaugeState failed{GaugeType::Normal, 3.19, false};
    const auto result = manager.applyJudgement(failed, Judgement::BD, 0.0);
    CHECK(result.game_over);
    CHECK_FALSE(result.downshifted);
    CHECK(failed.value == doctest::Approx(1.99));
    manager.applyJudgement(failed, Judgement::PG, 1.0);
    CHECK(failed.value == doctest::Approx(1.99));
    CHECK(failed.type == GaugeType::Normal);
    GaugeState stale{GaugeType::Normal, 1.99, false};
    CHECK(manager.applyJudgement(stale, Judgement::PG, 0.0).game_over);
    CHECK(stale.value == doctest::Approx(1.99));
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

namespace {
GaugeRuntimePolicy bms_lr2_policy(double total = 300.0, int notes = 1000) {
    GaugeRuntimePolicy policy;
    policy.bms_lr2_deltas = true;
    policy.bms_total = total;
    policy.bms_note_count = notes;
    return policy;
}

double bms_delta(const GaugeManager& manager, GaugeType type, Judgement judgement,
                 bool empty_poor = false, double initial = 50.0, double weight = 1.0) {
    GaugeState state{type, initial, false};
    manager.applyJudgementWeighted(state, judgement, 0.0, weight, empty_poor);
    return state.value - initial;
}
}  // namespace

TEST_CASE("BMS LR2 deltas use TOTAL recovery and distinct consumed versus empty POOR") {
    GaugeManager manager({}, bms_lr2_policy());
    const Judgement grades[]{Judgement::PG, Judgement::GR, Judgement::GD, Judgement::BD, Judgement::PR};
    const double normal[]{0.3, 0.3, 0.15, -4.0, -6.0};
    const double easy[]{0.36, 0.36, 0.18, -3.2, -4.8};
    const double hard[]{0.1, 0.1, 0.05, -6.0, -10.0};
    const double ex[]{0.1, 0.1, 0.05, -12.0, -20.0};
    for (int i = 0; i < 5; ++i) {
        CHECK(bms_delta(manager, GaugeType::Normal, grades[i]) == doctest::Approx(normal[i]));
        CHECK(bms_delta(manager, GaugeType::Easy, grades[i]) == doctest::Approx(easy[i]));
        CHECK(bms_delta(manager, GaugeType::Hard, grades[i]) == doctest::Approx(hard[i]));
        CHECK(bms_delta(manager, GaugeType::ExHard, grades[i]) == doctest::Approx(ex[i]));
    }
    CHECK(bms_delta(manager, GaugeType::Easy, Judgement::PR, true) == doctest::Approx(-1.6));
    for (auto type : {GaugeType::Normal, GaugeType::Hard, GaugeType::ExHard}) {
        CHECK(bms_delta(manager, type, Judgement::PR, true) == doctest::Approx(-2.0));
    }
    GaugeManager higher({}, bms_lr2_policy(600.0));
    CHECK(bms_delta(higher, GaugeType::Normal, Judgement::PG) == doctest::Approx(0.6));
    CHECK(bms_delta(higher, GaugeType::Easy, Judgement::GD) == doctest::Approx(0.36));
    CHECK(bms_delta(higher, GaugeType::Normal, Judgement::BD) == doctest::Approx(-4.0));
    CHECK(bms_delta(higher, GaugeType::Hard, Judgement::PG) == doctest::Approx(0.1));
}

TEST_CASE("BMS LR2 missing or invalid TOTAL uses the LR2 note-count fallback") {
    for (double total : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity()}) {
        GaugeManager manager({}, bms_lr2_policy(total, 500));
        // N=500 => 160 + (500+100)*.16 = 256.
        CHECK(bms_delta(manager, GaugeType::Normal, Judgement::PG) == doctest::Approx(0.512));
        CHECK(bms_delta(manager, GaugeType::Easy, Judgement::PG) == doctest::Approx(0.6144));
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::BD) == doctest::Approx(-12.0));
    }
    const struct { int notes; double total; } cases[]{{400, 224.0}, {600, 288.0}, {1000, 352.0}};
    for (const auto& row : cases) {
        GaugeManager manager({}, bms_lr2_policy(0.0, row.notes));
        CHECK(bms_delta(manager, GaugeType::Normal, Judgement::PG) == doctest::Approx(row.total / row.notes));
    }
    for (int count : {0, -5}) {
        GaugeManager empty({}, bms_lr2_policy(0.0, count));
        CHECK(bms_delta(empty, GaugeType::Normal, Judgement::PG) == 0.0);
        CHECK(bms_delta(empty, GaugeType::Easy, Judgement::GD) == 0.0);
        CHECK(std::isfinite(bms_delta(empty, GaugeType::Hard, Judgement::BD)));
    }
}

TEST_CASE("BMS LR2 Hard damage respects every low-note-count boundary") {
    const struct { int notes; double factor; } cases[]{
        {20, 10.0}, {21, 9.8}, {29, 8.2}, {30, 7.0}, {59, 5.0 + 0.2 / 3.0},
        {60, 5.0}, {124, 4.0 + 1.0 / 65.0}, {125, 4.0}, {249, 3.008},
        {250, 3.0}, {499, 2.004}, {500, 2.0}, {999, 1.002}, {1000, 1.0}, {2000, 1.0}};
    for (const auto& row : cases) {
        GaugeManager manager({}, bms_lr2_policy(1000.0, row.notes));
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::BD, false, 100.0) ==
              doctest::Approx(-6.0 * row.factor));
        CHECK(bms_delta(manager, GaugeType::ExHard, Judgement::BD, false, 80.0, 0.25) ==
              doctest::Approx(-3.0 * row.factor));
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::PR, true) ==
              doctest::Approx(-2.0 * row.factor));
    }
}

TEST_CASE("BMS LR2 Hard damage uses the larger TOTAL or note-count correction") {
    const struct { double total; double factor; } cases[]{
        {96.0, 10.0}, {159.0, 2.5}, {160.0, 2.0}, {175.0, 2.0},
        {176.0, 10.0 / 6.0}, {239.0, 10.0 / 9.0}, {240.0, 1.0}, {999.0, 1.0}};
    for (const auto& row : cases) {
        GaugeManager manager({}, bms_lr2_policy(row.total, 1000));
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::BD, false, 100.0) ==
              doctest::Approx(-6.0 * row.factor));
        CHECK(bms_delta(manager, GaugeType::ExHard, Judgement::PR, true) ==
              doctest::Approx(-2.0 * row.factor));
    }
    GaugeManager both({}, bms_lr2_policy(160.0, 250)); // TOTAL=2x, N=3x => 3x, not 6x.
    CHECK(bms_delta(both, GaugeType::Hard, Judgement::BD) == doctest::Approx(-18.0));
    CHECK(bms_delta(both, GaugeType::Normal, Judgement::BD) == doctest::Approx(-4.0));
    CHECK(bms_delta(both, GaugeType::Easy, Judgement::BD) == doctest::Approx(-3.2));
}

TEST_CASE("BMS LR2 only Hard softens all negative judgements below thirty two percent") {
    GaugeManager manager({}, bms_lr2_policy());
    for (double initial : {25.0, 30.0, 31.999, 32.0, 32.001}) {
        const double scale = initial < 32.0 ? 0.6 : 1.0;
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::BD, false, initial) == doctest::Approx(-6.0 * scale));
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::PR, false, initial) == doctest::Approx(-10.0 * scale));
        CHECK(bms_delta(manager, GaugeType::Hard, Judgement::PR, true, initial) == doctest::Approx(-2.0 * scale));
        CHECK(bms_delta(manager, GaugeType::ExHard, Judgement::BD, false, initial) == doctest::Approx(-12.0));
        CHECK(bms_delta(manager, GaugeType::Easy, Judgement::BD, false, initial) == doctest::Approx(-3.2));
    }
}

TEST_CASE("BMS LR2 deltas preserve TenRiff start zero failure mine damage and runtime shifts") {
    GaugeManager manager({}, bms_lr2_policy(160.0, 20));
    for (auto type : {GaugeType::Easy, GaugeType::Normal, GaugeType::Hard, GaugeType::ExHard}) {
        auto state = manager.initialState(type);
        CHECK(state.value == 100.0);
        CHECK_FALSE(manager.isFailedValue(1.99));
        CHECK(manager.isFailedValue(0.0));
        manager.applyDamage(state, 3.0);
        CHECK(state.value == doctest::Approx(97.0)); // Authored mine damage is not multiplied.
        state.value = 0.1;
        CHECK(manager.applyJudgement(state, Judgement::BD, 0.0).game_over);
        CHECK(state.value == 0.0);
        CHECK(state.type == type);
    }
    auto policy = bms_lr2_policy();
    policy.hard_to_normal_shift = true;
    policy.normal_to_easy_shift = true;
    GaugeManager shifting({}, policy);
    GaugeState hard{GaugeType::Hard, 70.0, false};
    CHECK(shifting.applyJudgement(hard, Judgement::BD, 0.0).downshifted);
    CHECK(hard.type == GaugeType::Normal);
    CHECK(hard.value == 100.0);
    GaugeState normal{GaugeType::Normal, 36.0, false};
    CHECK(shifting.applyJudgement(normal, Judgement::BD, 0.0).downshifted);
    CHECK(normal.type == GaugeType::Easy);
    CHECK(normal.value == 100.0);
}

TEST_CASE("BMS LR2 runtime policy is opt-in and course gauge takes precedence") {
    auto policy = bms_lr2_policy(160.0, 20);
    policy.bms_lr2_deltas = false;
    GaugeManager legacy({}, policy);
    CHECK(bms_delta(legacy, GaugeType::Normal, Judgement::PG) == doctest::Approx(0.19));
    CHECK(bms_delta(legacy, GaugeType::Hard, Judgement::BD) == doctest::Approx(-10.0));
    policy.bms_lr2_deltas = true;
    policy.course_lr2_deltas = true;
    GaugeManager course({}, policy);
    CHECK(bms_delta(course, GaugeType::Normal, Judgement::PG) == doctest::Approx(0.1));
    CHECK(bms_delta(course, GaugeType::Normal, Judgement::GD) == doctest::Approx(0.04));
    CHECK(bms_delta(course, GaugeType::Hard, Judgement::BD) == doctest::Approx(-2.0));
    CHECK(bms_delta(course, GaugeType::ExHard, Judgement::PR) == doctest::Approx(-3.0));
    CHECK(bms_delta(course, GaugeType::Easy, Judgement::PR, true) == doctest::Approx(-2.0));
    CHECK(course.isFailedValue(1.99));
}
