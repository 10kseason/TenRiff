#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <tuple>
#include <unordered_map>
#include <string>
#include <vector>

#include "app/SongIndex.h"
#include "app/SongIndexBudget.h"

using tenriff::app::SongIndex;
using tenriff::app::SongIndexLoadResult;
using tenriff::app::SongIndexOptions;
using tenriff::app::SongIndexProgress;
using tenriff::app::SongIndexProgressStage;
using tenriff::app::legacy_song_index_cache_path_for_source;
using tenriff::app::load_song_index;
using tenriff::app::save_song_index;
using tenriff::app::scan_songs;
using tenriff::app::song_index_cache_path_for_source;

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
    const auto base = std::filesystem::temp_directory_path() / "tenriff_song_index_tests";
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

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << content;
}

bool path_has_prefix(const std::filesystem::path& path, const std::filesystem::path& prefix) {
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();
    for (; prefix_it != prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == path.end() || *path_it != *prefix_it) {
            return false;
        }
    }
    return true;
}

std::string minimal_bms_chart(const std::string& title, int playlevel) {
    return "#TITLE " + title + "\n"
           "#ARTIST Composer\n"
           "#PLAYLEVEL " + std::to_string(playlevel) + "\n"
           "#BPM 150\n"
           "#WAV01 sample.wav\n"
           "#00111:01\n";
}

std::string sparse_bms_chart(const std::string& title, int playlevel) {
    return "#TITLE " + title + "\n"
           "#ARTIST Composer\n"
           "#PLAYLEVEL " + std::to_string(playlevel) + "\n"
           "#BPM 150\n"
           "#00111:01\n"
           "#00211:01\n"
           "#00311:01\n"
           "#00411:01\n";
}

std::string dense_bms_chart(const std::string& title, int playlevel) {
    return "#TITLE " + title + "\n"
           "#ARTIST Composer\n"
           "#PLAYLEVEL " + std::to_string(playlevel) + "\n"
           "#BPM 150\n"
           "#00111:0101010101010101\n"
           "#00112:0001000100010001\n"
           "#00211:0101010101010101\n"
           "#00212:0001000100010001\n";
}

}  // namespace

TEST_CASE("song scan only exposes BMS-family charts in the menu index") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "legacy.bms", minimal_bms_chart("Legacy", 12));
    write_file(temp.path / "another.bme", minimal_bms_chart("Another", 15));
    write_file(temp.path / "ignored.osu",
               "osu file format v14\n[General]\nAudioFilename:test.mp3\nMode:3\n[Metadata]\nTitle:Ignored\n");

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 2u);
    CHECK(warnings.empty());
    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : index.entries) {
        by_path.emplace(entry.path, entry);
        CHECK(entry.format == "bms");
        CHECK(entry.key_count == 10);
        CHECK(entry.rating >= 0.0);
        CHECK(entry.level >= 0);
    }
    REQUIRE(by_path.count("legacy.bms") == 1u);
    REQUIRE(by_path.count("another.bme") == 1u);
    CHECK(by_path.at("legacy.bms").title == "Legacy");
    CHECK(by_path.at("another.bme").title == "Another");
}

