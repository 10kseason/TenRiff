#include <cmath>

#include "doctest/doctest.h"

#include "chart/OsuManiaLoader.h"

using tenriff::chart::OsuManiaLoader;
using tenriff::chart::OsuManiaParseResult;
using tenriff::chart::OsuParseSeverity;

TEST_CASE("osu!mania loader parses base metadata and notes") {
    const char* content = R"(osu file format v14
[General]
Mode:3
[Metadata]
Title:Test Song
Artist:Composer
[Difficulty]
CircleSize:4
OverallDifficulty:7.5
[TimingPoints]
0,500,4,0,0,100,1,0
[HitObjects]
64,192,0,1,0,0:0:0:0:
448,192,500,128,0,750:0:0:0:
)";

    OsuManiaLoader loader;
    OsuManiaParseResult result = loader.parse(content);

    CHECK(result.success());
    CHECK(result.messages.empty());
    CHECK(result.chart.key_count == 4);
    CHECK(std::abs(result.chart.overall_difficulty - 7.5) <= 1e-6);
    CHECK(result.chart.title == "Test Song");
    CHECK(result.chart.artist == "Composer");
    CHECK(result.chart.timing_points.size() == 1);
    if (result.chart.timing_points.size() != 1) {
        return;
    }
    CHECK(std::abs(result.chart.base_bpm - 120.0) <= 1e-6);

    CHECK(result.chart.notes.size() == 2);
    if (result.chart.notes.size() != 2) {
        return;
    }
    CHECK(result.chart.notes[0].column == 0);
    CHECK(result.chart.notes[0].start_time_ms == 0);
    CHECK_FALSE(result.chart.notes[0].end_time_ms.has_value());

    CHECK(result.chart.notes[1].column == 3);
    CHECK(result.chart.notes[1].start_time_ms == 500);
    CHECK(result.chart.notes[1].end_time_ms.has_value());
    if (result.chart.notes[1].end_time_ms.has_value()) {
        CHECK(result.chart.notes[1].end_time_ms.value() == 750);
    }
}

TEST_CASE("osu!mania loader surfaces validation errors") {
    const char* content = R"([General]
Mode:0
[HitObjects]
256,192,0,1,0,0:0:0:0:
)";

    OsuManiaLoader loader;
    OsuManiaParseResult result = loader.parse(content);

    CHECK_FALSE(result.success());
    // Expect mode error and missing KeyCount/CircleSize error at minimum.
    CHECK(result.messages.size() >= 2);
    if (result.messages.empty()) {
        return;
    }
    CHECK(result.messages[0].severity == OsuParseSeverity::Error);
}

TEST_CASE("osu!mania loader clamps invalid hold end time") {
    const char* content = R"([General]
Mode:3
[Difficulty]
CircleSize:2
[HitObjects]
0,0,1000,128,0,900:0:0:0:
)";

    OsuManiaLoader loader;
    OsuManiaParseResult result = loader.parse(content);

    CHECK(result.success());
    CHECK(result.messages.size() == 1);
    if (result.messages.size() != 1) {
        return;
    }
    CHECK(result.messages.front().severity == OsuParseSeverity::Warning);

    CHECK(result.chart.notes.size() == 1);
    CHECK(result.chart.notes[0].column == 0);
    CHECK(result.chart.notes[0].start_time_ms == 1000);
    CHECK(result.chart.notes[0].end_time_ms.has_value());
    if (result.chart.notes[0].end_time_ms.has_value()) {
        CHECK(result.chart.notes[0].end_time_ms.value() == 1000);
    }
}

TEST_CASE("osu!mania loader accepts legacy KeyCount alias") {
    const char* content = R"([General]
Mode:3
[Difficulty]
KeyCount:10
[HitObjects]
0,0,0,1,0,0:0:0:0:
)";

    OsuManiaLoader loader;
    OsuManiaParseResult result = loader.parse(content);

    CHECK(result.success());
    CHECK(result.chart.key_count == 10);
}

TEST_CASE("osu!mania loader parses background image from events") {
    const char* content = R"(osu file format v14
[General]
Mode:3
[Metadata]
Title:Preview Test
[Difficulty]
CircleSize:4
[Events]
//Background and Video events
0,0,"bgs/cover image.jpg",0,0
[HitObjects]
64,192,0,1,0,0:0:0:0:
)";

    OsuManiaLoader loader;
    OsuManiaParseResult result = loader.parse(content);

    CHECK(result.success());
    CHECK(result.chart.background_filename == "bgs/cover image.jpg");
}
