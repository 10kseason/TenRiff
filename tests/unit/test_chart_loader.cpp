#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "app/ChartLoader.h"
#include "gameplay/GameplayChart.h"

using tenriff::app::ChartLoader;
using tenriff::app::ChartLoadResult;
using tenriff::app::ChartFormat;

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

struct CurrentPathGuard {
    std::filesystem::path original = std::filesystem::current_path();

    ~CurrentPathGuard() {
        std::error_code ec;
        std::filesystem::current_path(original, ec);
    }
};

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_chart_loader_tests";
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


std::string chart_audio_path(const tenriff::gameplay::GameplayChart& chart, std::size_t asset_id) {
    const std::string* path = chart.audio_asset_path(asset_id);
    return path ? *path : std::string{};
}

std::string chart_visual_path(const tenriff::gameplay::GameplayChart& chart, std::size_t asset_id) {
    const std::string* path = chart.visual_asset_path(asset_id);
    return path ? *path : std::string{};
}

bool refers_to_same_existing_file(std::string_view actual, const std::filesystem::path& expected) {
    if (actual.empty()) {
        return false;
    }
    std::error_code ec;
    const bool equivalent = std::filesystem::equivalent(
        std::filesystem::u8path(actual), expected, ec);
    return !ec && equivalent;
}

}  // namespace

TEST_CASE("chart loader falls back to ogg when referenced wav is missing") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "test.bms";
    const auto ogg_path = temp.path / "sample.ogg";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Fallback Test\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00101:01\n";
    }

    {
        std::ofstream ogg_file(ogg_path, std::ios::binary);
        REQUIRE(ogg_file.good());
        ogg_file << "OggS";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id), ogg_path));
}

TEST_CASE("chart loader resolves audio asset paths to absolute paths for relative chart loads") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    CurrentPathGuard cwd_guard;

    const auto chart_path = temp.path / "test.bms";
    const auto ogg_path = temp.path / "sample.ogg";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Absolute Path Test\n"
                      "#BPM 120\n"
                      "#WAV01 sample.ogg\n"
                      "#00101:01\n";
    }

    {
        std::ofstream ogg_file(ogg_path, std::ios::binary);
        REQUIRE(ogg_file.good());
        ogg_file << "OggS";
    }

    const auto parent = temp.path.parent_path();
    std::filesystem::current_path(parent);
    const auto relative_chart_path = std::filesystem::relative(chart_path, parent);

    ChartLoader loader;
    ChartLoadResult result = loader.load(relative_chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(std::filesystem::path(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id)).is_absolute());
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) ==
          std::filesystem::weakly_canonical(ogg_path).u8string());
}

TEST_CASE("chart loader resolves CP932-encoded BMS audio references") {
#ifndef _WIN32
    return;
#else
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "cp932_audio.bms";
    const auto ogg_path = temp.path / std::filesystem::u8path(u8"音.ogg");
    const std::string cp932_bms =
        "#TITLE \x93\xFA\x96\x7B\x8C\xEA\x83\x5E\x83\x43\x83\x67\x83\x8B\n"
        "#BPM 120\n"
        "#WAV01 \x89\xB9.ogg\n"
        "#00101:01\n";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file.write(cp932_bms.data(), static_cast<std::streamsize>(cp932_bms.size()));
    }

    {
        std::ofstream ogg_file(ogg_path, std::ios::binary);
        REQUIRE(ogg_file.good());
        ogg_file << "OggS";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id), ogg_path));
#endif
}

TEST_CASE("chart loader prefers ogg over wav for BMS wav references") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "test.bms";
    const auto wav_path = temp.path / "sample.wav";
    const auto ogg_path = temp.path / "sample.ogg";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Ogg Preferred\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00101:01\n";
    }

    {
        std::ofstream wav_file(wav_path, std::ios::binary);
        REQUIRE(wav_file.good());
        wav_file << "RIFF";
    }

    {
        std::ofstream ogg_file(ogg_path, std::ios::binary);
        REQUIRE(ogg_file.good());
        ogg_file << "OggS";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id), ogg_path));
}