TEST_CASE("song scan exposes 4K through 10K osu!mania charts when enabled") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "four.osu",
               "osu file format v14\n"
               "[General]\n"
               "AudioFilename:test.mp3\n"
               "Mode:3\n"
               "[Metadata]\n"
               "Title:Four\n"
               "Artist:Composer\n"
               "[Difficulty]\n"
               "CircleSize:4\n"
               "[TimingPoints]\n"
               "0,500,4,0,0,100,1,0\n"
               "[HitObjects]\n"
               "0,0,0,1,0,0:0:0:0:\n"
               "511,0,500,1,0,0:0:0:0:\n");
    write_file(temp.path / "ten.osu",
               "osu file format v14\n"
               "[General]\n"
               "AudioFilename:test.mp3\n"
               "Mode:3\n"
               "[Metadata]\n"
               "Title:Ten\n"
               "Artist:Composer\n"
               "[Difficulty]\n"
               "CircleSize:10\n"
               "[TimingPoints]\n"
               "0,500,4,0,0,100,1,0\n"
               "[HitObjects]\n"
               "0,0,0,1,0,0:0:0:0:\n"
               "256,0,500,1,0,0:0:0:0:\n");
    write_file(temp.path / "legacy.bms", minimal_bms_chart("Legacy", 12));

    SongIndexOptions options;
    options.include_osu = true;
    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings, {}, options);

    REQUIRE(index.entries.size() == 3u);
    CHECK(warnings.empty());
    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : index.entries) {
        by_path.emplace(entry.path, entry);
    }
    REQUIRE(by_path.count("four.osu") == 1u);
    CHECK(by_path.at("four.osu").format == "osu");
    CHECK(by_path.at("four.osu").key_count == 4);
    CHECK(by_path.at("four.osu").title == "Four");
    CHECK(by_path.at("four.osu").rating >= 0.0);
    REQUIRE(by_path.count("ten.osu") == 1u);
    CHECK(by_path.at("ten.osu").format == "osu");
    CHECK(by_path.at("ten.osu").key_count == 10);
    CHECK(by_path.at("ten.osu").title == "Ten");
    CHECK(by_path.at("ten.osu").rating >= 0.0);
}

TEST_CASE("song scan keeps chart names for BMS subtitles and osu versions") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "named_chart.bms",
               "#TITLE Named Song\n"
               "#SUBTITLE Another\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 10\n"
               "#BPM 150\n"
               "#00111:01\n");
    write_file(temp.path / "named_chart.osu",
               "osu file format v14\n"
               "[General]\n"
               "AudioFilename:test.mp3\n"
               "Mode:3\n"
               "[Metadata]\n"
               "Title:Named Osu\n"
               "TitleUnicode:네임드 오수\n"
               "Artist:Composer\n"
               "ArtistUnicode:작곡가\n"
               "Version:MX\n"
               "[Difficulty]\n"
               "CircleSize:4\n"
               "[TimingPoints]\n"
               "0,500,4,0,0,100,1,0\n"
               "[HitObjects]\n"
               "0,0,0,1,0,0:0:0:0:\n"
               "511,0,500,1,0,0:0:0:0:\n");

    SongIndexOptions options;
    options.include_osu = true;
    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings, {}, options);

    REQUIRE(index.entries.size() == 2u);
    CHECK(warnings.empty());

    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : index.entries) {
        by_path.emplace(entry.path, entry);
    }

    REQUIRE(by_path.count("named_chart.bms") == 1u);
    CHECK(by_path.at("named_chart.bms").chart_name == "Another");

    REQUIRE(by_path.count("named_chart.osu") == 1u);
    CHECK(by_path.at("named_chart.osu").title == std::string(u8"네임드 오수"));
    CHECK(by_path.at("named_chart.osu").artist == std::string(u8"작곡가"));
    CHECK(by_path.at("named_chart.osu").chart_name == "MX");
}

TEST_CASE("song scan computes 10k-calc difficulty for BMS entries") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "sparse.bms", sparse_bms_chart("Sparse", 20));
    write_file(temp.path / "dense.bms", dense_bms_chart("Dense", 1));

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 2u);
    CHECK(warnings.empty());

    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : index.entries) {
        by_path.emplace(entry.path, entry);
    }
    REQUIRE(by_path.count("sparse.bms") == 1u);
    REQUIRE(by_path.count("dense.bms") == 1u);
    CHECK(by_path.at("dense.bms").rating > by_path.at("sparse.bms").rating);
    CHECK(by_path.at("dense.bms").level >= by_path.at("sparse.bms").level);
}

TEST_CASE("song scan exposes BMS charts with explicit 4K headers") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "fourk.bms",
               "#TITLE Four Key\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 7\n"
               "#BPM 150\n"
               "#4K\n"
               "#00111:01\n"
               "#00112:01\n"
               "#00114:01\n"
               "#00115:01\n");

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 1u);
    CHECK(warnings.empty());
    CHECK(index.entries.front().format == "bms");
    CHECK(index.entries.front().key_count == 4);
    CHECK(index.entries.front().level >= 0);
    CHECK(index.entries.front().rating >= 0.0);
}

