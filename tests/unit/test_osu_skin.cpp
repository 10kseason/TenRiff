#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

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
               "WidthForNoteHeightScale: 45\n");

    const auto resolved = tenriff::app::resolve_osu_mania_skin(root.u8string(), "MetricSkin", 4);
    REQUIRE(resolved.found);
    REQUIRE(resolved.column_widths.size() == 4u);
    CHECK(resolved.column_widths[0] == doctest::Approx(24.0f));
    CHECK(resolved.column_widths[2] == doctest::Approx(36.0f));
    CHECK(resolved.width_for_note_height_scale == doctest::Approx(45.0f));
    CHECK(resolved.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(resolved.imported_note_height_ratio == doctest::Approx(1.5f));
}
