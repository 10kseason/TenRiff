#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "chart/BmsChartNorm.h"
#include "chart/BmsTimeline.h"

using tenriff::chart::BmsChart;
using tenriff::chart::BmsChartNormalizer;
using tenriff::chart::BmsNormalizedEventType;
using tenriff::chart::BmsTimelineBuilder;

TEST_CASE("timeline builder converts normalized events to samples") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.bpm["40"] = 150.0;

    chart.commands.push_back({0, "11", "0100"});
    chart.commands.push_back({1, "08", "4000"});
    chart.commands.push_back({1, "12", "00010000"});

    BmsChartNormalizer normalizer;
    auto normalization = normalizer.normalize(chart);
    bool normalization_success = normalization.success();
    CHECK(normalization_success);
    if (!normalization_success) {
        return;
    }

    BmsTimelineBuilder builder;
    auto timeline = builder.build(normalization.chart, 48000);

    bool timeline_success = timeline.success();
    CHECK(timeline_success);
    auto event_count = timeline.timeline.events.size();
    CHECK(event_count == 3u);
    if (!timeline_success || event_count != 3u) {
        return;
    }

    CHECK(timeline.timeline.events[0].event.type == BmsNormalizedEventType::Note);
    CHECK(timeline.timeline.events[0].time_samples == 0);

    CHECK(timeline.timeline.events[1].event.type == BmsNormalizedEventType::BpmChange);
    CHECK(timeline.timeline.events[1].time_samples == 96000);

    CHECK(timeline.timeline.events[2].event.type == BmsNormalizedEventType::Note);
    CHECK(timeline.timeline.events[2].time_samples == 115200);

    CHECK(timeline.timeline.duration_samples == 172800);
}

TEST_CASE("timeline builder applies stop durations after event groups") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.stop["AA"] = 96.0;

    chart.commands.push_back({0, "09", "AA00"});
    chart.commands.push_back({0, "11", "0100"});
    chart.commands.push_back({1, "11", "0100"});

    BmsChartNormalizer normalizer;
    auto normalization = normalizer.normalize(chart);
    bool normalization_success = normalization.success();
    CHECK(normalization_success);
    if (!normalization_success) {
        return;
    }

    BmsTimelineBuilder builder;
    auto timeline = builder.build(normalization.chart, 48000);

    bool timeline_success = timeline.success();
    CHECK(timeline_success);
    auto event_count = timeline.timeline.events.size();
    CHECK(event_count == 3u);
    if (!timeline_success || event_count != 3u) {
        return;
    }

    CHECK(timeline.timeline.events[0].event.type == BmsNormalizedEventType::Stop);
    CHECK(timeline.timeline.events[0].time_samples == 0);

    CHECK(timeline.timeline.events[1].event.type == BmsNormalizedEventType::Note);
    CHECK(timeline.timeline.events[1].time_samples == 0);

    CHECK(timeline.timeline.events[2].event.type == BmsNormalizedEventType::Note);
    CHECK(timeline.timeline.events[2].time_samples == 144000);

    CHECK(timeline.timeline.duration_samples == 240000);
}

TEST_CASE("timeline builder reports errors for non-positive bpm during advance") {
    tenriff::chart::BmsNormalizedChart chart;
    chart.base_bpm = 0.0;
    chart.lane_mapping = tenriff::chart::NoteLaneMapping::TenKeyDualPlayerDefault();
    chart.measures.push_back({0.0, 1.0});

    tenriff::chart::BmsNormalizedEvent event;
    event.type = BmsNormalizedEventType::Note;
    event.measure = 0;
    event.slice_index = 0;
    event.slice_count = 1;
    event.position = 1.0;
    chart.events.push_back(event);

    BmsTimelineBuilder builder;
    auto result = builder.build(chart, 48000);

    CHECK_FALSE(result.success());
    CHECK_FALSE(result.messages.empty());
}

TEST_CASE("timeline builder rejects non-positive sample rates") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.commands.push_back({0, "11", "0100"});

    BmsChartNormalizer normalizer;
    auto normalization = normalizer.normalize(chart);
    bool normalization_success = normalization.success();
    CHECK(normalization_success);
    if (!normalization_success) {
        return;
    }

    BmsTimelineBuilder builder;
    auto result = builder.build(normalization.chart, 0);

    CHECK_FALSE(result.success());
    CHECK_FALSE(result.messages.empty());
}