TEST_CASE("chart loader keeps wav when ogg fallback is unavailable") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "test.bms";
    const auto wav_path = temp.path / "sample.wav";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Wav Fallback\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00101:01\n";
    }

    {
        std::ofstream wav_file(wav_path, std::ios::binary);
        REQUIRE(wav_file.good());
        wav_file << "RIFF";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id), wav_path));
}

TEST_CASE("chart loader rejects unsupported osu chart files") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "blocked.osu";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "osu file format v14\n"
                      "[General]\n"
                      "AudioFilename:test.mp3\n"
                      "Mode:3\n"
                      "[Metadata]\n"
                      "Title:Blocked\n"
                      "[Difficulty]\n"
                      "CircleSize:10\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK_FALSE(result.success());
    CHECK(result.error == "Unsupported chart extension.");
    CHECK(result.format == ChartFormat::Unknown);
}

TEST_CASE("chart loader schedules BMS base and overlay BGA images") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "bga.bms";
    const auto base_path = temp.path / "base.png";
    const auto overlay_path = temp.path / "overlay.png";
    const auto poor_path = temp.path / "poor.png";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE BGA\n"
                      "#BPM 120\n"
                      "#BMP01 base.png\n"
                      "#BMP02 overlay.png\n"
                      "#BMP03 poor.png\n"
                      "#00104:01\n"
                      "#00107:0002\n"
                      "#00106:03\n";
    }
    {
        std::ofstream base_file(base_path, std::ios::binary);
        REQUIRE(base_file.good());
        base_file << "base";
    }
    {
        std::ofstream overlay_file(overlay_path, std::ios::binary);
        REQUIRE(overlay_file.good());
        overlay_file << "overlay";
    }
    {
        std::ofstream poor_file(poor_path, std::ios::binary);
        REQUIRE(poor_file.good());
        poor_file << "poor";
    }

    ChartLoader loader;
    const ChartLoadResult result =
        loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    REQUIRE(result.chart.visual_cues.size() == 2u);
    CHECK(result.chart.visual_cues[0].layer == tenriff::gameplay::VisualLayer::Base);
    CHECK(result.chart.visual_cues[1].layer == tenriff::gameplay::VisualLayer::Overlay);
    CHECK(result.chart.visual_cues[0].start_sample < result.chart.visual_cues[1].start_sample);
    CHECK(chart_visual_path(result.chart, result.chart.visual_cues[0].asset_id) ==
          std::filesystem::weakly_canonical(base_path).u8string());
    CHECK(chart_visual_path(result.chart, result.chart.visual_cues[1].asset_id) ==
          std::filesystem::weakly_canonical(overlay_path).u8string());
}

TEST_CASE("chart loader uses BMS stage image when timed BGA is absent") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "stage.bms";
    const auto stage_path = temp.path / "stage.jpg";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Static Background\n"
                      "#BPM 120\n"
                      "#STAGEFILE stage.jpg\n"
                      "#00111:01\n";
    }
    {
        std::ofstream image_file(stage_path, std::ios::binary);
        REQUIRE(image_file.good());
        image_file << "stage";
    }

    ChartLoader loader;
    const ChartLoadResult result =
        loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    REQUIRE(result.chart.visual_cues.size() == 1u);
    CHECK(result.chart.visual_cues.front().start_sample == 0);
    CHECK(chart_visual_path(result.chart, result.chart.visual_cues.front().asset_id) ==
          std::filesystem::weakly_canonical(stage_path).u8string());
}

TEST_CASE("chart loader accepts common BMS separator lines and header values containing colon") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "realworld_like.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "*---------------------- HEADER FIELD\n"
                      "#TITLE Example Song\n"
                      "#SUBARTIST obj:u_e\n"
                      "#BPM 120\n"
                      "*---------------------- MAIN DATA FIELD\n"
                      "#00111:01\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count >= 1);
    CHECK(result.chart.notes.size() == 1u);
}

