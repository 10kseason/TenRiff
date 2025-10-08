#include "doctest/doctest.h"

#include <cmath>

#include "chart/BmsChartNorm.h"
#include "chart/BmsTimeline.h"

using tenriff::chart::BmsChart;
using tenriff::chart::BmsChartNormalizer;
using tenriff::chart::BmsNormalizedEventType;
using tenriff::chart::BmsTimelineBuilder;

TEST_CASE("timeline builder converts normalized events to seconds") {
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
    auto timeline = builder.build(normalization.chart);

    bool timeline_success = timeline.success();
    CHECK(timeline_success);
    auto event_count = timeline.timeline.events.size();
    CHECK(event_count == 3u);
    if (!timeline_success || event_count != 3u) {
        return;
    }

    CHECK(timeline.timeline.events[0].event.type == BmsNormalizedEventType::Note);
    CHECK(std::abs(timeline.timeline.events[0].time - 0.0) < 1e-6);

    CHECK(timeline.timeline.events[1].event.type == BmsNormalizedEventType::BpmChange);
    CHECK(std::abs(timeline.timeline.events[1].time - 2.0) < 1e-6);

    CHECK(timeline.timeline.events[2].event.type == BmsNormalizedEventType::Note);
    CHECK(std::abs(timeline.timeline.events[2].time - 2.4) < 1e-6);

    CHECK(std::abs(timeline.timeline.duration - 3.6) < 1e-6);
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
    auto timeline = builder.build(normalization.chart);

    bool timeline_success = timeline.success();
    CHECK(timeline_success);
    auto event_count = timeline.timeline.events.size();
    CHECK(event_count == 3u);
    if (!timeline_success || event_count != 3u) {
        return;
    }

    CHECK(timeline.timeline.events[0].event.type == BmsNormalizedEventType::Stop);
    CHECK(std::abs(timeline.timeline.events[0].time - 0.0) < 1e-6);

    CHECK(timeline.timeline.events[1].event.type == BmsNormalizedEventType::Note);
    CHECK(std::abs(timeline.timeline.events[1].time - 0.0) < 1e-6);

    CHECK(timeline.timeline.events[2].event.type == BmsNormalizedEventType::Note);
    CHECK(std::abs(timeline.timeline.events[2].time - 3.0) < 1e-6);

    CHECK(std::abs(timeline.timeline.duration - 5.0) < 1e-6);
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
    auto result = builder.build(chart);

    CHECK_FALSE(result.success());
    CHECK_FALSE(result.messages.empty());
}

