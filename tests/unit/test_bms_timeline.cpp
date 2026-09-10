#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
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

TEST_CASE("lowercase BPM headers drive channel 03 and 08 speed changes end to end") {
    const char* data =
        "#bpm 120\n"
        "#bpmaa 240\n"
        "#00003:F0\n"
        "#00108:AA\n"
        "#00211:01\n";

    tenriff::chart::BmsParser parser;
    const auto parsed = parser.parse(data);
    REQUIRE(parsed.success());

    BmsChartNormalizer normalizer;
    const auto normalization = normalizer.normalize(parsed.chart);
    REQUIRE(normalization.success());

    BmsTimelineBuilder builder;
    const auto timeline = builder.build(normalization.chart, 48000);
    REQUIRE(timeline.success());

    const auto note = std::find_if(timeline.timeline.events.begin(), timeline.timeline.events.end(),
                                   [](const auto& event) {
                                       return event.event.type == BmsNormalizedEventType::Note;
                                   });
    REQUIRE(note != timeline.timeline.events.end());
    // Channel 03 changes measure 0 to 240 BPM and channel 08 keeps 240 BPM at
    // measure 1, so two four-beat measures take one second each.
    CHECK(note->time_samples == 96000);
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

TEST_CASE("timeline applies BPM changes to visual velocity") {
    BmsChart chart;
    chart.base_bpm = 200.0;
    chart.bpm["01"] = 100.0;
    chart.commands.push_back({1, "08", "01"});
    chart.commands.push_back({2, "11", "01"});

    BmsChartNormalizer normalizer;
    const auto normalization = normalizer.normalize(chart);
    REQUIRE(normalization.success());

    BmsTimelineBuilder builder;
    const auto timeline = builder.build(normalization.chart, 1000);
    REQUIRE(timeline.success());
    REQUIRE(timeline.timeline.scroll_segments.size() >= 2u);

    const auto& before_change = timeline.timeline.scroll_segments[0];
    const auto& after_change = timeline.timeline.scroll_segments[1];
    const double before_velocity =
        (before_change.end_position - before_change.start_position) /
        static_cast<double>(before_change.end_sample - before_change.start_sample);
    const double after_velocity =
        (after_change.end_position - after_change.start_position) /
        static_cast<double>(after_change.end_sample - after_change.start_sample);

    CHECK(before_change.end_sample == 1200);
    CHECK(after_change.end_sample - after_change.start_sample == 2400);
    CHECK(before_change.end_position - before_change.start_position == doctest::Approx(1.0));
    CHECK(after_change.end_position - after_change.start_position == doctest::Approx(1.0));
    CHECK(after_velocity == doctest::Approx(before_velocity * 0.5));
}

TEST_CASE("timeline combines BPM changes with BMS scroll factors") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.bpm["01"] = 240.0;
    chart.scroll["01"] = 0.5;
    chart.commands.push_back({1, "08", "01"});
    chart.commands.push_back({1, "SC", "01"});
    chart.commands.push_back({2, "11", "01"});

    BmsChartNormalizer normalizer;
    const auto normalization = normalizer.normalize(chart);
    REQUIRE(normalization.success());

    BmsTimelineBuilder builder;
    const auto timeline = builder.build(normalization.chart, 1000);
    REQUIRE(timeline.success());
    REQUIRE(timeline.timeline.scroll_segments.size() >= 2u);

    const auto& before_change = timeline.timeline.scroll_segments[0];
    const auto& after_change = timeline.timeline.scroll_segments[1];
    const double before_velocity =
        (before_change.end_position - before_change.start_position) /
        static_cast<double>(before_change.end_sample - before_change.start_sample);
    const double after_velocity =
        (after_change.end_position - after_change.start_position) /
        static_cast<double>(after_change.end_sample - after_change.start_sample);

    CHECK(before_change.end_sample - before_change.start_sample == 2000);
    CHECK(after_change.end_sample - after_change.start_sample == 1000);
    CHECK(after_change.end_position - after_change.start_position == doctest::Approx(0.5));
    CHECK(after_velocity == doctest::Approx(before_velocity));
}

