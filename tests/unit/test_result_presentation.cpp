#include "doctest/doctest.h"

#include "render/ResultPresentation.h"

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