TEST_CASE("song scan computes difficulty for non-10K charts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "four_dense.bms",
               "#TITLE Four Dense\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 7\n"
               "#BPM 150\n"
               "#4K\n"
               "#00111:01010101\n"
               "#00112:00010001\n"
               "#00114:01010101\n"
               "#00115:00010001\n");

    SongIndexOptions options;
    options.include_osu = true;
    write_file(temp.path / "four_dense.osu",
               "osu file format v14\n"
               "[General]\n"
               "AudioFilename:test.mp3\n"
               "Mode:3\n"
               "[Metadata]\n"
               "Title:Four Dense Osu\n"
               "Artist:Composer\n"
               "[Difficulty]\n"
               "CircleSize:4\n"
               "OverallDifficulty:8\n"
               "[TimingPoints]\n"
               "0,500,4,0,0,100,1,0\n"
               "[HitObjects]\n"
               "0,0,0,1,0,0:0:0:0:\n"
               "170,0,90,1,0,0:0:0:0:\n"
               "341,0,180,1,0,0:0:0:0:\n"
               "511,0,270,1,0,0:0:0:0:\n"
               "0,0,360,1,0,0:0:0:0:\n"
               "170,0,450,1,0,0:0:0:0:\n"
               "341,0,540,1,0,0:0:0:0:\n"
               "511,0,630,1,0,0:0:0:0:\n");

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings, {}, options);

    REQUIRE(index.entries.size() == 2u);
    CHECK(warnings.empty());
    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : index.entries) {
        by_path.emplace(entry.path, entry);
    }
    REQUIRE(by_path.count("four_dense.bms") == 1u);
    REQUIRE(by_path.count("four_dense.osu") == 1u);
    CHECK(by_path.at("four_dense.bms").rating > 0.0);
    CHECK(by_path.at("four_dense.osu").rating > 0.0);
}

TEST_CASE("song scan exposes BMS SP layouts with compact lane counts and labels") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "sp_header.bms",
               "#TITLE SP Header\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 9\n"
               "#BPM 150\n"
               "#5K\n"
               "#00111:01\n"
               "#00112:01\n"
               "#00113:01\n"
               "#00114:01\n"
               "#00115:01\n"
               "#00116:01\n");

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 1u);
    CHECK(warnings.empty());
    CHECK(index.entries.front().format == "bms");
    CHECK(index.entries.front().key_count == 6);
    CHECK(index.entries.front().layout_label == "5+1 SP");
    CHECK(index.entries.front().level == 2);
    CHECK(index.entries.front().rating == doctest::Approx(1.473446353));
}

TEST_CASE("song scan exposes PMS and 14+2 DP BMS layouts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "native_popnine.pms",
               "#TITLE Native PMS\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 9\n"
               "#BPM 150\n"
               "#00111:01\n"
               "#00115:01\n"
               "#00122:01\n"
               "#00125:01\n");
    write_file(temp.path / "dpa_layout.bms",
               "#TITLE DPA Layout\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 12\n"
               "#PLAYER 3\n"
               "#BPM 150\n"
               "#00111:01\n"
               "#00116:01\n"
               "#00119:01\n"
               "#00121:01\n"
               "#00126:01\n"
               "#00129:01\n");

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 2u);
    CHECK(warnings.empty());
    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : index.entries) {
        by_path.emplace(entry.path, entry);
    }
    REQUIRE(by_path.count("native_popnine.pms") == 1u);
    CHECK(by_path.at("native_popnine.pms").key_count == 9);
    CHECK(by_path.at("native_popnine.pms").layout_label == "PMS 9K");
    REQUIRE(by_path.count("dpa_layout.bms") == 1u);
    CHECK(by_path.at("dpa_layout.bms").key_count == 16);
    CHECK(by_path.at("dpa_layout.bms").layout_label == "14+2 DP");
    CHECK(by_path.at("dpa_layout.bms").level == 3);
    CHECK(by_path.at("dpa_layout.bms").rating == doctest::Approx(1.578062082));
}

