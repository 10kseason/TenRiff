#include <algorithm>
#include <limits>
#include <vector>

#include "doctest/doctest.h"

#include "gameplay/ModeApplier.h"

namespace {

struct Span {
    int lane = 0;
    int64_t start = 0;
    int64_t end = 0;
};

bool has_lane_overlap(const tenriff::gameplay::GameplayChart& chart) {
    const int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        return false;
    }

    std::vector<Span> spans;
    spans.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({note.lane, note.start_sample, end});
    }

    std::sort(spans.begin(), spans.end(), [](const Span& lhs, const Span& rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });

    std::vector<int64_t> lane_end(static_cast<std::size_t>(lane_count), std::numeric_limits<int64_t>::min());
    for (const auto& span : spans) {
        if (span.lane <= 0 || span.lane > lane_count) {
            continue;
        }
        const auto index = static_cast<std::size_t>(span.lane - 1);
        if (span.start <= lane_end[index]) {
            return true;
        }
        lane_end[index] = std::max(lane_end[index], span.end);
    }
    return false;
}

}  // namespace

TEST_CASE("Super Random avoids overlapping lanes") {
    using namespace tenriff::gameplay;

    GameplayChart chart;
    chart.lane_count = 4;
    chart.duration_samples = 1000;
    chart.notes = {
        {1, 100, 150},
        {2, 100, std::nullopt},
        {3, 120, std::nullopt},
        {4, 140, std::nullopt},
        {1, 200, std::nullopt},
        {2, 240, 320},
        {3, 260, std::nullopt}
    };

    ModeSettings settings;
    settings.random = RandomMode::SuperRandom;
    settings.random_seed = 1234;

    auto result = apply_mode_settings(chart, settings);
    CHECK_FALSE(has_lane_overlap(result.chart));
}

TEST_CASE("key mode parser accepts 4K through 10K") {
    using tenriff::gameplay::KeyMode;
    using tenriff::gameplay::parse_key_mode;

    REQUIRE(parse_key_mode("4k").has_value());
    CHECK(parse_key_mode("4k").value() == KeyMode::Keys4);
    REQUIRE(parse_key_mode("5K").has_value());
    CHECK(parse_key_mode("5K").value() == KeyMode::Keys5);
    REQUIRE(parse_key_mode("6key").has_value());
    CHECK(parse_key_mode("6key").value() == KeyMode::Keys6);
    REQUIRE(parse_key_mode("7k").has_value());
    CHECK(parse_key_mode("7k").value() == KeyMode::Keys7);
    REQUIRE(parse_key_mode("8k").has_value());
    CHECK(parse_key_mode("8k").value() == KeyMode::Keys8);
    REQUIRE(parse_key_mode("9k").has_value());
    CHECK(parse_key_mode("9k").value() == KeyMode::Keys9);
    REQUIRE(parse_key_mode("10k").has_value());
    CHECK(parse_key_mode("10k").value() == KeyMode::Keys10);
}
