#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "app/ChartLoader.h"

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
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == ogg_path.u8string());
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
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == ogg_path.u8string());
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
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == ogg_path.u8string());
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
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == wav_path.u8string());
}

TEST_CASE("chart loader rejects osu charts in the current build") {
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
    CHECK(result.error == "osu!mania charts are disabled in this build.");
    CHECK(result.format == ChartFormat::Unknown);
}

TEST_CASE("chart loader loads osu!mania charts when the option is enabled") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "enabled.osu";
    const auto audio_path = temp.path / "music.ogg";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "osu file format v14\n"
                      "[General]\n"
                      "AudioFilename:music.ogg\n"
                      "Mode:3\n"
                      "[Metadata]\n"
                      "Title:Enabled\n"
                      "Artist:Composer\n"
                      "[Difficulty]\n"
                      "CircleSize:10\n"
                      "[TimingPoints]\n"
                      "0,500,4,0,0,100,1,0\n"
                      "[HitObjects]\n"
                      "0,0,0,1,0,0:0:0:0:\n"
                      "511,0,500,128,0,1000:0:0:0:\n";
    }
    {
        std::ofstream audio_file(audio_path, std::ios::binary);
        REQUIRE(audio_file.good());
        audio_file << "OggS";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore", true);

    CHECK(result.success());
    CHECK(result.format == ChartFormat::OsuMania);
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 2u);
    CHECK(result.chart.lane_count == 10);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == audio_path.u8string());
}

TEST_CASE("chart loader falls back to osu sibling ogg when AudioFilename is missing") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "fallback_audio.osu";
    const auto audio_path = temp.path / "fallback_audio.ogg";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "osu file format v14\n"
                      "[General]\n"
                      "Mode:3\n"
                      "[Metadata]\n"
                      "Title:Fallback Audio\n"
                      "[Difficulty]\n"
                      "CircleSize:4\n"
                      "[TimingPoints]\n"
                      "0,500,4,0,0,100,1,0\n"
                      "[HitObjects]\n"
                      "0,0,0,1,0,0:0:0:0:\n";
    }
    {
        std::ofstream audio_file(audio_path, std::ios::binary);
        REQUIRE(audio_file.good());
        audio_file << "OggS";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore", true);

    CHECK(result.success());
    CHECK(result.format == ChartFormat::OsuMania);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == audio_path.u8string());
}

TEST_CASE("chart loader falls back to the only osu audio file when AudioFilename is missing") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "single_audio.osu";
    const auto audio_path = temp.path / "music.wav";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "osu file format v14\n"
                      "[General]\n"
                      "Mode:3\n"
                      "[Metadata]\n"
                      "Title:Single Audio\n"
                      "[Difficulty]\n"
                      "CircleSize:4\n"
                      "[TimingPoints]\n"
                      "0,500,4,0,0,100,1,0\n"
                      "[HitObjects]\n"
                      "0,0,0,1,0,0:0:0:0:\n";
    }
    {
        std::ofstream audio_file(audio_path, std::ios::binary);
        REQUIRE(audio_file.good());
        audio_file << "RIFF";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore", true);

    CHECK(result.success());
    CHECK(result.format == ChartFormat::OsuMania);
    REQUIRE(result.chart.audio_cues.size() == 1u);
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == audio_path.u8string());
}

TEST_CASE("chart loader keeps non-10K osu!mania lane counts when enabled") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "fourk.osu";
    {
        std::ofstream chart_file(chart_path, std::ios::binary);
        REQUIRE(chart_file.good());
        chart_file << "osu file format v14\n"
                      "[General]\n"
                      "Mode:3\n"
                      "[Metadata]\n"
                      "Title:Four Key\n"
                      "[Difficulty]\n"
                      "CircleSize:4\n"
                      "[TimingPoints]\n"
                      "0,500,4,0,0,100,1,0\n"
                      "[HitObjects]\n"
                      "0,0,0,1,0,0:0:0:0:\n"
                      "511,0,500,1,0,0:0:0:0:\n";
    }

    ChartLoader loader;
    ChartLoadResult result = loader.load(chart_path.u8string(), 48000, 1.0, "ignore", true);

    CHECK(result.success());
    CHECK(result.format == ChartFormat::OsuMania);
    CHECK(result.messages.empty());
    REQUIRE(result.chart.notes.size() == 2u);
    CHECK(result.chart.lane_count == 4);
    CHECK(result.chart.notes[0].lane == 1);
    CHECK(result.chart.notes[1].lane == 4);
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
    CHECK(result.chart.notes.front().end_sample.value() > result.chart.notes.front().start_sample);
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
    CHECK(chart_audio_path(result.chart, result.chart.notes.front().audio_asset_id) == wav_path.u8string());
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
    CHECK(chart_audio_path(result.chart, result.chart.notes[0].audio_asset_id) == wav_path.u8string());
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
    CHECK(chart_audio_path(result.chart, result.chart.audio_cues.front().asset_id) == wav_path.u8string());
    CHECK(result.chart.audio_cues.front().start_sample == result.chart.notes.front().start_sample);
}
