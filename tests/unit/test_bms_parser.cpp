#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "chart/BmsParser.h"

using tenriff::chart::BmsParseResult;
using tenriff::chart::BmsParser;
using tenriff::chart::BmsParserOptions;
using tenriff::chart::BmsParseSeverity;

namespace {

struct TempDirGuard {
    std::filesystem::path path;

    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_bms_parser_tests";
    std::filesystem::create_directories(base);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto candidate = base / ("case_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

static bool has_message(const BmsParseResult& result, BmsParseSeverity severity) {
    for (const auto& message : result.messages) {
        if (message.severity == severity) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif

    int failed = 0;
    for (const auto& test : ::doctest::registry()) {
        try {
            test.func();
            std::cout << "[pass] " << test.name << '\n';
        } catch (const ::doctest::TestFailure& failure) {
            ++failed;
            std::cerr << "[fail] " << test.name << " - " << failure.what() << '\n';
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[fail] " << test.name << " - unexpected exception: " << ex.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[fail] " << test.name << " - unknown exception" << '\n';
        }
    }
    if (failed != 0) {
        std::cerr << failed << " test(s) failed" << '\n';
        return 1;
    }

    std::cout << "All tests passed" << '\n';
    return 0;
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

TEST_CASE("ignores common BMS section separators and keeps header values with colon") {
    const char* data =
        "*---------------------- HEADER FIELD\n"
        "#TITLE Example Song\n"
        "#SUBARTIST obj:u_e\n"
        "*---------------------- MAIN DATA FIELD\n"
        "#00111:0100\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_FALSE(has_message(result, BmsParseSeverity::Warning));
    CHECK_EQ(result.chart.headers.at("SUBARTIST"), "obj:u_e");
    CHECK_EQ(result.chart.commands.size(), 1u);
}

TEST_CASE("accepts colon-delimited header assignments") {
    const char* data =
        "#TITLE: Example Song\n"
        "#WAV01: kick.wav\n"
        "#BPMAA: 120\n"
        "#STOPBB: 96\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.headers.at("TITLE"), "Example Song");
    CHECK_EQ(result.chart.wav.at("01"), "kick.wav");
    CHECK_EQ(result.chart.bpm.at("AA"), 120.0);
    CHECK_EQ(result.chart.stop.at("BB"), 96.0);
}

TEST_CASE("parseFile decodes CP932 BMS headers and asset references to UTF-8") {
#ifndef _WIN32
    return;
#else
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "cp932_headers.bms";
    const std::string cp932_bms =
        "#TITLE \x93\xFA\x96\x7B\x8C\xEA\x83\x5E\x83\x43\x83\x67\x83\x8B\n"
        "#ARTIST \x8D\xEC\x8B\xC8\x8E\xD2\n"
        "#WAV01 \x89\xB9.ogg\n"
        "#00101:01\n";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file.write(cp932_bms.data(), static_cast<std::streamsize>(cp932_bms.size()));
    }

    BmsParser parser;
    auto result = parser.parseFile(chart_path.u8string());

    CHECK(result.success());
    CHECK_EQ(result.chart.headers.at("TITLE"), std::string(u8"日本語タイトル"));
    CHECK_EQ(result.chart.headers.at("ARTIST"), std::string(u8"作曲者"));
    CHECK_EQ(result.chart.wav.at("01"), std::string(u8"音.ogg"));
#endif
}
