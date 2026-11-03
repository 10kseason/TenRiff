#include "doctest/doctest.h"

#include "render/ResultPresentation.h"
#include "app/RecordFeedback.h"
#include "app/ResultGaugeHistory.h"
#include "render/GameplayGaugePalette.h"

#include <limits>

TEST_CASE("Gauge Shift result accents do not recolor other result states") {
    using tenriff::render::result_gauge_shift_clear_color;
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT EASY CLEAR") == 0x72DB98);
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT NORMAL CLEAR") == 0xFFE477);
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT HARD CLEAR") == 0xFFAA60);
    CHECK(result_gauge_shift_clear_color("GAUGE SHIFT EX CLEAR") == 0x4AF3F7);
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

TEST_CASE("record celebration needs at least one score point and valid increasing ranks") {
    using namespace tenriff::app;
    RecordFeedback previous;
    CHECK_FALSE(record_score_improved(0, previous));
    CHECK(record_score_improved(1, previous));
    previous.has_previous = true;
    previous.previous_score = 9'500;
    CHECK_FALSE(record_score_improved(9'499, previous));
    CHECK_FALSE(record_score_improved(9'500, previous));
    CHECK(record_score_improved(9'501, previous));
    CHECK(record_rank_priority("S") > record_rank_priority("A"));
    CHECK(record_rank_priority("S+++") > record_rank_priority("S++"));
    CHECK(record_rank_priority("AUTO") == -1);
    CHECK(record_rank_priority("--") == -1);
}

TEST_CASE("score interference lasts exactly 1.2 seconds then restores the exact integer") {
    using namespace tenriff::render;
    const auto begin = kRecordScoreEffectStartNs;
    const auto end = begin + kRecordScoreEffectDurationNs;
    CHECK(result_record_effect_frame(begin - 1, false).distortion == 0.0f);
    CHECK(result_record_effect_frame(begin, false).distortion > 0.0f);
    CHECK(result_record_effect_frame(end - 1, false).distortion > 0.0f);
    CHECK(result_record_effect_frame(end, false).distortion == 0.0f);
    CHECK(result_record_score_text(9'501, 1.0f, begin, true) != "9501");
    CHECK(result_record_score_text(9'501, 0.0f, end, true) == "9501");
    CHECK(result_record_score_text(9'501, 0.0f, begin, true, true) == "9501");
    CHECK(result_record_score_text(9'501, 1.0f, begin, false) == "9501");
    CHECK(result_record_score_text(0, 1.0f, end, true) == "0");
}

TEST_CASE("rank upgrade brightens the old grade before shattering into the softer new grade") {
    using namespace tenriff::render;
    const auto before = result_record_effect_frame(800'000'000LL, true);
    const auto bright = result_record_effect_frame(1'200'000'000LL, true);
    const auto shatter = result_record_effect_frame(1'450'000'000LL, true);
    const auto after = result_record_effect_frame(2'000'000'000LL, true);
    CHECK(before.old_rank_opacity > 0.0f);
    CHECK(bright.old_rank_brightness > before.old_rank_brightness);
    CHECK(bright.shatter == 0.0f);
    CHECK(shatter.shatter > 0.0f);
    CHECK(shatter.new_rank_opacity == 0.0f);
    CHECK(after.old_rank_opacity == 0.0f);
    CHECK(after.new_rank_opacity == 1.0f);
    const auto skipped = result_record_effect_frame(0, true, true);
    CHECK(skipped.old_rank_opacity == 0.0f);
    CHECK(skipped.distortion == 0.0f);
    CHECK(skipped.new_rank_opacity == 1.0f);
}

TEST_CASE("Gauge Shift graph uses one EX100 to EASY0 axis without refill inversion") {
    using namespace tenriff;
    CHECK(app::result_total_gauge_value(game::GaugeType::ExHard, 100) == doctest::Approx(1.0));
    CHECK(app::result_total_gauge_value(game::GaugeType::ExHard, 0) ==
          app::result_total_gauge_value(game::GaugeType::Hard, 100));
    CHECK(app::result_total_gauge_value(game::GaugeType::Hard, 0) ==
          app::result_total_gauge_value(game::GaugeType::Normal, 100));
    CHECK(app::result_total_gauge_value(game::GaugeType::Normal, 0) ==
          app::result_total_gauge_value(game::GaugeType::Easy, 100));
    CHECK(app::result_total_gauge_value(game::GaugeType::Easy, 0) == doctest::Approx(0.0));
    gameplay::ResultStats stats;
    stats.gauge_history = {{0, 100}, {10, 3}, {20, 0}, {20, 100}, {30, 92}, {40, 100}, {50, 100}, {60, 0}};
    stats.shifts = {{20, game::GaugeType::ExHard, game::GaugeType::Hard},
                    {40, game::GaugeType::Hard, game::GaugeType::Normal},
                    {50, game::GaugeType::Normal, game::GaugeType::Easy}};
    const auto plot = app::result_display_gauge_history(stats, game::GaugeType::Easy, true);
    REQUIRE(plot.size() == 7);
    CHECK(plot[2].sample == 20);
    CHECK(plot[2].shifted);
    for (std::size_t i = 1; i < plot.size(); ++i) CHECK(plot[i].value <= plot[i - 1].value);
    CHECK(plot.front().value == doctest::Approx(1.0));
    CHECK(plot.back().value == doctest::Approx(0.0));
    // The display mapping must not mutate the replay/result's raw values.
    CHECK(stats.gauge_history[3].value == 100);
    CHECK(stats.gauge_history.size() == 8);
    const auto ordinary = app::result_display_gauge_history(stats, game::GaugeType::Easy, false);
    CHECK(ordinary.front().value == doctest::Approx(1.0));
    CHECK(ordinary[2].value == doctest::Approx(1.0));
}

TEST_CASE("colored result combo suffix leaves localized heading prefixes outside the glow") {
    using tenriff::render::result_semantic_suffix_range;
    for (const auto& prefix : {std::wstring(L"GRADE · "), std::wstring(L"등급 · "), std::wstring(L"グレード · ")}) {
        const std::wstring token = prefix == L"등급 · " ? L"풀 콤보" : L"FULL COMBO";
        const std::wstring label = prefix + token;
        const auto range = result_semantic_suffix_range(label, token);
        REQUIRE(range.has_value());
        CHECK(label.substr(0, range->start) == prefix);
        CHECK(label.substr(range->start, range->length) == token);
        CHECK(range->start + range->length == label.size());
    }
    const auto repeated = result_semantic_suffix_range(L"FULL COMBO · FULL COMBO", L"FULL COMBO");
    REQUIRE(repeated.has_value());
    CHECK(std::wstring(L"FULL COMBO · FULL COMBO").substr(0, repeated->start) == L"FULL COMBO · ");
    CHECK_FALSE(result_semantic_suffix_range(L"GRADE · FULL COMBO disabled", L"FULL COMBO").has_value());
    CHECK_FALSE(result_semantic_suffix_range(L"GRADE · ALL PERFECT", L"FULL COMBO").has_value());
    CHECK_FALSE(result_semantic_suffix_range(L"GRADE", L"FULL COMBO").has_value());
    CHECK_FALSE(result_semantic_suffix_range(L"GRADE", L"").has_value());
}

TEST_CASE("semantic text bloom is bounded without reducing crisp score or rank roles") {
    using tenriff::render::semantic_text_bloom_strength;
    CHECK(semantic_text_bloom_strength(0.0f, true) == 0.0f);
    CHECK(semantic_text_bloom_strength(0.12f, true) == doctest::Approx(0.12f));
    CHECK(semantic_text_bloom_strength(0.8f, true) == doctest::Approx(0.25f));
    CHECK(semantic_text_bloom_strength(-0.1f, true) == 0.0f);
    CHECK(semantic_text_bloom_strength(0.35f, false) == doctest::Approx(0.35f));
    CHECK(semantic_text_bloom_strength(4.0f, false) == 1.0f);
    CHECK(semantic_text_bloom_strength(std::numeric_limits<float>::quiet_NaN(), true) == 0.0f);
    CHECK(semantic_text_bloom_strength(std::numeric_limits<float>::infinity(), true) == 0.0f);
}
