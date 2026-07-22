#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/ImportedGameplaySkin.h"
#include "app/OsuSkin.h"

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
    std::filesystem::path old_path = std::filesystem::current_path();

    ~CurrentPathGuard() {
        std::error_code ec;
        std::filesystem::current_path(old_path, ec);
    }
};

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path() / "tenriff_osu_skin_tests";
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

void write_file(const std::filesystem::path& path, const std::string& content = "x") {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << content;
}

}  // namespace

TEST_CASE("osu skin scanner lists skins under the default test root") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "build" / "Release" / "test-skins-osu";
    std::filesystem::create_directories(root / "SampleSkin");
    write_file(root / "SampleSkin" / "skin.ini", "[Mania]\nKeys: 4\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    const std::string detected_root = tenriff::app::find_default_osu_skin_test_root();
    REQUIRE_FALSE(detected_root.empty());

    const auto skins = tenriff::app::list_osu_skin_names(detected_root);
    REQUIRE(skins.size() == 1u);
    CHECK(skins.front() == "SampleSkin");
}

TEST_CASE("osu skin detector recognizes a direct skin folder") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto skin = temp.path / "DirectSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "mania-note1.png");

    CHECK(tenriff::app::is_osu_skin_directory(skin.u8string()));
    CHECK_FALSE(tenriff::app::is_osu_skin_directory((temp.path / "MissingSkin").u8string()));
}

TEST_CASE("osu skin resolver honors mania note and key mappings from skin ini") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "MappedSkin";
    std::filesystem::create_directories(skin / "arrows");
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "NoteImage0: arrows/left\n"
               "NoteImage0H: arrows/leftH\n"
               "NoteImage0L: arrows/body\n"
               "NoteImage0T: arrows/tail\n"
               "KeyImage0: arrows/key\n"
               "KeyImage0D: arrows/keyD\n");
    write_file(skin / "arrows" / "left.png");
    write_file(skin / "arrows" / "leftH.png");
    write_file(skin / "arrows" / "body.png");
    write_file(skin / "arrows" / "tail.png");
    write_file(skin / "arrows" / "key.png");
    write_file(skin / "arrows" / "keyD.png");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "MappedSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 4u);
    CHECK(resolved.note_images[0].find("left.png") != std::string::npos);
    CHECK(resolved.hold_head_images[0].find("leftH.png") != std::string::npos);
    CHECK(resolved.hold_body_images[0].find("body.png") != std::string::npos);
    CHECK(resolved.hold_tail_images[0].find("tail.png") != std::string::npos);
    CHECK(resolved.key_images[0].find("key.png") != std::string::npos);
    CHECK(resolved.key_pressed_images[0].find("keyD.png") != std::string::npos);
}

TEST_CASE("osu skin resolver rejects asset paths outside the selected skin") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "SafeSkin";
    const auto outside = root / "outside.png";
    std::filesystem::create_directories(skin);
    write_file(outside);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "NoteImage0: ../outside\n"
               "NoteImage1: " + outside.u8string() + "\n");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "SafeSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 4u);
    CHECK(resolved.note_images[0].empty());
    CHECK(resolved.note_images[1].empty());

    CHECK_FALSE(tenriff::app::resolve_osu_mania_skin(root.u8string(), "../SafeSkin", 4).found);
}

TEST_CASE("osu skin resolver contains assets reached through directory links") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "LinkedSkin";
    const auto outside_dir = temp.path / "outside";
    std::filesystem::create_directories(skin);
    std::filesystem::create_directories(outside_dir);
    write_file(outside_dir / "note.png");
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "NoteImage0: linked/note\n");

    std::error_code ec;
    std::filesystem::create_directory_symlink(outside_dir, skin / "linked", ec);
    if (!ec) {
        const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "LinkedSkin", 4);
        REQUIRE(resolved.found);
        CHECK(resolved.note_images[0].empty());
    }
}

TEST_CASE("osu skin resolver requires an exact mania key section when skin ini exists") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto mismatch = root / "MismatchSkin";
    const auto non_mania = root / "NonManiaSkin";
    std::filesystem::create_directories(mismatch);
    std::filesystem::create_directories(non_mania);
    write_file(mismatch / "skin.ini", "[Mania]\nKeys: 7\n");
    write_file(mismatch / "mania-note1.png");
    write_file(non_mania / "skin.ini", "[General]\nName: UI only\n");
    write_file(non_mania / "mania-note1.png");

    CHECK_FALSE(tenriff::app::resolve_osu_mania_skin(root.u8string(), "MismatchSkin", 4).found);
    CHECK_FALSE(tenriff::app::resolve_osu_mania_skin(root.u8string(), "NonManiaSkin", 4).found);
}