TEST_CASE("song scan infers sparse standard SP BMS layouts instead of defaulting to 10K") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "sparse_standard_sp.bms",
               "#TITLE Sparse Standard SP\n"
               "#ARTIST Composer\n"
               "#PLAYLEVEL 9\n"
               "#BPM 150\n"
               "#00111:01\n"
               "#00114:01\n"
               "#00119:01\n"
               "#00212:01\n");

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 1u);
    CHECK(warnings.empty());
    CHECK(index.entries.front().key_count == 8);
    CHECK(index.entries.front().layout_label == "7+1 SP");
}

TEST_CASE("song scan supports UTF-8 song roots") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto korean_root = temp.path / std::filesystem::u8path(u8"한글 폴더");
    std::filesystem::create_directories(korean_root);
    write_file(korean_root / std::filesystem::u8path(u8"테스트.bms"), minimal_bms_chart("Legacy", 12));

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(korean_root.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 1u);
    CHECK(warnings.empty());
    CHECK(index.entries.front().path == std::filesystem::u8path(u8"테스트.bms").u8string());
}

TEST_CASE("cached song index load drops non-BMS menu entries") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto index_path = temp.path / "song_index.json";
    write_file(index_path,
               "{\n"
               "  \"version\": 8,\n"
               "  \"include_osu\": true,\n"
               "  \"entries\": [\n"
               "    {\"path\":\"ten.osu\",\"title\":\"Ten\",\"artist\":\"A\",\"chart_name\":\"MX\",\"format\":\"osu\",\"key_count\":10,\"level\":12,\"rating\":8.25,\"bpm\":180,\"mtime\":1},\n"
               "    {\"path\":\"legacy.bms\",\"title\":\"Legacy\",\"artist\":\"C\",\"chart_name\":\"Another\",\"format\":\"bms\",\"layout_label\":\"5+1 SP\",\"key_count\":6,\"level\":11,\"rating\":6.75,\"bpm\":180,\"mtime\":1},\n"
               "    {\"path\":\"another.bme\",\"title\":\"Another\",\"artist\":\"D\",\"format\":\"bms\",\"key_count\":10,\"level\":14,\"rating\":8.5,\"bpm\":180,\"mtime\":1}\n"
               "  ]\n"
               "}\n");

    SongIndexLoadResult result = load_song_index(index_path.u8string());

    CHECK(result.success());
    CHECK(result.loaded_from_file);
    REQUIRE(result.index.entries.size() == 2u);
    std::unordered_map<std::string, tenriff::app::SongEntry> by_path;
    for (const auto& entry : result.index.entries) {
        by_path.emplace(entry.path, entry);
    }
    REQUIRE(by_path.count("legacy.bms") == 1u);
    REQUIRE(by_path.count("another.bme") == 1u);
    CHECK(by_path.at("legacy.bms").key_count == 6);
    CHECK(by_path.at("legacy.bms").layout_label == "5+1 SP");
    CHECK(by_path.at("legacy.bms").chart_name == "Another");
    CHECK(by_path.at("legacy.bms").level == 11);
    CHECK(by_path.at("legacy.bms").rating == doctest::Approx(6.75));
    CHECK(by_path.at("another.bme").rating == doctest::Approx(8.5));
}

TEST_CASE("cached song index exposes osu entries when enabled") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto index_path = temp.path / "song_index.json";
    write_file(index_path,
               "{\n"
               "  \"version\": 8,\n"
               "  \"include_osu\": true,\n"
               "  \"entries\": [\n"
                "    {\"path\":\"ten.osu\",\"title\":\"Ten\",\"artist\":\"A\",\"chart_name\":\"MX\",\"format\":\"osu\",\"key_count\":10,\"level\":12,\"rating\":8.25,\"bpm\":180,\"mtime\":1},\n"
                "    {\"path\":\"legacy.bms\",\"title\":\"Legacy\",\"artist\":\"C\",\"format\":\"bms\",\"key_count\":10,\"level\":11,\"rating\":6.75,\"bpm\":180,\"mtime\":1}\n"
               "  ]\n"
               "}\n");

    SongIndexOptions options;
    options.include_osu = true;
    SongIndexLoadResult result = load_song_index(index_path.u8string(), options);

    CHECK(result.success());
    CHECK(result.loaded_from_file);
    REQUIRE(result.index.entries.size() == 2u);
}

