#include "doctest/doctest.h"

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
    CHECK(resolved.key_images[0].path.find("default.png") != std::string::npos);
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

TEST_CASE("lr2 sample FT skin resolves note directory customfile patterns") {
    const auto sample_root = find_repo_lr2_sample_root();
    if (sample_root.empty()) {
        return;
    }

    const auto resolved = tenriff::app::resolve_lr2_play_skin(sample_root.u8string(), "FT", 8);
    REQUIRE(resolved.found);
    CHECK(resolved.keys == 8);
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
    REQUIRE(resolved.note_images.size() == 8u);
    CHECK(resolved.note_images[0].path.find("LR2 default") != std::string::npos);
    CHECK(resolved.note_images[0].has_source_rect);
}
