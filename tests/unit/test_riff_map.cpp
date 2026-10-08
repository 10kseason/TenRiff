#include "doctest/doctest.h"

#include <algorithm>
#include <limits>

#include "app/GameplayHudRevisions.h"
#include "gameplay/RiffMap.h"

using tenriff::gameplay::GameplayChart;
using tenriff::gameplay::build_riff_map;

TEST_CASE("riff map is empty for no heads or a nonpositive duration") {
    GameplayChart chart;
    CHECK(build_riff_map(chart).count == 0);
    chart.duration_samples = 6400;
    chart.mines.push_back({1, 100, 5.0});
    CHECK(build_riff_map(chart).count == 0);
    chart.notes.push_back({1, 100, 300});
    chart.duration_samples = 0;
    CHECK(build_riff_map(chart).count == 0);
    chart.duration_samples = -10;
    CHECK(build_riff_map(chart).count == 0);
}

TEST_CASE("riff map normalizes a concentrated bin to 255 and ignores tails and mines") {
    GameplayChart chart;
    chart.duration_samples = 6400;
    chart.notes = {{1, 100, 6300}, {2, 110, std::nullopt}, {3, 120, std::nullopt}};
    chart.mines = {{1, 4000, 5.0}, {2, 5000, 10.0}};
    const auto map = build_riff_map(chart);
    REQUIRE(map.count == 64);
    CHECK(map.values[1] == 255);
    CHECK(std::count(map.values.begin(), map.values.end(), uint8_t{0}) == 63);
    // Building display metadata must not trim, reorder or offset the chart.
    CHECK(chart.notes[0].start_sample == 100);
    CHECK(chart.notes[0].end_sample == 6300);
    CHECK(chart.duration_samples == 6400);
}

TEST_CASE("riff map assigns equal density to uniformly distributed note heads") {
    GameplayChart chart;
    chart.duration_samples = 6400;
    for (int bin = 0; bin < 64; ++bin) {
        chart.notes.push_back({1, bin * 100 + 50, std::nullopt});
    }
    const auto map = build_riff_map(chart);
    REQUIRE(map.count == 64);
    CHECK(std::all_of(map.values.begin(), map.values.end(), [](uint8_t value) { return value == 255; }));
}

TEST_CASE("riff map rounds normalized relative counts and keeps the maximum exact") {
    GameplayChart chart;
    chart.duration_samples = 4000;
    chart.notes = {{1, 0, std::nullopt}, {1, 1000, std::nullopt}, {2, 1100, std::nullopt},
                   {1, 2000, std::nullopt}, {2, 2100, std::nullopt}, {3, 2200, std::nullopt}};
    const auto map = build_riff_map(chart, 4);
    REQUIRE(map.count == 4);
    CHECK(map.values[0] == 85);
    CHECK(map.values[1] == 170);
    CHECK(map.values[2] == 255);
    CHECK(map.values[3] == 0);
    CHECK(std::all_of(map.values.begin() + 4, map.values.end(), [](uint8_t value) { return value == 0; }));
}

TEST_CASE("riff map assigns bin boundary and final sample heads to their proper bins") {
    GameplayChart chart;
    chart.duration_samples = 6400;
    chart.notes = {{1, 99, std::nullopt}, {1, 100, std::nullopt},
                   {1, 6399, std::nullopt}, {1, 6400, std::nullopt}};
    const auto map = build_riff_map(chart);
    CHECK(map.values[0] == 128);
    CHECK(map.values[1] == 128);
    CHECK(map.values[63] == 255);
    CHECK(std::count(map.values.begin(), map.values.end(), uint8_t{0}) == 61);
}

TEST_CASE("riff map handles fractional bins and int64 sample limits without overflow") {
    GameplayChart chart;
    chart.duration_samples = 10;
    chart.notes = {{1, 3, std::nullopt}, {1, 4, std::nullopt},
                   {1, 6, std::nullopt}, {1, 7, std::nullopt}};
    auto map = build_riff_map(chart, 3);
    CHECK(map.values[0] == 128);
    CHECK(map.values[1] == 255);
    CHECK(map.values[2] == 128);

    chart.duration_samples = (std::numeric_limits<int64_t>::max)();
    const int64_t midpoint = chart.duration_samples / 2;
    chart.notes = {{1, midpoint, std::nullopt}, {1, midpoint + 1, std::nullopt},
                   {1, chart.duration_samples, std::nullopt}};
    map = build_riff_map(chart, 2);
    CHECK(map.values[0] == 128);
    CHECK(map.values[1] == 255);
}

TEST_CASE("riff map bounds requested bin counts and malformed timestamps") {
    GameplayChart chart;
    chart.duration_samples = 3;
    chart.notes = {{1, -10, std::nullopt}, {1, 1, std::nullopt}, {1, 5, std::nullopt}};
    CHECK(build_riff_map(chart, 0).count == 0);
    auto map = build_riff_map(chart, 100);
    REQUIRE(map.count == 64);
    CHECK(map.values[0] == 255);
    CHECK(map.values[21] == 255);
    CHECK(map.values[63] == 255);
    map = build_riff_map(chart, 1);
    REQUIRE(map.count == 1);
    CHECK(map.values[0] == 255);
}

TEST_CASE("riff map remains aligned after chart rate conversion changes sample scale") {
    GameplayChart normal;
    normal.duration_samples = 6400;
    normal.notes = {{1, 200, std::nullopt}, {2, 3000, 6000}, {3, 6398, std::nullopt}};
    auto doubled_rate = normal;
    doubled_rate.duration_samples /= 2;
    for (auto& note : doubled_rate.notes) {
        note.start_sample /= 2;
        if (note.end_sample) *note.end_sample /= 2;
    }
    CHECK(build_riff_map(normal).values == build_riff_map(doubled_rate).values);
}

TEST_CASE("riff map replacement refreshes motion while leaving HUD text revision unchanged") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.riff_map_revision = 7;
    auto next = previous;
    CHECK_FALSE(tenriff::app::diff_gameplay_hud_revisions(previous, next).motion_changed);
    next.riff_map_revision = 8;
    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.motion_changed);
    CHECK_FALSE(diff.text_changed);
}