TEST_CASE("streaming song index loader parses compact single-line schema 8 caches") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto index_path = temp.path / "song_index.json";
    write_file(index_path,
               "{\"version\":8,\"include_osu\":false,\"entries\":["
               "{\"path\":\"legacy.bms\",\"title\":\"Legacy\",\"artist\":\"Composer\",\"chart_name\":\"Hyper\",\"format\":\"bms\",\"layout_label\":\"7+1 SP\",\"key_count\":8,\"level\":12,\"rating\":7.5,\"bpm\":150,\"mtime\":1},"
               "{\"path\":\"ignored.osu\",\"title\":\"Ignored\",\"artist\":\"Mapper\",\"format\":\"osu\",\"key_count\":10,\"level\":13,\"rating\":8.5,\"bpm\":180,\"mtime\":2}"
               "]}");

    SongIndexLoadResult result = load_song_index(index_path.u8string());

    CHECK(result.success());
    CHECK(result.loaded_from_file);
    REQUIRE(result.index.entries.size() == 1u);
    CHECK(result.index.entries.front().path == "legacy.bms");
    CHECK(result.index.entries.front().chart_name == "Hyper");
    CHECK(result.index.entries.front().layout_label == "7+1 SP");
    CHECK(result.index.entries.front().rating == doctest::Approx(7.5));
}

TEST_CASE("song index save and load support UTF-8 cache paths") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto cache_dir = temp.path / std::filesystem::u8path(u8"한글 캐시");
    std::filesystem::create_directories(cache_dir);
    const auto index_path = cache_dir / "song_index.json";

    SongIndex index;
    tenriff::app::SongEntry entry;
    entry.path = std::filesystem::u8path(u8"테스트.bms").u8string();
    entry.title = "Legacy";
    entry.artist = "Composer";
    entry.chart_name = "Another";
    entry.format = "bms";
    entry.layout_label = "7+1 SP";
    entry.key_count = 8;
    entry.level = 12;
    entry.rating = 7.5;
    entry.bpm = 150.0;
    entry.mtime = 1;
    index.entries.push_back(entry);

    std::string error;
    REQUIRE(save_song_index(index_path.u8string(), index, {}, &error));
    CHECK(error.empty());

    SongIndexLoadResult result = load_song_index(index_path.u8string());

    CHECK(result.success());
    CHECK(result.loaded_from_file);
    REQUIRE(result.index.entries.size() == 1u);
    CHECK(result.index.entries.front().path == entry.path);
    CHECK(result.index.entries.front().title == "Legacy");
    CHECK(result.index.entries.front().chart_name == "Another");
    CHECK(result.index.entries.front().layout_label == "7+1 SP");
}

TEST_CASE("song index cache stays profile-local and does not create cache folders in the song source") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto profile_root = temp.path / "profiles" / "default";
    const auto songs_root = temp.path / "HugeLibrary";
    std::filesystem::create_directories(profile_root);
    std::filesystem::create_directories(songs_root);

    const auto cache_path = std::filesystem::path(
        song_index_cache_path_for_source(profile_root.u8string(), songs_root.u8string()));
    const auto legacy_path = std::filesystem::path(legacy_song_index_cache_path_for_source(songs_root.u8string()));

    CHECK(path_has_prefix(cache_path, profile_root));
    CHECK_FALSE(path_has_prefix(cache_path, songs_root));
    CHECK(cache_path != legacy_path);
    CHECK(cache_path.parent_path().filename() == "song-index");

    SongIndex index;
    tenriff::app::SongEntry entry;
    entry.path = "chart.bms";
    entry.title = "Title";
    entry.artist = "Artist";
    entry.chart_name = "Hyper";
    entry.format = "bms";
    entry.layout_label = "10K BMS";
    entry.key_count = 10;
    entry.level = 12;
    entry.rating = 4.5;
    entry.bpm = 150.0;
    entry.mtime = 1;
    index.entries.push_back(entry);

    std::string error;
    REQUIRE(save_song_index(cache_path.u8string(), index, {}, &error));
    CHECK(error.empty());
    CHECK(std::filesystem::exists(cache_path));
    CHECK_FALSE(std::filesystem::exists(songs_root / ".tenriff"));
}

