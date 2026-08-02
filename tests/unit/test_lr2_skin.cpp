#include "doctest/doctest.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/Lr2Skin.h"

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
    const auto base = std::filesystem::temp_directory_path() / "tenriff_lr2_skin_tests";
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

std::filesystem::path find_repo_lr2_sample_root() {
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "LR2-skin-sample",
        std::filesystem::absolute(std::filesystem::path(__FILE__)).parent_path().parent_path().parent_path() /
            "LR2-skin-sample",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_directory(candidate, ec)) {
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

void write_simple_lr2_skin(const std::filesystem::path& root,
                           const std::string& skin_name,
                           const std::string& asset_name,
                           std::string_view resolution_header,
                           float first_x,
                           float second_x,
                           float note_width,
                           float note_height) {
    const auto skin = root / skin_name;
    std::filesystem::create_directories(skin / "csv");
    std::filesystem::create_directories(skin / "img" / "notes");

    std::string top_level =
        "#INFORMATION,0," + skin_name + ",Tester\n";
    if (!resolution_header.empty()) {
        top_level += std::string(resolution_header) + "\n";
    }
    top_level += "#ENDOFHEADER\n"
                 "#INCLUDE,LR2Files\\Theme\\" + skin_name + "\\csv\\layout.csv\n";
    write_file(skin / "play.lr2skin", top_level);

    const std::string layout =
        "#IMAGE,LR2Files\\Theme\\" + skin_name + "\\img\\notes\\" + asset_name + "\n" +
        "#SRC_NOTE,0,0,0,0,30,22,1,1,0,0\n"
        "#SRC_NOTE,1,0,30,0,30,22,1,1,0,0\n"
        "#DST_NOTE,0,0," + std::to_string(first_x) + ",400," + std::to_string(note_width) + "," +
        std::to_string(note_height) + ",0,255,255,255,255,0,0,0,0,0,0,0,0,0\n" +
        "#DST_NOTE,1,0," + std::to_string(second_x) + ",400," + std::to_string(note_width) + "," +
        std::to_string(note_height) + ",0,255,255,255,255,0,0,0,0,0,0,0,0,0\n";
    write_file(skin / "csv" / "layout.csv", layout);
    write_file(skin / "img" / "notes" / asset_name);
}

}  // namespace

TEST_CASE("lr2 skin scanner lists skins under the default test root") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "build" / "Release" / "test-skins-lr2";
    std::filesystem::create_directories(root / "SampleSkin");
    write_file(root / "SampleSkin" / "sample.lr2skin", "#INFORMATION,0,Sample,Tester\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    const std::string detected_root = tenriff::app::find_default_lr2_skin_test_root();
    REQUIRE_FALSE(detected_root.empty());

    const auto skins = tenriff::app::list_lr2_skin_names(detected_root);
    REQUIRE(skins.size() == 1u);
    CHECK(skins.front() == "SampleSkin");
}

TEST_CASE("lr2 skin detector recognizes a direct skin folder") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto skin = temp.path / "DirectSkin";
    std::filesystem::create_directories(skin);
    write_file(skin / "play.lr2skin", "#INFORMATION,0,Direct,Tester\n");

    CHECK(tenriff::app::is_lr2_skin_directory(skin.u8string()));
    CHECK_FALSE(tenriff::app::is_lr2_skin_directory((temp.path / "MissingSkin").u8string()));
}

