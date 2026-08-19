#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

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

bool is_windows_runtime_integration_test(std::string_view name) {
    // These tests own real RawInput windows or multi-peer localhost sockets.
    // They remain part of the normal Release suite, but MSVC ASan can prevent
    // their worker shutdown from completing. Keep the sanitizer exclusion
    // exact so new unit coverage cannot be skipped accidentally.
    static constexpr std::array<std::string_view, 8> kTests = {
        "raw input thread can stop and restart without silently failing",
        "second live RawInput owner is rejected until the first owner stops",
        "RawInput thread falls back in place when its message pump exits",
        "RawInput window close switches to Polling without key activity",
        "RawInput target replacement switches to Polling without key activity",
        "peer session localhost round reaches final score and clean shutdown",
        "peer room coordinates four players and rotates leader in join order",
        "peer room accepts eight players and rejects a ninth",
    };
    return std::find(kTests.begin(), kTests.end(), name) != kTests.end();
}

bool is_nk3_openvino_integration_test(std::string_view name) {
    // OpenVINO inference is covered by the dedicated non-ASan CPU smoke job.
    // Keep this list exact so the sanitizer suite still runs every pure host test.
    static constexpr std::array<std::string_view, 2> kTests = {
        "runtime mode settings use the selected KeyWeaver NK3 converter",
        "runtime NK3 remasters a chart even when the target key count is unchanged",
    };
    return std::find(kTests.begin(), kTests.end(), name) != kTests.end();
}

}  // namespace

int main(int argc, char** argv) {
    // Keep CTest diagnostics useful if a sanitizer terminates a long-running
    // shard before the process gets a chance to flush its normal stream buffer.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int shard_index = 0;
    int shard_count = 1;
    bool skip_windows_runtime_integration = false;
    bool skip_nk3_openvino_integration = false;
    constexpr std::string_view kShardIndexPrefix = "--shard-index=";
    constexpr std::string_view kShardCountPrefix = "--shard-count=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        try {
            if (argument.rfind(kShardIndexPrefix, 0) == 0) {
                shard_index = std::stoi(std::string(argument.substr(kShardIndexPrefix.size())));
            } else if (argument.rfind(kShardCountPrefix, 0) == 0) {
                shard_count = std::stoi(std::string(argument.substr(kShardCountPrefix.size())));
            } else if (argument == "--skip-windows-runtime-integration") {
                skip_windows_runtime_integration = true;
            } else if (argument == "--skip-nk3-openvino-integration") {
                skip_nk3_openvino_integration = true;
            }
        } catch (...) {
            std::cerr << "Invalid test shard argument: " << argument << '\n';
            return 2;
        }
    }
    if (shard_count <= 0 || shard_index < 0 || shard_index >= shard_count) {
        std::cerr << "Invalid test shard " << shard_index << '/' << shard_count << '\n';
        return 2;
    }

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif

    int failed = 0;
    std::size_t registry_index = 0;
    std::size_t selected_tests = 0;
    std::size_t skipped_tests = 0;
    for (const auto& test : ::doctest::registry()) {
        const bool selected =
            registry_index % static_cast<std::size_t>(shard_count) ==
            static_cast<std::size_t>(shard_index);
        ++registry_index;
        if (!selected) {
            continue;
        }
        ++selected_tests;
        if (skip_windows_runtime_integration && is_windows_runtime_integration_test(test.name)) {
            ++skipped_tests;
            std::cout << "[skip] " << test.name
                      << " - covered by the normal Release OS-integration suite\n";
            continue;
        }
        if (skip_nk3_openvino_integration && is_nk3_openvino_integration_test(test.name)) {
            ++skipped_tests;
            std::cout << "[skip] " << test.name
                      << " - covered by the non-ASan OpenVINO CPU smoke job\n";
            continue;
        }
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

    if (selected_tests == 0) {
        std::cerr << "Test shard selected no tests" << '\n';
        return 2;
    }

    std::cout << "All tests passed (shard " << shard_index + 1 << '/' << shard_count
              << ", " << (selected_tests - skipped_tests) << " run, "
              << skipped_tests << " integration skips)" << '\n';
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

TEST_CASE("parses BMS scroll extensions and landmine channels") {
    const char* data =
        "#BPM 120\n"
        "#WAV00 mine.wav\n"
        "#SCROLL01 -1.5\n"
        "#000SC:01\n"
        "#000D1:0A\n"
        "#000E1:ZZ\n";

    BmsParser parser;
    const auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.wav.at("00"), "mine.wav");
    CHECK(result.chart.scroll.at("01") == doctest::Approx(-1.5));
    REQUIRE(result.chart.commands.size() == 3u);
    CHECK_EQ(result.chart.commands[0].channel, "SC");
    CHECK_EQ(result.chart.commands[1].channel, "D1");
    CHECK_EQ(result.chart.commands[2].channel, "E1");
}

