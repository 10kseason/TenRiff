#include "app/BmsEditor.h"
#include "gameplay/GameplayChart.h"

#include "chart/BmsParser.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct TempFile {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "tenriff_bms_editor_tests";

    TempFile() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root, ec);
    }

    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

void write_chart(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    file << "#TITLE:Editor test\n"
            "#ARTIST:TenRiff\n"
            "#BPM:120\n"
            "#WAV01:hit.wav\n"
            "#00111:01000000\n"
            "#00112:00010000\n"
            "#00103:00000001\n";
}

}  // namespace

TEST_CASE("BMS editor loads, toggles, and saves normal notes") {
    TempFile temp;
    const auto input = temp.root / "input.bms";
    const auto output = temp.root / "edited.bms";
    write_chart(input);

    tenriff::app::BmsEditorDocument document;
    std::string error;
    REQUIRE(document.load_file(input.u8string(), &error));
    CHECK(document.title() == "Editor test");
    CHECK(document.artist() == "TenRiff");
    CHECK(document.lane_count() >= 4);
    CHECK(document.editable_note_count() == 2);

    document.set_cursor(3, 0, 96);
    REQUIRE(document.toggle_cursor_note());
    CHECK(document.editable_note_count() == 3);
    REQUIRE(document.save_as(output.u8string(), &error));

    tenriff::chart::BmsParser parser;
    const auto parsed = parser.parseFile(output.u8string());
    REQUIRE(parsed.success());
    bool found_new_lane = false;
    for (const auto& command : parsed.chart.commands) {
        if (command.channel == "13" && command.data.size() >= 194 && command.data.substr(192, 2) != "00") {
            found_new_lane = true;
        }
    }
    CHECK(found_new_lane);
}

TEST_CASE("BMS editor undo and redo restore note edits") {
    TempFile temp;
    const auto input = temp.root / "input.bms";
    write_chart(input);

    tenriff::app::BmsEditorDocument document;
    REQUIRE(document.load_file(input.u8string()));
    document.set_cursor(3, 0, 24);
    REQUIRE(document.toggle_cursor_note());
    CHECK(document.editable_note_count() == 3);
    REQUIRE(document.undo());
    CHECK(document.editable_note_count() == 2);
    REQUIRE(document.redo());
    CHECK(document.editable_note_count() == 3);
}

TEST_CASE("BMS editor retains exact fractions, keysounds, long notes, BGM, and unknown commands") {
    TempFile temp;
    // Keep the filesystem path ASCII so the test remains portable across
    // Windows locales; the chart content itself still exercises Unicode.
    const auto input = temp.root / "unicode.bms";
    const auto output = temp.root / "edited-unicode.bms";
    {
        std::ofstream file(input, std::ios::binary);
        file << "#TITLE:曲テスト\n#ARTIST:作曲者\n#BPM:120\n#LNOBJ:ZZ\n"
                "#WAV01:tap.wav\n#WAV02:hold.wav\n#00101:02000000\n"
                "#00111:010000ZZ\n#00152:02000000\n#00252:00000200\n"
                "#00103:00000078\n#00102:1\n#001AA:ABCD\n#FOO:kept\n";
    }
    tenriff::app::BmsEditorDocument document;
    REQUIRE(document.load_file(input.u8string()));
    CHECK(document.title() == "曲テスト");
    REQUIRE(document.note_count() == 2); // LNOBJ pair and explicit LN pair.
    const auto& notes = document.notes();
    CHECK(notes[0].slice_count == 4);
    CHECK(notes[0].token == "01");
    CHECK(notes[0].long_note);
    CHECK(notes[0].end_slice == 3);
    CHECK(notes[0].end_slice_count == 4);
    CHECK(notes[1].long_note);
    CHECK(notes[1].token == "02");
    CHECK(notes[1].end_slice_count == 4);
    std::string error;
    REQUIRE(document.save_as(output.u8string(), &error));
    CHECK_FALSE(document.dirty());
    tenriff::chart::BmsParser parser;
    const auto parsed = parser.parseFile(output.u8string());
    REQUIRE(parsed.success());
    CHECK(parsed.chart.wav.at("01") == "tap.wav");
    CHECK(parsed.chart.wav.at("02") == "hold.wav");
    CHECK(parsed.chart.headers.at("FOO") == "kept"); // unknown headers remain parse-safe.
    bool bgm = false, unknown = false;
    for (const auto& command : parsed.chart.commands) {
        if (command.channel == "01") bgm = true;
        if (command.channel == "AA") unknown = true;
    }
    CHECK(bgm);
    CHECK(unknown);
}