TEST_CASE("lr2 Theme importer installs child skins separately and keeps sibling references") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto source_root = temp.path / "source";
    const auto theme_root = source_root / "LR2files" / "Theme";
    const auto primary = theme_root / "Primary";
    const auto shared = theme_root / "Shared";
    const auto excluded_iidx = theme_root / "IIDX";
    const auto iidx_dependent = theme_root / "IIDX Dependent";
    const auto imported_root = temp.path / "profile" / "skins" / "lr2";
    std::filesystem::create_directories(primary);
    std::filesystem::create_directories(shared);
    std::filesystem::create_directories(excluded_iidx);
    std::filesystem::create_directories(iidx_dependent);

    write_file(primary / "play.lr2skin",
               "#INFORMATION,0,Primary,Tester\n"
               "#ENDOFHEADER\n"
               "#INCLUDE,LR2files\\Theme\\Shared\\layout.csv\n");
    write_file(shared / "shared.lr2skin", "#INFORMATION,0,Shared,Tester\n");
    write_file(shared / "layout.csv",
               "#IMAGE,LR2files\\Theme\\Shared\\note.png\n"
               "#SRC_NOTE,0,0,0,0,30,22,1,1,0,0\n"
               "#SRC_NOTE,1,0,30,0,30,22,1,1,0,0\n"
               "#DST_NOTE,0,0,100,400,30,22,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,1,0,132,400,30,22,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n");
    write_file(shared / "note.png", "portable sibling asset");
    write_file(excluded_iidx / "play.lr2skin", "#INFORMATION,0,Excluded IIDX,Tester\n");
    write_file(iidx_dependent / "play.lr2skin",
               "#INFORMATION,3,Dependent 10K,Tester\n"
               "#INCLUDE,LR2files\\Theme\\IIDX\\csv\\play.csv\n");

    const auto imported = tenriff::app::import_lr2_skin_tree(
        theme_root.u8string(), imported_root.u8string());
    REQUIRE(imported.success());
    REQUIRE(imported.skin_names.size() == 2u);
    CHECK(imported.skin_names[0] == "Primary");
    CHECK(imported.skin_names[1] == "Shared");
    CHECK(imported.copied_files == 4u);
    CHECK(imported.copied_bytes > 0u);
    CHECK(std::filesystem::is_regular_file(imported_root / "Primary" / "play.lr2skin"));
    CHECK(std::filesystem::is_regular_file(imported_root / "Shared" / "note.png"));
    CHECK_FALSE(std::filesystem::exists(imported_root / "Theme"));
    CHECK_FALSE(std::filesystem::exists(imported_root / "IIDX"));
    CHECK_FALSE(std::filesystem::exists(imported_root / "IIDX Dependent"));
    CHECK(std::any_of(imported.warnings.begin(), imported.warnings.end(), [](const std::string& warning) {
        return warning.find("depends on excluded IIDX") != std::string::npos;
    }));

    const auto imported_from_lr2_root = tenriff::app::import_lr2_skin_tree(
        source_root.u8string(), (temp.path / "imported-from-lr2-root").u8string());
    REQUIRE(imported_from_lr2_root.success());
    CHECK(imported_from_lr2_root.skin_names == imported.skin_names);
    CHECK_FALSE(std::filesystem::exists(temp.path / "imported-from-lr2-root" / "IIDX"));

    const auto rejected_iidx = tenriff::app::import_lr2_skin_tree(
        excluded_iidx.u8string(), (temp.path / "rejected-iidx").u8string());
    CHECK_FALSE(rejected_iidx.success());
    CHECK_FALSE(std::filesystem::exists(temp.path / "rejected-iidx" / "IIDX"));

    const auto resolved = tenriff::app::resolve_lr2_play_skin(
        imported_root.u8string(), "Primary", 2);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 2u);
    CHECK(resolved.note_images[0].path.find("Shared") != std::string::npos);
    CHECK(resolved.note_images[0].path.find("note.png") != std::string::npos);

    write_file(imported_root / "Primary" / "keep.txt", "original");
    const auto duplicate = tenriff::app::import_lr2_skin_tree(
        primary.u8string(), imported_root.u8string());
    REQUIRE(duplicate.success());
    REQUIRE(duplicate.skin_names.size() == 1u);
    CHECK(duplicate.skin_names[0] == "Primary (2)");
    CHECK(std::filesystem::is_regular_file(imported_root / "Primary (2)" / "play.lr2skin"));
    std::ifstream kept(imported_root / "Primary" / "keep.txt", std::ios::binary);
    std::string kept_text;
    kept >> kept_text;
    CHECK(kept_text == "original");
}

TEST_CASE("lr2 importer keeps a direct skin with nested screen files as one unit") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto direct = temp.path / "Direct";
    const auto imported_root = temp.path / "imported";
    std::filesystem::create_directories(direct / "Play");
    std::filesystem::create_directories(direct / "Result");
    write_file(direct / "Play" / "play.lr2skin", "#INFORMATION,0,Direct Play,Tester\n");
    write_file(direct / "Result" / "result.lr2skin", "#INFORMATION,7,Direct Result,Tester\n");

    const auto imported = tenriff::app::import_lr2_skin_tree(
        direct.u8string(), imported_root.u8string());
    REQUIRE(imported.success());
    REQUIRE(imported.skin_names.size() == 1u);
    CHECK(imported.skin_names[0] == "Direct");
    CHECK(std::filesystem::is_regular_file(imported_root / "Direct" / "Play" / "play.lr2skin"));
    CHECK(std::filesystem::is_regular_file(imported_root / "Direct" / "Result" / "result.lr2skin"));
    CHECK_FALSE(std::filesystem::exists(imported_root / "Play"));
}

