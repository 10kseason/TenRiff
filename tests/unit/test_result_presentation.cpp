#include "doctest/doctest.h"

#include "render/ResultPresentation.h"

TEST_CASE("Gauge Shift result accents do not recolor other result states") {
    using tenriff::render::result_gauge_shift_clear_color;
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT EASY CLEAR") == 0x72DB98);
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT NORMAL CLEAR") == 0xFFE477);
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT HARD CLEAR") == 0xFFAA60);
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT EX CLEAR") == 0xFF647A);
    for (const auto status : {"CLEAR", "EX CLEAR", "EASY CLEAR", "HARD CLEAR", "FAILED", "ABORTED",
                              "AUTOPLAY", "SUDDEN DEATH CLEAR", "PACEMAKER SCORE CLEAR",
                              "ASSIST PRACTICE GAUGE SHIFT EASY CLEAR"}) {
        CHECK_FALSE(result_gauge_shift_clear_color(status).has_value());
    }
}

TEST_CASE("result presentation unlocks controls after the staged reveal") {
    using tenriff::render::result_presentation_frame;

    const auto early = result_presentation_frame(1'000'000'000LL);
    CHECK(early.background == doctest::Approx(1.0f));
    CHECK(early.prism == doctest::Approx(1.0f));
    CHECK(early.score > 0.0f);
    CHECK(early.rank == doctest::Approx(0.0f));
    CHECK_FALSE(early.interaction_ready);

    const auto ready = result_presentation_frame(2'200'000'000LL);
    CHECK(ready.interaction_ready);
    CHECK(ready.statistics.back() > 0.0f);
    CHECK(ready.graph > 0.0f);
}

TEST_CASE("result presentation skip completes persistent phases without replaying impacts") {
    const auto skipped = tenriff::render::result_presentation_frame(0, true);
    CHECK(skipped.interaction_ready);
    CHECK(skipped.background == doctest::Approx(1.0f));
    CHECK(skipped.rank == doctest::Approx(1.0f));
    CHECK(skipped.controls == doctest::Approx(1.0f));
    CHECK(skipped.chromatic == doctest::Approx(0.0f));
    CHECK(skipped.score_pulse == doctest::Approx(0.0f));
}

TEST_CASE("result score countup leaves the last three digits for the slow finish") {
    using tenriff::render::result_counted_score;
    CHECK(result_counted_score(99'450, 0.0f) == 0);
    CHECK(result_counted_score(99'450, 0.76f) == 98'451);
    CHECK(result_counted_score(99'450, 1.0f) == 99'450);
    CHECK(result_counted_score(483, 1.0f) == 483);
}