TEST_CASE("song scan sanitizes malformed UI metadata without failing") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::string malformed_title = "Bad_";
    malformed_title.push_back(static_cast<char>(0x81));
    malformed_title.push_back(static_cast<char>(0xFF));
    malformed_title += "_Title";

    std::string chart = "#TITLE " + malformed_title + "\n"
                        "#ARTIST  Artist\tName\r\n"
                        "#PLAYLEVEL 12\n"
                        "#BPM 150\n"
                        "#00111:01\n";
    write_file(temp.path / "broken_meta.bms", chart);

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    REQUIRE(index.entries.size() == 1u);
    CHECK(warnings.empty());
    CHECK_FALSE(index.entries.front().title.empty());
    CHECK(index.entries.front().title.find('\n') == std::string::npos);
    CHECK(index.entries.front().title.find('\0') == std::string::npos);
    CHECK(index.entries.front().artist == "Artist Name");
}

TEST_CASE("cached song index sanitizes control heavy metadata on load") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto index_path = temp.path / "song_index.json";
    write_file(index_path,
               "{\n"
               "  \"version\": 8,\n"
               "  \"entries\": [\n"
                "    {\"path\":\"legacy.bms\",\"title\":\"Bad\\nTitle\",\"artist\":\"Artist\\tName\",\"chart_name\":\"Hyper\\r\",\"format\":\"bms\\r\",\"key_count\":10,\"level\":11,\"rating\":6.75,\"bpm\":180,\"mtime\":1}\n"
               "  ]\n"
               "}\n");

    SongIndexLoadResult result = load_song_index(index_path.u8string());

    CHECK(result.success());
    CHECK(result.loaded_from_file);
    REQUIRE(result.index.entries.size() == 1u);
    CHECK(result.index.entries.front().title == "Bad Title");
    CHECK(result.index.entries.front().artist == "Artist Name");
    CHECK(result.index.entries.front().chart_name == "Hyper");
    CHECK(result.index.entries.front().format == "bms");
}

TEST_CASE("song index budget leaves RAM reserve under low memory") {
    const auto budget = tenriff::app::choose_song_index_work_budget(
        tenriff::app::SongIndexProfile::Safe,
        16,
        5000,
        tenriff::app::SongIndexMemorySnapshot{
            1ull * 1024ull * 1024ull * 1024ull,
            16ull * 1024ull * 1024ull * 1024ull});

    CHECK(budget.reserve_bytes >= 768ull * 1024ull * 1024ull);
    CHECK(budget.headroom_bytes < 512ull * 1024ull * 1024ull);
    CHECK(budget.worker_count < 16u);
    CHECK(budget.worker_count <= 2u);
    CHECK(budget.batch_size >= budget.worker_count);
    CHECK(budget.batch_size <= 64u);
}

TEST_CASE("song index fast profile allows more work than safe profile when memory is plentiful") {
    const tenriff::app::SongIndexMemorySnapshot memory{
        24ull * 1024ull * 1024ull * 1024ull,
        32ull * 1024ull * 1024ull * 1024ull};
    const auto safe_budget = tenriff::app::choose_song_index_work_budget(
        tenriff::app::SongIndexProfile::Safe, 16, 50000, memory);
    const auto fast_budget = tenriff::app::choose_song_index_work_budget(
        tenriff::app::SongIndexProfile::Fast, 16, 50000, memory);

    CHECK(fast_budget.worker_count >= safe_budget.worker_count);
    CHECK(fast_budget.batch_size >= safe_budget.batch_size);
    CHECK(fast_budget.reserve_bytes <= safe_budget.reserve_bytes);
}

TEST_CASE("song scan progress reports scanning and metadata stages") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "one.bms", minimal_bms_chart("One", 10));
    write_file(temp.path / "two.bms", minimal_bms_chart("Two", 11));

    std::vector<std::string> warnings;
    std::vector<std::tuple<SongIndexProgressStage, int, int>> events;
    SongIndex index = scan_songs(
        temp.path.u8string(),
        nullptr,
        warnings,
        [&events](const SongIndexProgress& progress) {
            events.emplace_back(progress.stage, progress.processed, progress.total);
        });

    REQUIRE(index.entries.size() == 2u);
    CHECK(warnings.empty());
    REQUIRE_FALSE(events.empty());
    CHECK(std::get<0>(events.front()) == SongIndexProgressStage::ScanningFiles);
    CHECK(std::get<0>(events.back()) == SongIndexProgressStage::BuildingMetadata);
    CHECK(std::get<1>(events.back()) == 2);
    CHECK(std::get<2>(events.back()) == 2);
}