TEST_CASE("lr2 skin resolver follows includes, custom files, and note destinations") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "Barebones";
    std::filesystem::create_directories(skin / "csv");
    std::filesystem::create_directories(skin / "img" / "notes");

    write_file(skin / "barebones.lr2skin",
               "#INFORMATION,0,Barebones,Tester\n"
               "#CUSTOMOPTION,Side,920,1P,2P\n"
               "#CUSTOMFILE,Note,LR2Files\\Theme\\Barebones\\img\\notes\\*.png,default\n"
               "#ENDOFHEADER\n"
               "#IF,920\n"
               "#INCLUDE,LR2Files\\Theme\\Barebones\\csv\\1p.csv\n"
               "#ENDIF\n");
    write_file(skin / "csv" / "1p.csv",
               "#IMAGE,LR2Files\\Theme\\Barebones\\img\\notes\\*.png\n"
               "#SRC_NOTE,0,0,0,56,30,8,1,1,0,0\n"
               "#SRC_NOTE,1,0,31,56,20,8,1,1,0,0\n"
               "#SRC_NOTE,2,0,52,56,20,8,1,1,0,0\n"
               "#SRC_NOTE,3,0,73,56,30,8,1,1,0,0\n"
               "#SRC_LN_START,0,0,0,94,30,14,1,1,0,0\n"
               "#SRC_LN_BODY,0,0,0,1,30,48,1,1,0,0\n"
               "#SRC_LN_END,0,0,0,80,30,14,1,1,0,0\n"
               "#DST_NOTE,0,0,100,400,30,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,1,0,132,400,20,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,2,0,154,400,20,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,3,0,186,400,30,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n");
    write_file(skin / "img" / "notes" / "default.png");

    const auto resolved = tenriff::app::resolve_lr2_play_skin(root.u8string(), "Barebones", 4);
    REQUIRE(resolved.found);
    CHECK(resolved.keys == 4);
    REQUIRE(resolved.note_images.size() == 4u);
    CHECK(resolved.note_images[0].path.find("default.png") != std::string::npos);
    CHECK(resolved.note_images[0].has_source_rect);
    CHECK(resolved.note_images[1].source_x == doctest::Approx(31.0f));
    REQUIRE(resolved.hold_head_images.size() == 4u);
    CHECK(resolved.hold_head_images[0].path.find("default.png") != std::string::npos);
    REQUIRE(resolved.lane_divider_widths.size() == 3u);
    CHECK(resolved.lane_divider_widths[0] == doctest::Approx(2.0f));
    CHECK(resolved.lane_divider_widths[1] == doctest::Approx(2.0f));
    CHECK(resolved.lane_divider_widths[2] == doctest::Approx(12.0f));
    CHECK(resolved.imported_note_width_ratio == doctest::Approx(25.0f / 30.0f));
    CHECK(resolved.imported_note_height_ratio == doctest::Approx(8.0f / 22.0f));
    REQUIRE(resolved.key_images.size() == 4u);
    CHECK(resolved.key_images[0].path.empty());
    CHECK(resolved.key_pressed_images[0].path.empty());
    CHECK(resolved.gear_overlay_image.path.empty());
}

TEST_CASE("lr2 skin image slots survive missing assets and include boundaries") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "StableSlots";
    std::filesystem::create_directories(skin / "csv");
    std::filesystem::create_directories(skin / "img");

    write_file(skin / "play.lr2skin",
               "#INFORMATION,0,Stable Slots,Tester\n"
               "#ENDOFHEADER\n"
               "#IMAGE,LR2Files\\Theme\\StableSlots\\img\\missing.png\n"
               "#INCLUDE,LR2Files\\Theme\\StableSlots\\csv\\included.csv\n"
               "#IMAGE,LR2Files\\Theme\\StableSlots\\img\\parent.png\n"
               "#SRC_NOTE,1,2,30,0,30,22,1,1,0,0\n"
               "#DST_NOTE,1,0,140,400,30,22,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n");
    write_file(skin / "csv" / "included.csv",
               "#IMAGE,LR2Files\\Theme\\StableSlots\\img\\included.png\n"
               "#SRC_NOTE,0,1,0,0,30,22,1,1,0,0\n"
               "#DST_NOTE,0,0,100,400,30,22,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n");
    write_file(skin / "img" / "included.png");
    write_file(skin / "img" / "parent.png");

    const auto resolved = tenriff::app::resolve_lr2_play_skin(
        root.u8string(), "StableSlots", 2);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 2u);
    CHECK(resolved.note_images[0].path.find("included.png") != std::string::npos);
    CHECK(resolved.note_images[1].path.find("parent.png") != std::string::npos);
}