TEST_CASE("parses lowercase BPM headers and extended BPM dictionaries case insensitively") {
    const char* data =
        "#title Lowercase Timing\n"
        "#bpm 120\n"
        "#bpmaa 187.5\n"
        "#00103:7800\n"
        "#00208:aa00\n";

    BmsParser parser;
    const auto result = parser.parse(data);

    REQUIRE(result.success());
    CHECK(result.chart.base_bpm == doctest::Approx(120.0));
    CHECK(result.chart.bpm.at("AA") == doctest::Approx(187.5));
    REQUIRE(result.chart.commands.size() == 2u);
    CHECK(result.chart.commands[0].channel == "03");
    CHECK(result.chart.commands[1].channel == "08");
}

TEST_CASE("BMS RANDOM IF ELSEIF and ELSE select one deterministic branch") {
    const char* data =
        "#RANDOM 3\n"
        "#IF 1\n"
        "#00111:01\n"
        "#ELSEIF 2\n"
        "#00112:02\n"
        "#ELSE\n"
        "#00113:03\n"
        "#ENDIF\n"
        "#ENDRANDOM\n";

    BmsParserOptions options;
    options.random_seed = 42;
    BmsParser parser;
    const auto first = parser.parse(data, options);
    const auto second = parser.parse(data, options);

    REQUIRE(first.success());
    REQUIRE(second.success());
    REQUIRE(first.chart.commands.size() == 1u);
    REQUIRE(second.chart.commands.size() == 1u);
    CHECK(first.chart.commands.front().channel == second.chart.commands.front().channel);
    CHECK(first.chart.commands.front().data == second.chart.commands.front().data);
}