TEST_CASE("reference BPM aggregates repeated running sections before sample rounding") {
    BmsChart chart;
    chart.base_bpm = 240.0;
    chart.commands = {{0, "11", "01"}, {1, "03", "78"}, {2, "03", "F0"},
                      {3, "02", "0.5"}, {3, "11", "01"}};
    // 240 BPM lasts 1 + 1.5 seconds; 120 BPM lasts one 2-second section.
    const auto normalized = BmsChartNormalizer{}.normalize(chart);
    REQUIRE(normalized.success());
    for (int sample_rate : {1000, 44100, 48000, 96000}) {
        const auto result = BmsTimelineBuilder{}.build(normalized.chart, sample_rate);
        REQUIRE(result.success());
        CHECK(result.timeline.reference_bpm == doctest::Approx(240.0));
        CHECK(result.timeline.duration_samples == static_cast<int64_t>(4.5 * sample_rate));
    }
}

TEST_CASE("reference BPM uses running seconds instead of beats or note counts") {
    BmsChart chart;
    chart.base_bpm = 60.0;
    chart.commands = {{0, "11", "01"}, {1, "03", "F0"},
                      {1, "11", "0101010101010101"},
                      {2, "11", "0101010101010101"},
                      {3, "11", "0101010101010101"}};
    const auto normalized = BmsChartNormalizer{}.normalize(chart);
    REQUIRE(normalized.success());
    const auto result = BmsTimelineBuilder{}.build(normalized.chart, 48000);
    REQUIRE(result.success());
    // Four beats at 60 BPM last longer than twelve dense beats at 240 BPM.
    CHECK(result.timeline.reference_bpm == doctest::Approx(60.0));
    CHECK(result.timeline.duration_samples == 7 * 48000);
}

TEST_CASE("reference BPM excludes STOP waits while retaining zero-scroll running time") {
    BmsChart chart;
    chart.base_bpm = 60.0;
    chart.stop["01"] = 4800.0;
    chart.scroll["01"] = 0.0;
    chart.commands = {{0, "02", "0.25"}, {0, "09", "01"},
                      {1, "03", "F0"}, {1, "SC", "01"}, {4, "11", "01"}};
    const auto normalized = BmsChartNormalizer{}.normalize(chart);
    REQUIRE(normalized.success());
    const auto result = BmsTimelineBuilder{}.build(normalized.chart, 1000);
    REQUIRE(result.success());
    // 60 BPM: 1 second running + 100 seconds STOP; 240 BPM: 4 seconds running.
    CHECK(result.timeline.reference_bpm == doctest::Approx(240.0));
    CHECK(result.timeline.duration_samples == 105000);
}

TEST_CASE("reference BPM ties use the first tempo that actually advances time") {
    BmsChart chart;
    chart.base_bpm = 60.0;
    chart.commands = {{0, "03", "F0"}, {2, "03", "78"}, {2, "11", "01"}};
    const auto normalized = BmsChartNormalizer{}.normalize(chart);
    REQUIRE(normalized.success());
    const auto result = BmsTimelineBuilder{}.build(normalized.chart, 1000);
    REQUIRE(result.success());
    // The unused 60 BPM header loses to the first actual tempo in a 2s/2s tie.
    CHECK(result.timeline.reference_bpm == doctest::Approx(240.0));
}

TEST_CASE("reference BPM includes the final measure and dedicated long-note tails") {
    BmsChart chart;
    chart.base_bpm = 120.0;
    chart.bpm["01"] = 180.5;
    chart.commands = {{1, "08", "01"}, {1, "51", "01"}, {4, "51", "01"}};
    const auto normalized = BmsChartNormalizer{}.normalize(chart);
    REQUIRE(normalized.success());
    const auto result = BmsTimelineBuilder{}.build(normalized.chart, 1000);
    REQUIRE(result.success());
    CHECK(result.timeline.reference_bpm == doctest::Approx(180.5));
    CHECK(result.timeline.duration_samples == std::llround((2.0 + 4.0 * 240.0 / 180.5) * 1000));
}

TEST_CASE("reference BPM has deterministic empty-chart and invalid-domain fallbacks") {
    tenriff::chart::BmsNormalizedChart chart;
    chart.base_bpm = 133.25;
    const auto empty = BmsTimelineBuilder{}.build(chart, 48000);
    REQUIRE(empty.success());
    CHECK(empty.timeline.reference_bpm == doctest::Approx(133.25));

    const auto invalid_rate = BmsTimelineBuilder{}.build(chart, 0);
    CHECK_FALSE(invalid_rate.success());
    CHECK(invalid_rate.timeline.reference_bpm == 0.0);

    chart.base_bpm = std::numeric_limits<double>::quiet_NaN();
    chart.measures.push_back({0.0, 1.0});
    const auto invalid_bpm = BmsTimelineBuilder{}.build(chart, 48000);
    CHECK_FALSE(invalid_bpm.success());
    CHECK(invalid_bpm.timeline.reference_bpm == 0.0);
}