TEST_CASE("osu skin resolver prefers high DPI png assets") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "HighDpiSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "NoteImage0: note.png\n");
    write_file(skin / "note.png", "sd");
    write_file(skin / "note@2x.png", "hd");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "HighDpiSkin", 4);
    REQUIRE(resolved.found);
    CHECK(resolved.note_images[0].find("note@2x.png") != std::string::npos);
}

TEST_CASE("osu skin resolver finds extensionless Korean asset tokens") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "KoreanSkin";
    const auto folder = skin / std::filesystem::u8path(u8"화살표");
    std::filesystem::create_directories(folder);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "NoteImage0: 화살표/노트\n"
               "KeyImage0: 화살표/키\n");
    write_file(folder / std::filesystem::u8path(u8"노트.png"));
    write_file(folder / std::filesystem::u8path(u8"키.png"));

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "KoreanSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 4u);
    CHECK(resolved.note_images[0] == (folder / std::filesystem::u8path(u8"노트.png")).u8string());
    CHECK(resolved.key_images[0] == (folder / std::filesystem::u8path(u8"키.png")).u8string());
}

TEST_CASE("osu skin resolver falls back to standard mania family assets") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "FallbackSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "mania-note1.png");
    write_file(skin / "mania-note2.png");
    write_file(skin / "mania-note3.png");
    write_file(skin / "mania-note1H.png");
    write_file(skin / "mania-note1L.png");
    write_file(skin / "mania-note1T.png");
    write_file(skin / "mania-key1.png");
    write_file(skin / "mania-key1D.png");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "FallbackSkin", 10);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 10u);
    CHECK_FALSE(resolved.note_images[0].empty());
    CHECK_FALSE(resolved.note_images[2].empty());
    CHECK_FALSE(resolved.key_images[0].empty());
    CHECK_FALSE(resolved.key_pressed_images[0].empty());
}

TEST_CASE("osu skin resolver uses official mania fallback families and composed extended layouts") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "FallbackFamilies";
    std::filesystem::create_directories(skin);
    for (const std::string family : {"1", "2", "3", "S"}) {
        write_file(skin / ("mania-note" + family + ".png"));
    }

    struct FallbackCase {
        int keys;
        std::vector<std::string> families;
    };
    const std::vector<FallbackCase> cases = {
        {4, {"1", "2", "2", "1"}},
        {5, {"1", "2", "S", "2", "1"}},
        {6, {"1", "2", "1", "1", "2", "1"}},
        {7, {"1", "2", "1", "S", "1", "2", "1"}},
        {8, {"1", "2", "1", "2", "2", "1", "2", "1"}},
        {9, {"1", "2", "1", "2", "S", "2", "1", "2", "1"}},
        {10, {"1", "2", "S", "2", "1", "1", "2", "S", "2", "1"}},
        {16, {"1", "2", "1", "2", "2", "1", "2", "1",
              "1", "2", "1", "2", "2", "1", "2", "1"}},
    };

    for (const auto& fallback_case : cases) {
        const auto resolved = tenriff::app::resolve_osu_mania_skin(
            root.u8string(), "FallbackFamilies", fallback_case.keys);
        REQUIRE(resolved.found);
        REQUIRE(resolved.note_images.size() == fallback_case.families.size());
        for (std::size_t lane = 0; lane < fallback_case.families.size(); ++lane) {
            const auto actual = std::filesystem::u8path(resolved.note_images[lane]).filename().u8string();
            const auto expected = "mania-note" + fallback_case.families[lane] + ".png";
            CHECK(actual == expected);
        }
    }
}

TEST_CASE("osu skin resolver imports mania column line widths as internal lane dividers") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "DividerSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "ColumnLineWidth: 0,1.5,2.5,3.5,0\n");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "DividerSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.lane_divider_widths.size() == 3u);
    CHECK(resolved.lane_divider_widths[0] == doctest::Approx(1.5f));
    CHECK(resolved.lane_divider_widths[1] == doctest::Approx(2.5f));
    CHECK(resolved.lane_divider_widths[2] == doctest::Approx(3.5f));
}