TEST_CASE("chart loader compacts BMS lanes for explicit 4K, 6K, and 8K header charts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    ChartLoader loader;
    struct CaseData {
        const char* filename;
        const char* header;
        int lane_count;
        const char* body;
    };

    const CaseData cases[] = {
        {"fourk_header.bms", "#4K", 4, "#00111:01\n#00112:01\n#00114:01\n#00115:01\n"},
        {"sixk_header.bms", "#6K", 6, "#00111:01\n#00112:01\n#00113:01\n#00114:01\n#00115:01\n#00118:01\n"},
        {"eightk_header.bms", "#8K", 8,
         "#00111:01\n#00112:01\n#00113:01\n#00114:01\n#00115:01\n#00118:01\n#00119:01\n#00121:01\n"},
    };

    for (const auto& case_data : cases) {
        const auto chart_path = temp.path / case_data.filename;
        {
            std::ofstream chart_file(chart_path, std::ios::binary);
            REQUIRE(chart_file.good());
            chart_file << "#TITLE Explicit Key Header\n"
                          "#BPM 120\n"
                       << case_data.header << "\n"
                       << case_data.body;
        }

        ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

        CHECK(result.success());
        CHECK(result.format == ChartFormat::Bms);
        CHECK(result.chart.lane_count == case_data.lane_count);
        REQUIRE(result.chart.notes.size() == static_cast<std::size_t>(case_data.lane_count));
        for (int lane = 0; lane < case_data.lane_count; ++lane) {
            CHECK(result.chart.notes[static_cast<std::size_t>(lane)].lane == lane + 1);
        }
    }
}

TEST_CASE("chart loader compacts BMS lanes for explicit 5+1 and 7+1 SP charts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    ChartLoader loader;
    struct CaseData {
        const char* filename;
        const char* header;
        int lane_count;
        const char* body;
    };

    const CaseData cases[] = {
        {"five_plus_one_sp.bms", "#5K", 6,
         "#00111:01\n#00112:01\n#00113:01\n#00114:01\n#00115:01\n#00116:01\n"},
        {"seven_plus_one_sp.bms", "#7K", 8,
         "#00111:01\n#00112:01\n#00113:01\n#00114:01\n#00115:01\n#00116:01\n#00118:01\n#00119:01\n"},
    };

    for (const auto& case_data : cases) {
        const auto chart_path = temp.path / case_data.filename;
        {
            std::ofstream chart_file(chart_path, std::ios::binary);
            REQUIRE(chart_file.good());
            chart_file << "#TITLE SP Header\n"
                          "#BPM 120\n"
                       << case_data.header << "\n"
                       << case_data.body;
        }

        ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

        CHECK(result.success());
        CHECK(result.format == ChartFormat::Bms);
        CHECK(result.chart.lane_count == case_data.lane_count);
        REQUIRE(result.chart.notes.size() == static_cast<std::size_t>(case_data.lane_count));
        for (int lane = 0; lane < case_data.lane_count; ++lane) {
            CHECK(result.chart.notes[static_cast<std::size_t>(lane)].lane == lane + 1);
        }
        REQUIRE(result.chart.scratch_lanes.size() == 1u);
        CHECK(result.chart.scratch_lanes.front() == (case_data.lane_count == 6 ? 6 : 1));
    }
}

TEST_CASE("chart loader auto-detects headerless player-one 5+1 SP charts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "headerless_sp.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Headerless SP\n"
                      "#PLAYER 1\n"
                      "#BPM 120\n"
                      "#00111:01\n"
                      "#00112:01\n"
                      "#00113:01\n"
                      "#00114:01\n"
                      "#00115:01\n"
                      "#00116:01\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count == 6);
    REQUIRE(result.chart.notes.size() == 6u);
    for (int lane = 0; lane < 6; ++lane) {
        CHECK(result.chart.notes[static_cast<std::size_t>(lane)].lane == lane + 1);
    }
}

TEST_CASE("chart loader auto-detects headerless player-one 7+1 SP charts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "headerless_seven_sp.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Headerless Seven SP\n"
                      "#PLAYER 1\n"
                      "#BPM 120\n"
                      "#00111:01\n"
                      "#00112:01\n"
                      "#00113:01\n"
                      "#00114:01\n"
                      "#00115:01\n"
                      "#00116:01\n"
                      "#00118:01\n"
                      "#00119:01\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count == 8);
    REQUIRE(result.chart.scratch_lanes.size() == 1u);
    CHECK(result.chart.scratch_lanes.front() == 1);
    REQUIRE(result.chart.notes.size() == 8u);
    for (int lane = 0; lane < 8; ++lane) {
        CHECK(result.chart.notes[static_cast<std::size_t>(lane)].lane == lane + 1);
    }
}

