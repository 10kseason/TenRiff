#include "doctest/doctest.h"

#include <cmath>

#include "chart/BmsChartNorm.h"

using tenriff::chart::BmsChart;
using tenriff::chart::BmsChartNormalizer;
using tenriff::chart::BmsNormalizedEventType;
using tenriff::chart::BmsNormalizationResult;

TEST_CASE("normalizes measures and note events with scaling") {
    BmsChart chart;
    chart.base_bpm = 180.0;
    chart.bpm["AA"] = 150.0;
    chart.stop["ZZ"] = 96.0;

    chart.commands.push_back({0, "02", "1.5"});
    chart.commands.push_back({0, "11", "0100"});
    chart.commands.push_back({0, "08", "AA00"});
    chart.commands.push_back({1, "02", "0.5"});
    chart.commands.push_back({1, "09", "ZZ00"});
    chart.commands.push_back({1, "21", "0001"});

    BmsChartNormalizer normalizer;
    BmsNormalizationResult result = normalizer.normalize(chart);

    CHECK(result.success());
    CHECK(std::abs(result.chart.base_bpm - chart.base_bpm) < 1e-6);
    CHECK(result.chart.measures.size() == 2u);
    CHECK(std::abs(result.chart.measures[0].start - 0.0) < 1e-6);
    CHECK(std::abs(result.chart.measures[0].length - 1.5) < 1e-6);
    CHECK(std::abs(result.chart.measures[1].start - 1.5) < 1e-6);
    CHECK(std::abs(result.chart.measures[1].length - 0.5) < 1e-6);

    CHECK(result.chart.events.size() == 4u);

    const auto& bpm_event = result.chart.events[0];
    CHECK(bpm_event.type == BmsNormalizedEventType::BpmChange);
    CHECK(bpm_event.value.has_value());
    CHECK(std::abs(bpm_event.value.value() - 150.0) < 1e-6);
    CHECK(std::abs(bpm_event.position - 0.0) < 1e-6);

    const auto& note_event = result.chart.events[1];
    CHECK(note_event.type == BmsNormalizedEventType::Note);
    CHECK(note_event.lane.has_value());
    if (note_event.lane.has_value()) {
        CHECK(note_event.lane.value() == 1u);
    }
    CHECK(std::abs(note_event.position - 0.0) < 1e-6);

    const auto& stop_event = result.chart.events[2];
    CHECK(stop_event.type == BmsNormalizedEventType::Stop);
    CHECK(stop_event.value.has_value());
    CHECK(std::abs(stop_event.value.value() - 96.0) < 1e-6);
    CHECK(std::abs(stop_event.position - 1.5) < 1e-6);

    const auto& p2_note_event = result.chart.events[3];
    CHECK(p2_note_event.type == BmsNormalizedEventType::Note);
    CHECK(p2_note_event.lane.has_value());
    if (p2_note_event.lane.has_value()) {
        CHECK(p2_note_event.lane.value() == 6u);
    }
    CHECK(std::abs(p2_note_event.position - 1.75) < 1e-6);
}

TEST_CASE("supports fractional measure lengths") {
    BmsChart chart;
    chart.commands.push_back({0, "02", "3/4"});
    chart.commands.push_back({0, "11", "0100"});

    BmsChartNormalizer normalizer;
    auto result = normalizer.normalize(chart);

    CHECK(result.success());
    CHECK(result.chart.measures.size() == 1u);
    CHECK(std::abs(result.chart.measures[0].length - 0.75) < 1e-6);
    CHECK(result.chart.events.size() == 1u);
    const auto& note_event = result.chart.events[0];
    CHECK(note_event.type == BmsNormalizedEventType::Note);
    CHECK(note_event.lane.has_value());
    CHECK(std::abs(note_event.position - 0.0) < 1e-6);
}

TEST_CASE("reports normalization errors for invalid measure length") {
    BmsChart chart;
    chart.commands.push_back({0, "02", "abc"});
    chart.commands.push_back({0, "11", "01"});

    BmsChartNormalizer normalizer;
    auto result = normalizer.normalize(chart);

    CHECK_FALSE(result.success());
    CHECK(!result.messages.empty());
    bool found_error = false;
    for (const auto& message : result.messages) {
        if (message.severity == tenriff::chart::BmsParseSeverity::Error) {
            found_error = true;
        }
    }
    CHECK(found_error);
}

TEST_CASE("reports normalization errors for zero measure denominator") {
    BmsChart chart;
    chart.commands.push_back({0, "02", "4/0"});

    BmsChartNormalizer normalizer;
    auto result = normalizer.normalize(chart);

    CHECK_FALSE(result.success());
    bool found_zero_denominator_error = false;
    for (const auto& message : result.messages) {
        if (message.severity == tenriff::chart::BmsParseSeverity::Error &&
            message.text.find("denominator") != std::string::npos) {
            found_zero_denominator_error = true;
        }
    }
    CHECK(found_zero_denominator_error);
}

