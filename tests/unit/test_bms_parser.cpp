#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "chart/BmsParser.h"

using tenriff::chart::BmsParseResult;
using tenriff::chart::BmsParser;
using tenriff::chart::BmsParserOptions;
using tenriff::chart::BmsParseSeverity;

static bool has_message(const BmsParseResult& result, BmsParseSeverity severity) {
    for (const auto& message : result.messages) {
        if (message.severity == severity) {
            return true;
        }
    }
    return false;
}

TEST_CASE("parses basic headers and dictionaries") {
    const char* data =
        "#TITLE Example Song\n"
        "#ARTIST Composer\n"
        "#BPM 180.5\n"
        "#WAV01 kick.wav\n"
        "#BPMAA 120\n"
        "#STOPBB 96\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.headers.at("TITLE"), "Example Song");
    CHECK_EQ(result.chart.headers.at("ARTIST"), "Composer");
    CHECK_EQ(result.chart.base_bpm, 180.5);
    CHECK_EQ(result.chart.wav.at("01"), "kick.wav");
    CHECK_EQ(result.chart.bpm.at("AA"), 120.0);
    CHECK_EQ(result.chart.stop.at("BB"), 96.0);
}

TEST_CASE("parses measure commands with even tokens") {
    const char* data =
        "#00111:0100\n"
        "#00202:1.5\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.commands.size(), 2u);
    CHECK_EQ(result.chart.commands[0].measure, 1);
    CHECK_EQ(result.chart.commands[0].channel, "11");
    CHECK_EQ(result.chart.commands[0].data, "0100");
    CHECK_EQ(result.chart.commands[1].channel, "02");
    auto lane = result.chart.lane_mapping.laneForChannel("21");
    CHECK(lane.has_value());
    CHECK_EQ(lane.value(), 6);
}

TEST_CASE("reports odd-length measure tokens as error in strict mode") {
    const char* data = "#00111:001";
    BmsParser parser;
    auto result = parser.parse(data);

    CHECK_FALSE(result.success());
    CHECK(has_message(result, BmsParseSeverity::Error));
    CHECK_EQ(result.chart.commands.size(), 0u);
}

TEST_CASE("tolerant mode downgrades odd-length tokens to warning") {
    const char* data = "#00111:001";
    BmsParser parser;
    BmsParserOptions options;
    options.tolerant = true;
    auto result = parser.parse(data, options);

    CHECK(result.success());
    CHECK(has_message(result, BmsParseSeverity::Warning));
    CHECK_EQ(result.chart.commands.size(), 1u);
}
