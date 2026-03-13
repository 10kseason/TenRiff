#include "doctest/doctest.h"

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