TEST_CASE("song index save reports streaming cache progress") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto index_path = temp.path / "song_index.json";
    SongIndex index;
    for (int i = 0; i < 3; ++i) {
        tenriff::app::SongEntry entry;
        entry.path = "chart" + std::to_string(i) + ".bms";
        entry.title = "Chart " + std::to_string(i);
        entry.artist = "Composer";
        entry.format = "bms";
        entry.key_count = 10;
        entry.level = 12;
        entry.rating = 7.0 + static_cast<double>(i);
        entry.bpm = 150.0;
        entry.mtime = i;
        index.entries.push_back(std::move(entry));
    }

    std::vector<std::tuple<SongIndexProgressStage, int, int>> events;
    std::string error;
    REQUIRE(save_song_index(index_path.u8string(),
                            index,
                            {},
                            &error,
                            [&events](const SongIndexProgress& progress) {
                                events.emplace_back(progress.stage, progress.processed, progress.total);
                            }));
    CHECK(error.empty());
    REQUIRE_FALSE(events.empty());
    CHECK(std::get<0>(events.front()) == SongIndexProgressStage::SavingCache);
    CHECK(std::get<0>(events.back()) == SongIndexProgressStage::SavingCache);
    CHECK(std::get<1>(events.back()) == 3);
    CHECK(std::get<2>(events.back()) == 3);
}

TEST_CASE("song scan skips oversized metadata charts instead of stalling the batch") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    std::string oversized_chart;
    oversized_chart.reserve(9u * 1024u * 1024u);
    oversized_chart.append("#TITLE Oversized\n#ARTIST Composer\n#PLAYLEVEL 12\n");
    oversized_chart.append(9u * 1024u * 1024u, 'A');
    write_file(temp.path / "oversized.bms", oversized_chart);

    std::vector<std::string> warnings;
    SongIndex index = scan_songs(temp.path.u8string(), nullptr, warnings);

    CHECK(index.entries.empty());
    REQUIRE_FALSE(warnings.empty());
    CHECK(warnings.front().find("Skipped oversized metadata chart file") != std::string::npos);
}

TEST_CASE("song index save cancellation removes the partial cache file") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    SongIndex index;
    index.entries.push_back({"one.bms", "One", "Artist", "Hyper", "bms", "", 10, 10, 1.0, 150.0, 1});

    const auto index_path = temp.path / "song_index.json";
    std::string error;
    const bool saved = save_song_index(
        index_path.u8string(),
        index,
        {},
        &error,
        {},
        []() { return true; });

    CHECK_FALSE(saved);
    CHECK(error == "Song index save canceled.");
    CHECK_FALSE(std::filesystem::exists(index_path));
}

TEST_CASE("stale song index version triggers silent rescan instead of using cached entries") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto index_path = temp.path / "song_index.json";
    write_file(index_path,
               "{\n"
               "  \"version\": 6,\n"
               "  \"entries\": [\n"
                "    {\"path\":\"legacy.bms\",\"title\":\"Legacy\",\"artist\":\"C\",\"format\":\"bms\",\"key_count\":10,\"level\":11,\"rating\":6.75,\"bpm\":180,\"mtime\":1}\n"
               "  ]\n"
               "}\n");

    SongIndexLoadResult result = load_song_index(index_path.u8string());

    CHECK(result.success());
    CHECK_FALSE(result.loaded_from_file);
    CHECK(result.index.entries.empty());
    CHECK(result.warnings.empty());
}

TEST_CASE("missing song index reports not loaded from file") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    SongIndexLoadResult result = load_song_index((temp.path / "missing_song_index.json").u8string());

    CHECK(result.success());
    CHECK_FALSE(result.loaded_from_file);
    CHECK(result.index.entries.empty());
    CHECK_FALSE(result.warnings.empty());
}