TEST_CASE("chart loader maps 7+1 SP scratch to the first gameplay lane") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "seven_sp_scratch_first.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Seven SP Scratch First\n"
                      "#7K\n"
                      "#BPM 120\n"
                      "#00116:01\n"
                      "#00211:01\n"
                      "#00319:01\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count == 8);
    REQUIRE(result.chart.scratch_lanes.size() == 1u);
    CHECK(result.chart.scratch_lanes.front() == 1);
    REQUIRE(result.chart.notes.size() == 3u);
    CHECK(result.chart.notes[0].lane == 1);
    CHECK(result.chart.notes[1].lane == 2);
    CHECK(result.chart.notes[2].lane == 8);
}

TEST_CASE("chart loader maps pms extension charts to 9 lanes") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "native_popnine.pms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Native PMS\n"
                      "#BPM 120\n"
                      "#00111:01\n"
                      "#00115:01\n"
                      "#00122:01\n"
                      "#00125:01\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count == 9);
    REQUIRE(result.chart.notes.size() == 4u);
    CHECK(result.chart.notes[0].lane == 1);
    CHECK(result.chart.notes[1].lane == 5);
    CHECK(result.chart.notes[2].lane == 6);
    CHECK(result.chart.notes[3].lane == 9);
}

TEST_CASE("chart loader preserves 14+2 DP lane positions") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "dpa_layout.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE DPA Layout\n"
                      "#PLAYER 3\n"
                      "#BPM 120\n"
                      "#00111:01\n"
                      "#00116:01\n"
                      "#00119:01\n"
                      "#00121:01\n"
                      "#00126:01\n"
                      "#00129:01\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count == 16);
    REQUIRE((result.chart.scratch_lanes == std::vector<int>{6, 14}));
    CHECK(result.chart.lane_group_size == 8);

    REQUIRE(result.chart.notes.size() == 6u);
    CHECK(result.chart.notes[0].lane == 1);
    CHECK(result.chart.notes[1].lane == 6);
    CHECK(result.chart.notes[2].lane == 8);
    CHECK(result.chart.notes[3].lane == 9);
    CHECK(result.chart.notes[4].lane == 14);
    CHECK(result.chart.notes[5].lane == 16);
}

TEST_CASE("chart loader detects 10+2 DP and marks both scratch lanes") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "dp10_plus_two.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE 10 Plus 2 DP\n"
                      "#PLAYER 3\n"
                      "#BPM 120\n"
                      "#00111:01\n"
                      "#00112:01\n"
                      "#00113:01\n"
                      "#00114:01\n"
                      "#00115:01\n"
                      "#00116:01\n"
                      "#00121:01\n"
                      "#00122:01\n"
                      "#00123:01\n"
                      "#00124:01\n"
                      "#00125:01\n"
                      "#00126:01\n";
    }

    ChartLoader loader;
    const ChartLoadResult result =
        loader.load(chart_path.u8string(), 48000, 1.0, "ignore");
    CHECK(result.success());
    CHECK(result.format == ChartFormat::Bms);
    CHECK(result.chart.lane_count == 12);
    CHECK(result.chart.lane_group_size == 6);
    CHECK((result.chart.scratch_lanes == std::vector<int>{6, 12}));
    REQUIRE(result.chart.notes.size() == 12u);
    for (int lane = 0; lane < 12; ++lane) {
        CHECK(result.chart.notes[static_cast<std::size_t>(lane)].lane == lane + 1);
    }
}


TEST_CASE("chart loader builds hold notes from BMS long-note channels") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "long_channel.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Long Channel\n"
                      "#BPM 120\n"
                      "#00151:0101\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().lane == 1);
    CHECK(result.chart.notes.front().end_sample.has_value());
    CHECK_FALSE(result.chart.notes.front().release_required);
    CHECK(result.chart.notes.front().end_sample.value() > result.chart.notes.front().start_sample);
}

TEST_CASE("chart loader builds hold notes from BMS LNOBJ markers") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "lnobj.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE LNOBJ\n"
                      "#BPM 120\n"
                      "#LNOBJ AA\n"
                      "#00111:01AA\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().lane == 1);
    CHECK(result.chart.notes.front().end_sample.has_value());
    CHECK_FALSE(result.chart.notes.front().release_required);
    CHECK(result.chart.notes.front().end_sample.value() > result.chart.notes.front().start_sample);
}