TEST_CASE("lr2 skin resolver handles multi-option branches, customfile directories, and normalized lane order") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "Realish";
    std::filesystem::create_directories(skin / "play" / "Note" / "Circle-KB");

    write_file(skin / "play_7.lr2skin",
               "#INFORMATION,0,Realish 7KEY,Tester\n"
               "#CUSTOMOPTION,PlaySide,900,1P,2P\n"
               "#CUSTOMOPTION,Turntable,905,LEFT,RIGHT\n"
               "#CUSTOMOPTION,LaneSize,910,NORMAL,WIDE\n"
               "#CUSTOMFILE,Note,LR2Files\\Theme\\Realish\\play\\Note\\*,Circle-KB\n"
               "#ENDOFHEADER\n"
               "#SETOPTION,989,1\n"
               "#IF,900,905,910,989\n"
               "#INCLUDE,LR2Files\\Theme\\Realish\\play\\layout.csv\n"
               "#ENDIF\n");
    write_file(skin / "play" / "layout.csv",
               "#IMAGE,LR2Files\\Theme\\Realish\\play\\Note\\*\\Note.png\n"
               "#SRC_NOTE,10,0,0,0,30,8,1,1,0,0\n"
               "#SRC_NOTE,11,0,30,0,20,8,1,1,0,0\n"
               "#SRC_NOTE,12,0,60,0,20,8,1,1,0,0\n"
               "#DST_NOTE,10,0,100,400,30,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,12,0,132,400,20,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,11,0,154,400,20,8,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n");
    write_file(skin / "play" / "Note" / "Circle-KB" / "Note.png");

    const auto resolved = tenriff::app::resolve_lr2_play_skin(root.u8string(), "Realish", 3);
    REQUIRE(resolved.found);
    CHECK(resolved.keys == 3);
    REQUIRE(resolved.note_images.size() == 3u);
    CHECK(resolved.note_images[0].path.find("Circle-KB") != std::string::npos);
    CHECK(resolved.note_images[0].source_x == doctest::Approx(0.0f));
    CHECK(resolved.note_images[1].source_x == doctest::Approx(60.0f));
    CHECK(resolved.note_images[2].source_x == doctest::Approx(30.0f));
    REQUIRE(resolved.lane_divider_widths.size() == 2u);
    CHECK(resolved.lane_divider_widths[0] == doctest::Approx(2.0f));
    CHECK(resolved.lane_divider_widths[1] == doctest::Approx(2.0f));
}