TEST_CASE("BMS SETRANDOM and SETSWITCH support exact branches skip and default") {
    const char* data =
        "#SETRANDOM 2\n"
        "#IF 1\n"
        "#TITLE Wrong\n"
        "#ELSEIF 2\n"
        "#TITLE Selected\n"
        "#ENDIF\n"
        "#ENDRANDOM\n"
        "#SETSWITCH 2\n"
        "#CASE 1\n"
        "#00111:01\n"
        "#SKIP\n"
        "#CASE 2\n"
        "#00112:02\n"
        "#SKIP\n"
        "#DEF\n"
        "#00113:03\n"
        "#ENDSW\n";

    BmsParser parser;
    const auto result = parser.parse(data);

    REQUIRE(result.success());
    CHECK(result.chart.headers.at("TITLE") == "Selected");
    REQUIRE(result.chart.commands.size() == 1u);
    CHECK(result.chart.commands.front().channel == "12");
    CHECK(result.chart.commands.front().data == "02");
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

TEST_CASE("index parser mode skips heavy asset maps and nonessential commands") {
    const char* data =
        "#TITLE Index Mode\n"
        "#ARTIST Composer\n"
        "#SUBTITLE Another\n"
        "#DIFFICULTY 4\n"
        "#LNOBJ AA\n"
        "#SUBARTIST Guest\n"
        "#WAV01 kick.wav\n"
        "#BMP01 stage.png\n"
        "#00101:01\n"
        "#00104:01\n"
        "#00111:01\n"
        "#00103:0A\n"
        "#00108:AA\n"
        "#00109:BB\n";

    BmsParser parser;
    BmsParserOptions options;
    options.retain_wav_bmp = false;
    options.retain_unknown_headers = false;
    options.retain_nonessential_commands = false;
    auto result = parser.parse(data, options);

    CHECK(result.success());
    CHECK_EQ(result.chart.headers.at("TITLE"), "Index Mode");
    CHECK_EQ(result.chart.headers.at("ARTIST"), "Composer");
    CHECK_EQ(result.chart.headers.at("SUBTITLE"), "Another");
    CHECK_EQ(result.chart.headers.at("DIFFICULTY"), "4");
    CHECK_EQ(result.chart.headers.at("LNOBJ"), "AA");
    CHECK(result.chart.headers.count("SUBARTIST") == 0u);
    CHECK(result.chart.wav.empty());
    CHECK(result.chart.bmp.empty());
    REQUIRE(result.chart.commands.size() == 5u);
    CHECK_EQ(result.chart.commands[0].channel, "04");
    CHECK_EQ(result.chart.commands[1].channel, "11");
    CHECK_EQ(result.chart.commands[2].channel, "03");
    CHECK_EQ(result.chart.commands[3].channel, "08");
    CHECK_EQ(result.chart.commands[4].channel, "09");
}

TEST_CASE("compact lane mapping follows explicit BMS key headers") {
    const char* data =
        "#TITLE Four Key Example\n"
        "#4K\n"
        "#00111:01\n"
        "#00112:01\n"
        "#00114:01\n"
        "#00115:01\n"
        "#00154:0101\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 4);
    CHECK(result.chart.headers.count("4K") == 1u);

    auto lane11 = result.chart.lane_mapping.laneForChannel("11");
    auto lane12 = result.chart.lane_mapping.laneForChannel("12");
    auto lane14 = result.chart.lane_mapping.laneForChannel("14");
    auto lane15 = result.chart.lane_mapping.laneForChannel("15");
    auto lane54 = result.chart.lane_mapping.laneForChannel("54");

    REQUIRE(lane11.has_value());
    REQUIRE(lane12.has_value());
    REQUIRE(lane14.has_value());
    REQUIRE(lane15.has_value());
    REQUIRE(lane54.has_value());
    CHECK_EQ(lane11.value(), 1);
    CHECK_EQ(lane12.value(), 2);
    CHECK_EQ(lane14.value(), 3);
    CHECK_EQ(lane15.value(), 4);
    CHECK_EQ(lane54.value(), 3);
}

TEST_CASE("parser detects explicit 5+1 SP layout and stores label") {
    const char* data =
        "#TITLE Five Plus Scratch\n"
        "#5K\n"
        "#00111:01\n"
        "#00112:01\n"
        "#00113:01\n"
        "#00114:01\n"
        "#00115:01\n"
        "#00116:01\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 6);
    CHECK_EQ(result.chart.layout_label, "5+1 SP");
    for (int lane = 1; lane <= 6; ++lane) {
        const std::string channel = (lane < 6) ? ("1" + std::to_string(lane)) : "16";
        auto mapped_lane = result.chart.lane_mapping.laneForChannel(channel);
        REQUIRE(mapped_lane.has_value());
        CHECK_EQ(mapped_lane.value(), lane);
    }
}

TEST_CASE("parser detects explicit 7+1 SP layout and stores label") {
    const char* data =
        "#TITLE Seven Plus Scratch\n"
        "#7K\n"
        "#00111:01\n"
        "#00112:01\n"
        "#00113:01\n"
        "#00114:01\n"
        "#00115:01\n"
        "#00116:01\n"
        "#00118:01\n"
        "#00119:01\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 8);
    CHECK_EQ(result.chart.layout_label, "7+1 SP");
    for (const std::pair<const char*, int> lane_case : {std::pair{"16", 1}, {"11", 2}, {"12", 3}, {"13", 4},
                                                         {"14", 5}, {"15", 6}, {"18", 7}, {"19", 8}}) {
        auto mapped_lane = result.chart.lane_mapping.laneForChannel(lane_case.first);
        REQUIRE(mapped_lane.has_value());
        CHECK_EQ(mapped_lane.value(), lane_case.second);
    }
}

TEST_CASE("parser auto-detects headerless player-one 5+1 SP layout") {
    const char* data =
        "#TITLE Headerless SP\n"
        "#PLAYER 1\n"
        "#00111:01\n"
        "#00112:01\n"
        "#00113:01\n"
        "#00114:01\n"
        "#00115:01\n"
        "#00116:01\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 6);
    CHECK_EQ(result.chart.layout_label, "5+1 SP");
}

TEST_CASE("parser auto-detects headerless player-one 7+1 SP layout") {
    const char* data =
        "#TITLE Headerless Seven SP\n"
        "#PLAYER 1\n"
        "#00111:01\n"
        "#00112:01\n"
        "#00113:01\n"
        "#00114:01\n"
        "#00115:01\n"
        "#00116:01\n"
        "#00118:01\n"
        "#00119:01\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 8);
    CHECK_EQ(result.chart.layout_label, "7+1 SP");
    for (const std::pair<const char*, int> lane_case : {std::pair{"16", 1}, {"11", 2}, {"12", 3}, {"13", 4},
                                                         {"14", 5}, {"15", 6}, {"18", 7}, {"19", 8}}) {
        auto mapped_lane = result.chart.lane_mapping.laneForChannel(lane_case.first);
        REQUIRE(mapped_lane.has_value());
        CHECK_EQ(mapped_lane.value(), lane_case.second);
    }
}

TEST_CASE("parser auto-detects headerless player-three 14+2 DP layout") {
    const char* data =
        "#TITLE Headerless DP\n"
        "#PLAYER 3\n"
        "#00111:01\n"
        "#00116:01\n"
        "#00119:01\n"
        "#00121:01\n"
        "#00126:01\n"
        "#00129:01\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 16);
    CHECK_EQ(result.chart.layout_label, "14+2 DP");
    for (const std::pair<const char*, int> lane_case : {std::pair{"11", 1}, {"16", 6}, {"19", 8},
                                                         {"21", 9}, {"26", 14}, {"29", 16}}) {
        auto mapped_lane = result.chart.lane_mapping.laneForChannel(lane_case.first);
        REQUIRE(mapped_lane.has_value());
        CHECK_EQ(mapped_lane.value(), lane_case.second);
    }
}

TEST_CASE("parser infers sparse standard SP charts without falling back to 10K") {
    const char* data =
        "#TITLE Sparse Standard SP\n"
        "#00111:01\n"
        "#00114:01\n"
        "#00119:01\n";

    BmsParser parser;
    auto result = parser.parse(data);

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 8);
    CHECK_EQ(result.chart.layout_label, "7+1 SP");
    auto lane11 = result.chart.lane_mapping.laneForChannel("11");
    auto lane14 = result.chart.lane_mapping.laneForChannel("14");
    auto lane19 = result.chart.lane_mapping.laneForChannel("19");
    REQUIRE(lane11.has_value());
    REQUIRE(lane14.has_value());
    REQUIRE(lane19.has_value());
    CHECK_EQ(lane11.value(), 2);
    CHECK_EQ(lane14.value(), 5);
    CHECK_EQ(lane19.value(), 8);
}

TEST_CASE("parseFile infers PMS 9K layout from pms extension") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "popnine.pms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE PMS Nine\n"
                      "#00111:01\n"
                      "#00115:01\n"
                      "#00122:01\n"
                      "#00125:01\n";
    }

    BmsParser parser;
    auto result = parser.parseFile(chart_path.u8string());

    CHECK(result.success());
    CHECK_EQ(result.chart.declared_key_count, 9);
    CHECK_EQ(result.chart.layout_label, "PMS 9K");
    for (const std::pair<const char*, int> lane_case : {std::pair{"11", 1}, {"15", 5}, {"22", 6}, {"25", 9}}) {
        auto mapped_lane = result.chart.lane_mapping.laneForChannel(lane_case.first);
        REQUIRE(mapped_lane.has_value());
        CHECK_EQ(mapped_lane.value(), lane_case.second);
    }
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