TEST_CASE("chart loader builds cross-measure MGQ holds from BMS LNTYPE 2") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "lntype2.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE LNTYPE 2\n"
                      "#BPM 120\n"
                      "#LNTYPE 2\n"
                      "#00151:00111111\n"
                      "#00251:11110000\n";
    }

    ChartLoader loader;
    const ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    REQUIRE(result.success());
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().lane == 1);
    REQUIRE(result.chart.notes.front().end_sample.has_value());
    CHECK(result.chart.notes.front().end_sample.value() > result.chart.notes.front().start_sample);
    CHECK_FALSE(result.chart.notes.front().release_required);
}

TEST_CASE("chart loader enables release judgement for BMS LNMODE 2 long-note channels") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "charge_long_channel.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Charge Long Channel\n"
                      "#BPM 120\n"
                      "#LNMODE 2\n"
                      "#00151:0101\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().end_sample.has_value());
    CHECK(result.chart.notes.front().release_required);
}

TEST_CASE("chart loader enables release judgement for BMS LNMODE 2 LNOBJ holds") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "charge_lnobj.bms";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Charge LNOBJ\n"
                      "#BPM 120\n"
                      "#LNMODE 2\n"
                      "#LNOBJ AA\n"
                      "#00111:01AA\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().end_sample.has_value());
    CHECK(result.chart.notes.front().release_required);
}

TEST_CASE("chart loader attaches BMS note keysounds when follow policy is enabled") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "keysound_follow.bms";
    const auto wav_path = temp.path / "sample.wav";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Keysound Follow\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00111:01\n";
    }

    {
        std::ofstream wav_file(wav_path, std::ios::binary);
        REQUIRE(wav_file.good());
        wav_file << "RIFF";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "follow");

    CHECK(result.success());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.notes.front().audio_asset_id), wav_path));
    CHECK(result.chart.audio_cues.empty());
}

TEST_CASE("chart loader leaves BMS note keysounds unset when follow policy is ignored") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "keysound_ignore.bms";
    const auto wav_path = temp.path / "sample.wav";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Keysound Ignore\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00111:01\n";
    }

    {
        std::ofstream wav_file(wav_path, std::ios::binary);
        REQUIRE(wav_file.good());
        wav_file << "RIFF";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore");

    CHECK(result.success());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().audio_asset_id == tenriff::gameplay::kInvalidAudioAssetId);
    CHECK(result.chart.audio_cues.empty());
}

TEST_CASE("chart loader interns repeated BMS keysound assets once") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "keysound_intern.bms";
    const auto wav_path = temp.path / "sample.wav";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Keysound Intern\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00111:01\n"
                      "#00211:01\n";
    }

    {
        std::ofstream wav_file(wav_path, std::ios::binary);
        REQUIRE(wav_file.good());
        wav_file << "RIFF";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "follow");

    CHECK(result.success());
    REQUIRE(result.chart.notes.size() == 2u);
    REQUIRE(result.chart.audio_assets.size() == 1u);
    CHECK(result.chart.notes[0].audio_asset_id == result.chart.notes[1].audio_asset_id);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.notes[0].audio_asset_id), wav_path));
}

TEST_CASE("chart loader can autoplay BMS note keysounds as background cues") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "keysound_autoplay.bms";
    const auto wav_path = temp.path / "sample.wav";

    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "#TITLE Keysound Autoplay\n"
                      "#BPM 120\n"
                      "#WAV01 sample.wav\n"
                      "#00111:01\n";
    }

    {
        std::ofstream wav_file(wav_path, std::ios::binary);
        REQUIRE(wav_file.good());
        wav_file << "RIFF";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "autoplay");

    CHECK(result.success());
    REQUIRE(result.chart.notes.size() == 1u);
    CHECK(result.chart.notes.front().audio_asset_id == tenriff::gameplay::kInvalidAudioAssetId);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(refers_to_same_existing_file(
        chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id), wav_path));
    CHECK(result.chart.audio_cues.front().start_sample == result.chart.notes.front().start_sample);
}