TEST_CASE("lr2 gear wildcard include supplies full-lane receptor art") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    const auto skin = root / "GearSkin";
    std::filesystem::create_directories(skin / "play" / "Gear" / "Default");
    std::filesystem::create_directories(skin / "play" / "Note");

    write_file(skin / "play_7.lr2skin",
               "#INFORMATION,0,Gear Skin,Tester\n"
               "#RESOLUTION,1\n"
               "#CUSTOMOPTION,PlaySide,900,1P\n"
               "#CUSTOMFILE,Gear,LR2files\\Theme\\GearSkin\\play\\Gear\\*,Default\n"
               "#ENDOFHEADER\n"
               "#IMAGE,LR2files\\Theme\\GearSkin\\play\\Gear\\*\\Gear.png\n"
               "#IMAGE,LR2files\\Theme\\GearSkin\\play\\Note\\Note.png\n"
               "#INCLUDE,LR2files\\Theme\\GearSkin\\play\\layout.csv\n"
               "#INCLUDE,LR2files\\Theme\\GearSkin\\play\\Gear\\*\\Gear.csv\n");
    write_file(skin / "play" / "layout.csv",
               "#SRC_NOTE,0,1,0,0,30,20,1,1,0,0\n"
               "#SRC_NOTE,1,1,30,0,30,20,1,1,0,0\n"
               "#DST_NOTE,0,0,100,400,30,20,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n"
               "#DST_NOTE,1,0,130,400,30,20,0,255,255,255,255,0,0,0,0,0,0,0,0,0\n");
    write_file(skin / "play" / "Gear" / "Default" / "Gear.csv",
               "#SRC_IMAGE,0,0,0,0,80,480,1,1,0,0,0,0,0\n"
               "#DST_IMAGE,0,0,90,0,80,480,0,255,255,255,255,1,0,0,0,0,0,0,0,0\n");
    write_file(skin / "play" / "Gear" / "Default" / "Gear.png");
    write_file(skin / "play" / "Note" / "Note.png");

    const auto resolved = tenriff::app::resolve_lr2_play_skin(root.u8string(), "GearSkin", 2);
    REQUIRE(resolved.found);
    REQUIRE(resolved.note_images.size() == 2u);
    CHECK(resolved.note_images[0].path.find("Note.png") != std::string::npos);
    REQUIRE(resolved.key_images.size() == 2u);
    CHECK(resolved.key_images[0].path.find("Gear.png") != std::string::npos);
    CHECK(resolved.key_images[0].source_x == doctest::Approx(10.0f));
    CHECK(resolved.key_images[0].source_y == doctest::Approx(400.0f));
    CHECK(resolved.key_images[0].source_width == doctest::Approx(30.0f));
    CHECK(resolved.key_images[0].source_height == doctest::Approx(80.0f));
    CHECK(resolved.use_full_lane_receptor_layout);
    CHECK(resolved.gear_overlay_image.path.find("Gear.png") != std::string::npos);
    CHECK(resolved.gear_overlay_image.has_source_rect);
    CHECK(resolved.gear_overlay_image.source_x == doctest::Approx(0.0f));
    CHECK(resolved.gear_overlay_image.source_y == doctest::Approx(0.0f));
    CHECK(resolved.gear_overlay_image.source_width == doctest::Approx(80.0f));
    CHECK(resolved.gear_overlay_image.source_height == doctest::Approx(480.0f));
}
TEST_CASE("lr2 skin resolver honors explicit resolution headers") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    write_simple_lr2_skin(root, "HdExplicit", "note.png", "#RESOLUTION,1", 320.0f, 420.0f, 60.0f, 33.0f);
    write_simple_lr2_skin(root, "FhdExplicit", "note.png", "#RESOLUTION,2", 320.0f, 420.0f, 90.0f, 49.5f);

    const auto hd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "HdExplicit", 2, "auto");
    REQUIRE(hd.found);
    CHECK(hd.resolution_family == tenriff::app::Lr2ResolutionFamily::Hd);
    CHECK(hd.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(hd.imported_note_height_ratio == doctest::Approx(1.0f));

    const auto fhd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "FhdExplicit", 2, "auto");
    REQUIRE(fhd.found);
    CHECK(fhd.resolution_family == tenriff::app::Lr2ResolutionFamily::Fhd);
    CHECK(fhd.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(fhd.imported_note_height_ratio == doctest::Approx(1.0f));
}

TEST_CASE("lr2 skin resolver auto-detects sd hd and fhd from dst note positions") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    write_simple_lr2_skin(root, "SdAuto", "note.png", "", 640.0f, 700.0f, 30.0f, 22.0f);
    write_simple_lr2_skin(root, "HdAuto", "note.png", "", 1280.0f, 1340.0f, 60.0f, 33.0f);
    write_simple_lr2_skin(root, "FhdAuto", "note.png", "", 1920.0f, 2010.0f, 90.0f, 49.5f);

    const auto sd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "SdAuto", 2, "auto");
    REQUIRE(sd.found);
    CHECK(sd.resolution_family == tenriff::app::Lr2ResolutionFamily::Sd);
    CHECK(sd.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(sd.imported_note_height_ratio == doctest::Approx(1.0f));

    const auto hd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "HdAuto", 2, "auto");
    REQUIRE(hd.found);
    CHECK(hd.resolution_family == tenriff::app::Lr2ResolutionFamily::Hd);
    CHECK(hd.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(hd.imported_note_height_ratio == doctest::Approx(1.0f));

    const auto fhd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "FhdAuto", 2, "auto");
    REQUIRE(fhd.found);
    CHECK(fhd.resolution_family == tenriff::app::Lr2ResolutionFamily::Fhd);
    CHECK(fhd.imported_note_width_ratio == doctest::Approx(1.0f));
    CHECK(fhd.imported_note_height_ratio == doctest::Approx(1.0f));
}