TEST_CASE("BMS editor rejects conditional charts and never overwrites Save As") {
    TempFile temp;
    const auto conditional = temp.root / "conditional.bms";
    const auto existing = temp.root / "existing.bms";
    {
        std::ofstream file(conditional);
        file << "#RANDOM 2\n#IF 1\n#TITLE:bad\n";
    }
    tenriff::app::BmsEditorDocument document;
    std::string error;
    CHECK_FALSE(document.load_file(conditional.u8string(), &error));
    CHECK(error.find("RANDOM") != std::string::npos);
    write_chart(existing);
    REQUIRE(document.load_file(existing.u8string(), &error));
    const auto before = std::filesystem::file_size(existing);
    CHECK_FALSE(document.save_as(existing.u8string(), &error));
    CHECK(std::filesystem::file_size(existing) == before);
}

TEST_CASE("BMS editor cursor seek follows chart seconds") {
    TempFile temp;
    const auto input = temp.root / "seek.bms";
    {
        std::ofstream file(input);
        file << "#BPM:120\n#WAV01:hit.wav\n#00011:01\n#00311:01\n";
    }
    tenriff::app::BmsEditorDocument document;
    REQUIRE(document.load_file(input.u8string()));
    REQUIRE(document.move_cursor_seconds(5.0));
    CHECK(document.cursor_measure() == 2);
    CHECK(document.cursor_slice() == 96);
    REQUIRE(document.move_cursor_seconds(-5.0));
    CHECK(document.cursor_measure() == 0);
    CHECK(document.cursor_slice() == 0);
    REQUIRE(document.cursor_seconds().has_value());
    CHECK(std::abs(*document.cursor_seconds()) < 0.001);
}

TEST_CASE("practice chart trim starts notes and audio at the editor cursor") {
    tenriff::gameplay::GameplayChart chart;
    chart.duration_samples = 10'000;
    chart.notes.push_back({1, 2'000, std::nullopt});
    chart.notes.push_back({1, 6'000, std::nullopt});
    chart.audio_cues.push_back({1'000, 0});
    chart.audio_cues.push_back({7'000, 0});
    tenriff::gameplay::trim_gameplay_chart_before_sample(chart, 5'000);
    REQUIRE(chart.notes.size() == 1);
    CHECK(chart.notes.front().start_sample == 1'000);
    REQUIRE(chart.audio_cues.size() == 1);
    CHECK(chart.audio_cues.front().start_sample == 2'000);
    CHECK(chart.duration_samples == 5'000);
}

TEST_CASE("BMS editor exposes and moves BGM channel events") {
    TempFile temp;
    const auto input = temp.root / "bgm.bms";
    const auto output = temp.root / "bgm-edited.bms";
    {
        std::ofstream file(input);
        file << "#BPM:120\n#WAV01:bgm.wav\n#00001:01\n#00201:01\n";
    }
    tenriff::app::BmsEditorDocument document;
    REQUIRE(document.load_file(input.u8string()));
    REQUIRE(document.bgm().size() == 2);
    const auto moved_id = document.bgm().front().id;
    REQUIRE(document.move_bgm(moved_id, 1, 96));
    REQUIRE(document.save_as(output.u8string()));
    tenriff::chart::BmsParser parser;
    const auto parsed = parser.parseFile(output.u8string());
    REQUIRE(parsed.success());
    bool found_moved = false;
    for (const auto& command : parsed.chart.commands) {
        if (command.channel == "01" && command.measure == 1 && command.data.size() > 193 &&
            command.data.substr(192, 2) == "01") {
            found_moved = true;
        }
    }
    CHECK(found_moved);
}

TEST_CASE("BMS editor can move a same-time BGM sample onto a note") {
    TempFile temp; const auto input=temp.root/"bgm-note.bms";
    { std::ofstream file(input); file << "#BPM:120\n#WAV01:bgm.wav\n#WAV02:old.wav\n#00001:01\n#00011:02\n"; }
    tenriff::app::BmsEditorDocument document; REQUIRE(document.load_file(input.u8string()));
    REQUIRE(document.bgm().size()==1); REQUIRE(document.notes().size()==1);
    document.set_cursor(1, 0, 0);
    document.select_note(document.notes().front().id);
    REQUIRE(document.move_bgm_to_cursor());
    CHECK(document.bgm().size()==1); CHECK(document.bgm().front().token=="02"); CHECK(document.notes().front().token=="01");
}