TEST_CASE("osu skin resolver parses column width and note height scale metrics") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "MetricSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "ColumnWidth: 24,30,36,30\n"
               "ColumnSpacing: 1,2,3\n"
               "HitPosition: 402\n"
               "WidthForNoteHeightScale: 45\n");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "MetricSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.column_widths.size() == 4u);
    CHECK(resolved.column_widths[0] == doctest::Approx(24.0f));
    CHECK(resolved.column_widths[2] == doctest::Approx(36.0f));
    REQUIRE(resolved.column_spacings.size() == 3u);
    CHECK(resolved.column_spacings[0] == doctest::Approx(1.0f));
    CHECK(resolved.column_spacings[2] == doctest::Approx(3.0f));
    CHECK(resolved.has_hit_position);
    CHECK(resolved.hit_position == doctest::Approx(402.0f));
    CHECK(resolved.width_for_note_height_scale == doctest::Approx(45.0f));
    CHECK(resolved.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(resolved.imported_note_height_ratio == doctest::Approx(1.5f));

    const auto imported = tenriff::app::resolve_imported_gameplay_skin(
        "osu", root.u8string(), "MetricSkin", 4);
    REQUIRE(imported.found);
    REQUIRE(imported.column_widths.size() == 4u);
    REQUIRE(imported.column_spacings.size() == 3u);
    CHECK(imported.column_widths[2] == doctest::Approx(36.0f));
    CHECK(imported.column_spacings[2] == doctest::Approx(3.0f));
    CHECK(imported.has_hit_position);
    CHECK(imported.hit_position == doctest::Approx(402.0f));
}

TEST_CASE("osu skin resolver fills short column metrics with mania defaults") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "ShortMetricSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "ColumnWidth: 24,36\n"
               "ColumnSpacing: 1,2\n"
               "ColumnLineWidth: 4\n");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(
        root.u8string(), "ShortMetricSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.column_widths.size() == 4u);
    CHECK(resolved.column_widths[0] == doctest::Approx(24.0f));
    CHECK(resolved.column_widths[1] == doctest::Approx(36.0f));
    CHECK(resolved.column_widths[2] == doctest::Approx(30.0f));
    CHECK(resolved.column_widths[3] == doctest::Approx(30.0f));
    REQUIRE(resolved.column_spacings.size() == 3u);
    CHECK(resolved.column_spacings[0] == doctest::Approx(1.0f));
    CHECK(resolved.column_spacings[1] == doctest::Approx(2.0f));
    CHECK(resolved.column_spacings[2] == doctest::Approx(0.0f));
    REQUIRE(resolved.lane_divider_widths.size() == 3u);
    CHECK(resolved.lane_divider_widths[0] == doctest::Approx(4.0f));
    CHECK(resolved.lane_divider_widths[1] == doctest::Approx(2.0f));
    CHECK(resolved.lane_divider_widths[2] == doctest::Approx(2.0f));

    const auto single_skin = root / "SingleMetricSkin";
    std::filesystem::create_directories(single_skin);
    write_file(single_skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "ColumnWidth: 24\n"
               "ColumnSpacing: 1\n");
    const auto single = tenriff::app::resolve_osu_mania_skin(
        root.u8string(), "SingleMetricSkin", 4);
    REQUIRE(single.found);
    REQUIRE(single.column_widths.size() == 4u);
    CHECK(single.column_widths[0] == doctest::Approx(24.0f));
    CHECK(single.column_widths[1] == doctest::Approx(30.0f));
    REQUIRE(single.column_spacings.size() == 3u);
    CHECK(single.column_spacings[0] == doctest::Approx(1.0f));
    CHECK(single.column_spacings[1] == doctest::Approx(0.0f));
}

TEST_CASE("imported osu gameplay skin applies missing assets per lane") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "PerLaneFallbackSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.ini",
               "[Mania]\n"
               "Keys: 4\n"
               "NoteImage0: note0\n"
               "NoteImage0H: head0\n"
               "NoteImage1: note1\n");
    write_file(skin / "note0.png");
    write_file(skin / "head0.png");
    write_file(skin / "note1.png");

    const auto resolved = tenriff::app::resolve_imported_gameplay_skin(
        "osu", root.u8string(), "PerLaneFallbackSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.key_images.size() == 4u);
    REQUIRE(resolved.key_pressed_images.size() == 4u);
    REQUIRE(resolved.hold_tail_images.size() == 4u);
    CHECK(resolved.key_images[0].path == resolved.note_images[0].path);
    CHECK(resolved.key_images[1].path == resolved.note_images[1].path);
    CHECK(resolved.key_pressed_images[0].path == resolved.hold_head_images[0].path);
    CHECK(resolved.key_pressed_images[1].path == resolved.note_images[1].path);
    CHECK(resolved.hold_tail_images[0].path == resolved.hold_head_images[0].path);
    CHECK(resolved.hold_tail_images[1].path.empty());
}