TEST_CASE("timeline builder applies channel 03 hex BPM before advancing positions") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.commands.push_back({0, "03", "C0"});
    chart.commands.push_back({1, "11", "01"});

    BmsChartNormalizer normalizer;
    auto normalization = normalizer.normalize(chart);
    bool normalization_success = normalization.success();
    CHECK(normalization_success);
    if (!normalization_success) {
        return;
    }

    BmsTimelineBuilder builder;
    auto timeline = builder.build(normalization.chart, 48000);
    bool timeline_success = timeline.success();
    CHECK(timeline_success);
    if (!timeline_success) {
        return;
    }

    bool found_note = false;
    for (const auto& evt : timeline.timeline.events) {
        if (evt.event.type == BmsNormalizedEventType::Note) {
            found_note = true;
            CHECK(evt.time_samples == 60000);
            break;
        }
    }
    CHECK(found_note);
}

TEST_CASE("timeline builder applies fractional channel 08 BPM references before advancing positions") {
    BmsChart chart;
    chart.base_bpm = 133.0;
    chart.bpm["01"] = 66.5;
    chart.commands.push_back({0, "08", "01"});
    chart.commands.push_back({1, "11", "01"});

    BmsChartNormalizer normalizer;
    auto normalization = normalizer.normalize(chart);
    bool normalization_success = normalization.success();
    CHECK(normalization_success);
    if (!normalization_success) {
        return;
    }

    BmsTimelineBuilder builder;
    auto timeline = builder.build(normalization.chart, 48000);
    bool timeline_success = timeline.success();
    CHECK(timeline_success);
    if (!timeline_success) {
        return;
    }

    bool found_note = false;
    const auto expected_note_time = static_cast<int64_t>(std::llround(((4.0 * 60.0) / 66.5) * 48000.0));
    for (const auto& evt : timeline.timeline.events) {
        if (evt.event.type == BmsNormalizedEventType::Note) {
            found_note = true;
            CHECK(evt.time_samples == expected_note_time);
            break;
        }
    }
    CHECK(found_note);
}

TEST_CASE("timeline scroll segments support speed changes stops and reverse motion") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.scroll["01"] = 2.0;
    chart.scroll["02"] = 0.0;
    chart.scroll["03"] = -1.0;
    chart.commands.push_back({0, "SC", "01"});
    chart.commands.push_back({1, "SC", "02"});
    chart.commands.push_back({2, "SC", "03"});
    chart.commands.push_back({3, "11", "01"});

    BmsChartNormalizer normalizer;
    const auto normalization = normalizer.normalize(chart);
    REQUIRE(normalization.success());

    BmsTimelineBuilder builder;
    const auto timeline = builder.build(normalization.chart, 1000);
    REQUIRE(timeline.success());
    REQUIRE(timeline.timeline.scroll_segments.size() == 4u);

    const auto& fast = timeline.timeline.scroll_segments[0];
    CHECK(fast.start_sample == 0);
    CHECK(fast.end_sample == 2000);
    CHECK(fast.start_position == doctest::Approx(0.0));
    CHECK(fast.end_position == doctest::Approx(2.0));

    const auto& frozen = timeline.timeline.scroll_segments[1];
    CHECK(frozen.start_sample == 2000);
    CHECK(frozen.end_sample == 4000);
    CHECK(frozen.start_position == doctest::Approx(2.0));
    CHECK(frozen.end_position == doctest::Approx(2.0));

    const auto& reverse = timeline.timeline.scroll_segments[2];
    CHECK(reverse.start_sample == 4000);
    CHECK(reverse.end_sample == 6000);
    CHECK(reverse.start_position == doctest::Approx(2.0));
    CHECK(reverse.end_position == doctest::Approx(1.0));

    const auto note = std::find_if(timeline.timeline.events.begin(), timeline.timeline.events.end(), [](const auto& event) {
        return event.event.type == BmsNormalizedEventType::Note;
    });
    REQUIRE(note != timeline.timeline.events.end());
    CHECK(note->time_samples == 6000);
    CHECK(timeline.timeline.duration_samples == 8000);
}