TEST_CASE("lr2 skin resolver override takes precedence over auto detection") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    write_simple_lr2_skin(root, "OverrideHd", "note.png", "", 1280.0f, 1340.0f, 60.0f, 33.0f);
    write_simple_lr2_skin(root, "OverrideSd", "note.png", "", 640.0f, 700.0f, 30.0f, 22.0f);

    const auto forced_sd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "OverrideHd", 2, "sd");
    REQUIRE(forced_sd.found);
    CHECK(forced_sd.resolution_family == tenriff::app::Lr2ResolutionFamily::Sd);

    const auto forced_fhd = tenriff::app::resolve_lr2_play_skin(root.u8string(), "OverrideSd", 2, "fhd");
    REQUIRE(forced_fhd.found);
    CHECK(forced_fhd.resolution_family == tenriff::app::Lr2ResolutionFamily::Fhd);
}

TEST_CASE("lr2 skin resolver ignores hd asset names when auto-detecting layout family") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto root = temp.path / "skins";
    write_simple_lr2_skin(root, "NameTrap", "Snow_HD.png", "", 640.0f, 700.0f, 30.0f, 22.0f);

    const auto resolved = tenriff::app::resolve_lr2_play_skin(root.u8string(), "NameTrap", 2, "auto");
    REQUIRE(resolved.found);
    CHECK(resolved.resolution_family == tenriff::app::Lr2ResolutionFamily::Sd);
    CHECK(resolved.note_images[0].path.find("Snow_HD.png") != std::string::npos);
}

TEST_CASE("lr2 external Theme sample resolves independent non-IIDX playskins when configured") {
    const char* configured_root = std::getenv("TENRIFF_LR2_THEME_ROOT");
    if (configured_root == nullptr || *configured_root == '\0') {
        return;
    }

    const std::filesystem::path root(configured_root);
    REQUIRE(std::filesystem::is_directory(root));
    const auto names = tenriff::app::list_lr2_skin_names(root.u8string());
    CHECK(names.size() >= 5u);
    CHECK(std::find(names.begin(), names.end(), "LR2") != names.end());
    CHECK(std::find(names.begin(), names.end(), "WMIX_HD") != names.end());

    const auto standard = tenriff::app::resolve_lr2_play_skin(root.u8string(), "LR2", 8);
    REQUIRE(standard.found);
    CHECK(standard.keys == 8);
    CHECK(std::any_of(standard.note_images.begin(), standard.note_images.end(), [](const auto& asset) {
        return !asset.path.empty();
    }));

    const auto alternate = tenriff::app::resolve_lr2_play_skin(root.u8string(), "WMIX_HD", 8);
    REQUIRE(alternate.found);
    CHECK(alternate.keys == 8);
    CHECK(std::any_of(alternate.note_images.begin(), alternate.note_images.end(), [](const auto& asset) {
        return !asset.path.empty();
    }));

    if (std::find(names.begin(), names.end(), "FT") != names.end()) {
        const auto ft = tenriff::app::resolve_lr2_play_skin(root.u8string(), "FT", 8);
        REQUIRE(ft.found);
        REQUIRE(ft.note_images.size() == 8u);
        CHECK(ft.note_images[0].path.find("Note.png") != std::string::npos);
        CHECK(ft.note_images[0].path.find("Numbers.png") == std::string::npos);
        REQUIRE(ft.key_images.size() == 8u);
        CHECK(ft.key_images[0].path.find("Gear.png") != std::string::npos);
        CHECK(ft.use_full_lane_receptor_layout);
    }
}

TEST_CASE("lr2 sample FT skin resolves note directory customfile patterns") {
    const auto sample_root = find_repo_lr2_sample_root();
    if (sample_root.empty()) {
        return;
    }

    const auto resolved = tenriff::app::resolve_lr2_play_skin(sample_root.u8string(), "FT", 8);
    REQUIRE(resolved.found);
    CHECK(resolved.keys == 8);
    CHECK(resolved.resolution_family == tenriff::app::Lr2ResolutionFamily::Hd);
    REQUIRE(resolved.note_images.size() == 8u);
    CHECK_FALSE(resolved.note_images[0].path.empty());
    CHECK(resolved.note_images[0].has_source_rect);
}

TEST_CASE("lr2 sample standard skin resolves multi-option 7key branch") {
    const auto sample_root = find_repo_lr2_sample_root();
    if (sample_root.empty()) {
        return;
    }

    const auto resolved = tenriff::app::resolve_lr2_play_skin(sample_root.u8string(), "LR2", 8);
    REQUIRE(resolved.found);
    CHECK(resolved.keys == 8);
    CHECK(resolved.resolution_family == tenriff::app::Lr2ResolutionFamily::Sd);
    REQUIRE(resolved.note_images.size() == 8u);
    CHECK(resolved.note_images[0].path.find("LR2 default") != std::string::npos);
    CHECK(resolved.note_images[0].has_source_rect);
}
