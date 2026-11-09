#include "doctest/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "app/ImportedGameplaySkin.h"
#include "app/LanePresentationLayout.h"
#include "app/MenuAppSkinUtils.h"
#include "app/TenRiffSkin.h"
#include "render/NativeGameplayOverrides.h"
#include <limits>

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
    const auto base = std::filesystem::temp_directory_path() / "tenriff_skin_manifest_tests";
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

void write_file(const std::filesystem::path& path, const std::string& content = "png") {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << content;
}

void write_manifest_skin(const std::filesystem::path& root) {
    write_file(root / "lobby" / "background.png");
    write_file(root / "lobby" / "logo.png");
    write_file(root / "gameplay" / "note.png");
    write_file(root / "gameplay" / "key.png");
    write_file(root / "gameplay" / "background.jpg");
    write_file(root / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Aurora Glass\",\n"
               "  \"author\": \"Tester\",\n"
               "  \"lobby\": {\n"
               "    \"background\": \"lobby/background.png\",\n"
               "    \"logo\": \"lobby/logo.png\",\n"
               "    \"background_opacity\": 0.5\n"
               "  },\n"
               "  \"gameplay\": {\n"
               "    \"background\": \"gameplay/background.jpg\",\n"
               "    \"background_opacity\": 0.4,\n"
               "    \"note\": \"gameplay/note.png\",\n"
               "    \"key_idle\": \"gameplay/key.png\",\n"
               "    \"key_pressed\": \"gameplay/key.png\",\n"
               "    \"note_width_ratio\": 1.25,\n"
               "    \"note_height_ratio\": 0.8,\n"
               "    \"judgement_line_position\": 0.84\n"
               "  }\n"
               "}\n");
}

}  // namespace

TEST_CASE("TenRiff skin manifest resolves lobby and gameplay assets") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Aurora";
    write_manifest_skin(skin);

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    CHECK(loaded.format_version == tenriff::app::kTenRiffSkinFormatVersion);
    CHECK(loaded.name == "Aurora Glass");
    CHECK(loaded.author == "Tester");
    CHECK(!loaded.lobby_background_path.empty());
    CHECK(!loaded.lobby_logo_path.empty());
    CHECK(loaded.lobby_background_opacity == doctest::Approx(0.5f));
    CHECK(!loaded.gameplay_background_path.empty());
    CHECK(loaded.gameplay_background_opacity == doctest::Approx(0.4f));
    REQUIRE(loaded.gameplay.note_images.size() == 1u);
    CHECK(!loaded.gameplay.note_images.front().path.empty());
    CHECK(loaded.gameplay.imported_note_width_ratio == doctest::Approx(1.25f));
    CHECK(loaded.gameplay.imported_note_height_ratio == doctest::Approx(0.8f));
    CHECK(loaded.gameplay.has_hit_position);
    CHECK(loaded.referenced_asset_paths.size() == 5u);
    CHECK(loaded.layout_rects.empty());
    CHECK_FALSE(loaded.native_menu_renderer);
    CHECK_FALSE(loaded.native_gameplay_fallback);
    CHECK(loaded.native_menu.metrics.empty());
}

TEST_CASE("TenRiff native menu style loads typed offsets and imports replacement assets") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "NativeEditable";
    write_file(skin / "menu" / "emblem.png");
    write_file(skin / "skin.json", R"({
      "format": "tenriff-skin", "version": 1, "name": "Native Editable",
      "lobby": {"renderer": "native"},
      "native": {
        "metrics": {"font.title.size": 36, "panel.offset": -24},
        "colors": {"accent": "#63E9F280"},
        "rects": {"title.button": [10, -5, 32, -8], "title.logo": [0, 0, 0, 0]},
        "assets": {"emblem": "menu/emblem.png"},
        "motion": {"enabled": 0, "speed": 1.5},
        "fonts": {"body": "Yu Gothic UI"}
      }
    })");
    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 10);
    REQUIRE(loaded.found);
    CHECK(loaded.native_menu_renderer);
    CHECK(loaded.native_gameplay_fallback);
    CHECK(loaded.warnings.empty());
    CHECK(loaded.native_menu.metrics.at("font.title.size") == doctest::Approx(36));
    CHECK(loaded.native_menu.metrics.at("panel.offset") == doctest::Approx(-24));
    CHECK(loaded.native_menu.colors.at("accent")[3] == doctest::Approx(128.0 / 255.0));
    CHECK(loaded.native_menu.rects.at("title.button")[3] == doctest::Approx(-8));
    CHECK(loaded.native_menu.rects.count("title.logo") == 1u);
    CHECK(loaded.native_menu.motion.at("enabled") == doctest::Approx(0));
    CHECK(loaded.native_menu.motion.at("speed") == doctest::Approx(1.5));
    CHECK(loaded.native_menu.fonts.at("body") == "Yu Gothic UI");
    CHECK(loaded.referenced_asset_paths.size() == 1u);
    CHECK(loaded.gameplay.note_images.empty());
    CHECK_FALSE(loaded.gameplay_style.note_shape.has_value());
    CHECK_FALSE(loaded.gameplay.has_hit_position);

    const auto imported = tenriff::app::import_tenriff_skin(skin.u8string(),
                                                          (temp.path / "imports").u8string());
    REQUIRE(imported.success());
    const auto reloaded = tenriff::app::resolve_tenriff_skin(imported.install_root,
                                                          imported.skin_name, 10);
    REQUIRE(reloaded.found);
    CHECK(reloaded.warnings.empty());
    REQUIRE(reloaded.native_menu.assets.count("emblem") == 1u);
    CHECK(std::filesystem::is_regular_file(
        std::filesystem::u8path(reloaded.native_menu.assets.at("emblem"))));
    CHECK(reloaded.native_menu.assets.at("emblem") != loaded.native_menu.assets.at("emblem"));
    CHECK(reloaded.native_gameplay_fallback);
}

TEST_CASE("TenRiff native menu-only skins keep native receptors until gameplay is customized") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "MenuOnly";
    write_file(skin / "gameplay" / "custom.png");
    auto load_gameplay = [&](const std::string& fields, int keys = 10) {
        write_file(skin / "skin.json",
            "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Menu Only\","
            "\"lobby\":{\"renderer\":\"native\"},\"gameplay\":{" + fields + "}}");
        return tenriff::app::load_tenriff_skin_folder(skin.u8string(), keys);
    };
    const auto empty = load_gameplay("");
    REQUIRE(empty.found);
    CHECK(empty.native_gameplay_fallback);
    CHECK(empty.warnings.empty());
    CHECK(load_gameplay("\"note_width_ratio\":1,\"note_height_ratio\":1").native_gameplay_fallback);
    // Presence of optional false/zero values still overrides a player setting.
    for (const std::string field : {
             "\"show_lane_dividers\":false", "\"show_judgement_line\":false",
             "\"show_timing_feedback\":false", "\"show_gear_boundary_line\":false",
             "\"show_hold_tail\":false", "\"hold_tail_taper\":false",
             "\"judgement_line_glow\":false", "\"key_pulse\":false",
             "\"note_border\":false", "\"black_playfield\":false",
             "\"key_pulse_brightness\":0", "\"key_backdrop\":false", "\"key_backdrop_opacity\":0",
             "\"key_backdrop_brightness\":0", "\"key_backdrop_height\":0", "\"lane_background_opacity\":0",
             "\"visual_opacity\":1", "\"note_outline_opacity\":0",
             "\"hold_body_opacity\":1", "\"hold_body_opacity\":0", "\"hit_burst_style\":\"ring\"",
             "\"key_label_position\":\"off\"", "\"note_shape\":\"rect\"",
             "\"lane_colors\":[\"#FFFFFF\"]", "\"note_width_ratio\":1.2",
             "\"note_height_ratio\":1.2", "\"full_lane_receptors\":true",
             "\"note_aspect\":\"contain\"", "\"judgement_line_position\":0.82",
             "\"column_widths\":[40]", "\"column_spacings\":[1]",
             "\"note_rotations\":[90]", "\"key_rotations\":[90]",
             "\"background\":\"gameplay/custom.png\"", "\"gear\":\"gameplay/custom.png\"",
             "\"note\":\"gameplay/custom.png\"", "\"hold_head\":\"gameplay/custom.png\"",
             "\"hold_body\":\"gameplay/custom.png\"", "\"hold_tail\":\"gameplay/custom.png\"",
             "\"key_idle\":\"gameplay/custom.png\"", "\"key_pressed\":\"gameplay/custom.png\""}) {
        const auto customized = load_gameplay(field);
        CHECK(customized.warnings.empty());
        CHECK_FALSE(customized.native_gameplay_fallback);
    }
    const std::string modes = "\"modes\":{\"7k\":{\"note\":\"gameplay/custom.png\"}}";
    CHECK(load_gameplay(modes, 4).native_gameplay_fallback);
    CHECK_FALSE(load_gameplay(modes, 7).native_gameplay_fallback);
    write_file(skin / "gameplay" / "note.png");
    CHECK_FALSE(load_gameplay("").native_gameplay_fallback);
}

TEST_CASE("TenRiff note height ratios accept 50 through 400 percent with safe invalid fallback") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "HeightBounds";
    const auto load_ratio = [&](double ratio, bool per_mode) {
        const std::string value = "\"note_height_ratio\":" + std::to_string(ratio);
        write_file(skin / "skin.json",
            "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Height\",\"gameplay\":{" +
            (per_mode ? "\"modes\":{\"16k\":{" + value + "}}" : value) + "}}");
        return tenriff::app::load_tenriff_skin_folder(skin.u8string(), 16);
    };
    for (bool per_mode : {false, true}) {
        for (double valid : {0.5, 1.0, 4.0}) {
            const auto loaded = load_ratio(valid, per_mode);
            REQUIRE(loaded.found);
            CHECK(loaded.warnings.empty());
            CHECK(loaded.gameplay.imported_note_height_ratio == doctest::Approx(valid));
        }
        for (double invalid : {0.49, 4.01}) {
            const auto loaded = load_ratio(invalid, per_mode);
            REQUIRE(loaded.found);
            CHECK(loaded.gameplay.imported_note_height_ratio == doctest::Approx(1.0));
            CHECK(std::any_of(loaded.warnings.begin(), loaded.warnings.end(), [](const auto& warning) {
                return warning.find("note_height_ratio") != std::string::npos;
            }));
        }
    }
}

TEST_CASE("TenRiff native menu invalid fields fall back without leaking out of the skin") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "InvalidNative";
    write_file(temp.path / "outside.png");
    write_file(skin / "skin.json", R"({
      "format": "tenriff-skin", "version": 1, "name": "Invalid Native",
      "lobby": {"renderer": "unknown"},
      "native": {
        "metrics": {"size": "big", "overflow": 8193, "Invalid name": 10},
        "colors": {"accent": "cyan"},
        "rects": {"title.button": [1, 2, 3], "title.logo": [0, 0, 0, 8193]},
        "assets": {"emblem": "../outside.png", "bad": 42},
        "motion": {"speed": -1, "duration": 121},
        "fonts": {"body": "", "heading": "bad\nfont"}
      }
    })");
    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 10);
    REQUIRE(loaded.found);
    CHECK_FALSE(loaded.native_menu_renderer);
    CHECK(loaded.native_menu.metrics.empty());
    CHECK(loaded.native_menu.colors.empty());
    CHECK(loaded.native_menu.rects.empty());
    CHECK(loaded.native_menu.assets.empty());
    CHECK(loaded.native_menu.motion.empty());
    CHECK(loaded.native_menu.fonts.empty());
    CHECK(loaded.referenced_asset_paths.empty());
    CHECK(loaded.warnings.size() == 13u);
}

TEST_CASE("TenRiff native menu objects are optional and reject malformed categories") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "NativeCategories";
    write_file(skin / "skin.json", R"({
      "format": "tenriff-skin", "version": 1, "name": "Native Categories",
      "lobby": {"renderer": "legacy"},
      "native": {"metrics": [], "colors": 4, "rects": null, "assets": false,
                 "motion": "fast", "fonts": [], "typo": {}}
    })");
    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 10);
    REQUIRE(loaded.found);
    CHECK_FALSE(loaded.native_menu_renderer);
    CHECK(loaded.warnings.size() == 7u);
}

TEST_CASE("TenRiff skin arrow options parse aspect mode and per-lane rotations") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Arrows";
    write_file(skin / "gameplay" / "arrow.png");
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Arrows\",\n"
               "  \"gameplay\": {\n"
               "    \"note\": \"gameplay/arrow.png\",\n"
               "    \"note_aspect\": \"width\",\n"
               "    \"note_rotations\": [270, 180, 0, -90]\n"
               "  }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    CHECK(loaded.gameplay.note_aspect == "width");
    REQUIRE(loaded.gameplay.note_rotations.size() == 4u);
    CHECK(loaded.gameplay.note_rotations[0] == doctest::Approx(270.0f));
    CHECK(loaded.gameplay.note_rotations[1] == doctest::Approx(180.0f));
    CHECK(loaded.gameplay.note_rotations[2] == doctest::Approx(0.0f));
    // Negative degrees wrap into [0, 360) so the renderer never sees a sign.
    CHECK(loaded.gameplay.note_rotations[3] == doctest::Approx(270.0f));
    // key_rotations stays empty; the renderer falls back to note_rotations.
    CHECK(loaded.gameplay.key_rotations.empty());
    CHECK(loaded.warnings.empty());
}

TEST_CASE("TenRiff skin rejects an unknown note_aspect and keeps the default") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "BadAspect";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"BadAspect\",\n"
               "  \"gameplay\": { \"note_aspect\": \"squish\" }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    CHECK(loaded.gameplay.note_aspect.empty());
    CHECK(loaded.warnings.size() == 1u);
}

TEST_CASE("TenRiff skin layout keeps known slots and drops malformed ones") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Layout";
    std::filesystem::create_directories(skin);
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Layout\",\n"
               "  \"layout\": {\n"
               "    \"song_select\": {\n"
               "      \"center_panel\": [1126, 152, 1882, 922],\n"
               "      \"avatar\": [1530, 26, 1606, 102],\n"
               "      \"left_panel\": [486, 152, 38, 922],\n"
               "      \"sidebar\": [0, 0, 10, 10]\n"
               "    },\n"
               "    \"title\": { \"buttons\": [470, 360, 1450] }\n"
               "  }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    REQUIRE(loaded.layout_rects.count("song_select.center_panel") == 1u);
    REQUIRE(loaded.layout_rects.count("song_select.avatar") == 1u);
    const auto& rect = loaded.layout_rects.at("song_select.center_panel");
    CHECK(rect.left == doctest::Approx(1126.0f));
    CHECK(rect.top == doctest::Approx(152.0f));
    CHECK(rect.right == doctest::Approx(1882.0f));
    CHECK(rect.bottom == doctest::Approx(922.0f));

    // right <= left, an unknown slot name, and a three-number rect are all dropped.
    CHECK(loaded.layout_rects.count("song_select.left_panel") == 0u);
    CHECK(loaded.layout_rects.count("song_select.sidebar") == 0u);
    CHECK(loaded.layout_rects.count("title.buttons") == 0u);
    CHECK(loaded.layout_rects.size() == 2u);
    CHECK(loaded.warnings.size() == 3u);
}

TEST_CASE("TenRiff skin assets cannot escape their manifest folder") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Unsafe";
    std::filesystem::create_directories(skin);
    write_file(temp.path / "outside.png");
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Unsafe\",\n"
               "  \"gameplay\": { \"note\": \"../outside.png\" }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 7);
    REQUIRE(loaded.found);
    REQUIRE(loaded.gameplay.note_images.size() == 1u);
    CHECK(loaded.gameplay.note_images.front().path.empty());
    CHECK(!loaded.warnings.empty());
}

TEST_CASE("TenRiff skin import is portable and never overwrites an existing install") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto source = temp.path / "source";
    const auto imports = temp.path / "profiles" / "tester" / "skins" / "tenriff";
    write_manifest_skin(source);

    const auto first = tenriff::app::import_tenriff_skin(source.u8string(), imports.u8string());
    REQUIRE(first.success());
    CHECK(first.skin_name == "Aurora-Glass");
    CHECK(first.copied_files == 6u);
    CHECK(std::filesystem::is_regular_file(imports / first.skin_name / "skin.json"));

    const auto imported = tenriff::app::resolve_tenriff_skin(imports.u8string(), first.skin_name, 4);
    REQUIRE(imported.found);
    const auto runtime = tenriff::app::resolve_imported_gameplay_skin(
        "tenriff", imports.u8string(), first.skin_name, 4);
    REQUIRE(runtime.found);
    CHECK(runtime.keys == 4);
    REQUIRE(runtime.note_images.size() == 1u);

    const auto second = tenriff::app::import_tenriff_skin(source.u8string(), imports.u8string());
    REQUIRE(second.success());
    CHECK(second.skin_name == "Aurora-Glass-2");
    const auto names = tenriff::app::list_tenriff_skin_names(imports.u8string());
    CHECK(names.size() == 2u);
}

TEST_CASE("TenRiff skin catalog merges bundled skins behind profile overrides") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto profile_root = temp.path / "profile";
    const auto bundled_root = temp.path / "bundled";

    write_file(profile_root / "Shared" / "skin.json",
               "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Profile Shared\"}");
    write_file(profile_root / "Personal" / "skin.json",
               "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Personal\"}");
    write_file(bundled_root / "Shared" / "skin.json",
               "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Bundled Shared\"}");
    write_file(bundled_root / "Factory" / "skin.json",
               "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Factory\"}");

    const auto catalog = tenriff::app::catalog_tenriff_skins(
        {profile_root.u8string(), bundled_root.u8string()});
    REQUIRE(catalog.names.size() == 3u);
    CHECK(catalog.names[0] == "Factory");
    CHECK(catalog.names[1] == "Personal");
    CHECK(catalog.names[2] == "Shared");
    CHECK(catalog.roots_by_name.at("Shared") == profile_root.u8string());
    CHECK(catalog.roots_by_name.at("Factory") == bundled_root.u8string());
}

TEST_CASE("bundled TenRiff skin root exposes the shipped catalog") {
    const std::string root = tenriff::app::find_bundled_tenriff_skin_root();
    REQUIRE(!root.empty());
    const auto names = tenriff::app::list_tenriff_skin_names(root);
    CHECK(std::find(names.begin(), names.end(), "TenRiff_AgentPrism_Universal_1K-16K") !=
          names.end());
    CHECK(std::find(names.begin(), names.end(), "TenRiff-Example") == names.end());
}

TEST_CASE("TenRiff skin detects conventional assets without manifest paths") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Convention";
    write_file(skin / "lobby" / "background.jpg");
    write_file(skin / "lobby" / "logo.png");
    write_file(skin / "gameplay" / "note.png");
    write_file(skin / "gameplay" / "hold-body.png");
    write_file(skin / "gameplay" / "key-idle.png");
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Convention\",\n"
               "  \"lobby\": {},\n"
               "  \"gameplay\": {}\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 7);
    REQUIRE(loaded.found);
    CHECK(!loaded.lobby_background_path.empty());
    CHECK(!loaded.lobby_logo_path.empty());
    REQUIRE(loaded.gameplay.note_images.size() == 1u);
    CHECK(!loaded.gameplay.note_images.front().path.empty());
    REQUIRE(loaded.gameplay.hold_body_images.size() == 1u);
    REQUIRE(loaded.gameplay.key_images.size() == 1u);
    CHECK(loaded.referenced_asset_paths.size() == 5u);
    CHECK(loaded.warnings.empty());
}

TEST_CASE("TenRiff skin expands lane patterns and active key mode overrides") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Patterns";
    for (const std::string lane : {"left", "down", "up", "right"}) {
        write_file(skin / "gameplay" / ("note-" + lane + ".png"));
    }
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Patterns\",\n"
               "  \"theme\": {\n"
               "    \"accent\": \"#123456CC\",\n"
               "    \"scene_primary\": \"#102030\",\n"
               "    \"scene_background\": \"#010203FF\"\n"
               "  },\n"
               "  \"gameplay\": {\n"
               "    \"note\": \"gameplay/note-{lane}.png\",\n"
               "    \"modes\": {\n"
               "      \"4k\": {\n"
               "        \"lane_map\": [\"left\", \"down\", \"up\", \"right\"],\n"
               "        \"lane_colors\": [\"#FF0000\", \"#00FF00\", \"#0000FF\", \"#FFFFFF\"],\n"
               "        \"note_shape\": \"diamond\",\n"
               "        \"show_lane_dividers\": false\n"
               "      }\n"
               "    }\n"
               "  }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    REQUIRE(loaded.gameplay.note_images.size() == 4u);
    for (const auto& image : loaded.gameplay.note_images) CHECK(!image.path.empty());
    REQUIRE(loaded.theme_colors.count("accent") == 1u);
    CHECK(loaded.theme_colors.at("accent")[3] == doctest::Approx(0.8f));
    CHECK(loaded.theme_colors.count("scene_primary") == 1u);
    CHECK(loaded.theme_colors.count("scene_background") == 1u);
    REQUIRE(loaded.gameplay_style.note_shape.has_value());
    CHECK(*loaded.gameplay_style.note_shape == "diamond");
    REQUIRE(loaded.gameplay_style.show_lane_dividers.has_value());
    CHECK_FALSE(*loaded.gameplay_style.show_lane_dividers);
    CHECK(loaded.gameplay_style.lane_colors.size() == 4u);
    CHECK(loaded.warnings.empty());
}

TEST_CASE("TenRiff skin warns about misspelled manifest fields") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Typos";
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Typos\",\n"
               "  \"gameplay\": { \"key_pressd\": \"missing.png\" }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    REQUIRE(loaded.warnings.size() == 1u);
    CHECK(loaded.warnings.front().find("key_pressd") != std::string::npos);
}

TEST_CASE("TenRiff skin template creates an editable directly installed skin") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto created = tenriff::app::create_tenriff_skin_template(temp.path.u8string());
    REQUIRE(created.success());
    CHECK(std::filesystem::is_regular_file(std::filesystem::u8path(created.folder_path) / "skin.json"));
    CHECK(std::filesystem::is_directory(std::filesystem::u8path(created.folder_path) / "lobby"));
    CHECK(std::filesystem::is_directory(std::filesystem::u8path(created.folder_path) / "lobby" / "screens"));
    CHECK(std::filesystem::is_directory(std::filesystem::u8path(created.folder_path) / "gameplay"));
    std::ifstream manifest_file(std::filesystem::u8path(created.folder_path) / "skin.json",
                                std::ios::binary);
    const std::string manifest((std::istreambuf_iterator<char>(manifest_file)),
                               std::istreambuf_iterator<char>());
    CHECK(manifest.find("githubusercontent.com/10kseason/TenRiff/") != std::string::npos);
    CHECK(manifest.find("krrcream-Toolkit") == std::string::npos);
    const auto loaded = tenriff::app::resolve_tenriff_skin(
        temp.path.u8string(), created.skin_name, 4);
    REQUIRE(loaded.found);
    CHECK(loaded.name == "My TenRiff Skin");
}

TEST_CASE("TenRiff skin supports conventional per-screen backgrounds and layouts") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Screens";
    write_file(skin / "lobby" / "screens" / "settings_skins.jpg");
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Screens\",\n"
               "  \"layout\": {\n"
               "    \"settings_skins\": { \"content\": [40, 100, 1500, 980] }\n"
               "  }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 10);
    REQUIRE(loaded.found);
    CHECK(loaded.screen_background_paths.count("settings_skins") == 1u);
    CHECK(loaded.layout_rects.count("settings_skins.content") == 1u);
    CHECK(loaded.warnings.empty());
}

TEST_CASE("TenRiff skin import includes assets referenced only by another key mode") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto source = temp.path / "source";
    const auto imports = temp.path / "imports";
    write_file(source / "gameplay" / "4k-note.png");
    write_file(source / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\",\n"
               "  \"version\": 1,\n"
               "  \"name\": \"Mode Assets\",\n"
               "  \"gameplay\": {\n"
               "    \"modes\": { \"4k\": { \"note\": \"gameplay/4k-note.png\" } }\n"
               "  }\n"
               "}\n");

    const auto imported = tenriff::app::import_tenriff_skin(
        source.u8string(), imports.u8string());
    REQUIRE(imported.success());
    CHECK(std::filesystem::is_regular_file(
        imports / imported.skin_name / "gameplay" / "4k-note.png"));
    const auto loaded = tenriff::app::resolve_tenriff_skin(
        imports.u8string(), imported.skin_name, 4);
    REQUIRE(loaded.found);
    REQUIRE(loaded.gameplay.note_images.size() == 1u);
    CHECK(!loaded.gameplay.note_images.front().path.empty());
}

TEST_CASE("Skin settings retain stable IDs while grouping lane selection with note color") {
    using namespace tenriff::app;
    for (const bool lr2 : {false, true}) {
        const SkinSettingsRows rows{lr2};
        CHECK(rows.count() == (lr2 ? 63 : 61));
        CHECK(rows.index_of(SkinSettingsRowId::Lr2Resolution) == (lr2 ? 4 : -1));
        CHECK(rows.index_of(SkinSettingsRowId::TargetGap) == -1);
        CHECK(rows.index_of(SkinSettingsRowId::VisualPreset) == -1);
        CHECK(rows.index_of(SkinSettingsRowId::IndicatorBackground) == -1);
        CHECK(rows.index_of(SkinSettingsRowId::ExportPreset) >= 0);
        CHECK(rows.index_of(SkinSettingsRowId::TargetLane) == -1);
        CHECK(rows.index_of(SkinSettingsRowId::LaneColor) >= 0);
        CHECK(rows.index_of(SkinSettingsRowId::UiFont) == -1);
        CHECK(rows.index_of(SkinSettingsRowId::UiFontFile) == -1);
        CHECK(rows.index_of(SkinSettingsRowId::TimingTextSize) + 1 == rows.index_of(SkinSettingsRowId::TimingTextX));
        CHECK(rows.index_of(SkinSettingsRowId::TimingTextX) + 1 == rows.index_of(SkinSettingsRowId::TimingTextY));
        CHECK(rows.index_of(SkinSettingsRowId::TimingBarSize) + 1 == rows.index_of(SkinSettingsRowId::TimingBarX));
        CHECK(rows.index_of(SkinSettingsRowId::TimingBarX) + 1 == rows.index_of(SkinSettingsRowId::TimingBarY));
        CHECK(rows.index_of(SkinSettingsRowId::JudgementX) + 1 == rows.index_of(SkinSettingsRowId::JudgementY));
        CHECK(rows.index_of(SkinSettingsRowId::ComboX) + 1 == rows.index_of(SkinSettingsRowId::ComboY));
        CHECK(rows.index_of(SkinSettingsRowId::HitBurstStyle) + 1 == rows.index_of(SkinSettingsRowId::HitBurstSize));
        CHECK(rows.index_of(SkinSettingsRowId::HitBurstSize) + 1 == rows.index_of(SkinSettingsRowId::HitBurstPalette));
        CHECK(rows.index_of(SkinSettingsRowId::HitBurstPalette) + 1 == rows.index_of(SkinSettingsRowId::HitBurstImage));
        CHECK(rows.index_of(SkinSettingsRowId::HitBurstImage) + 1 == rows.index_of(SkinSettingsRowId::ClearHitBurstImage));
        CHECK(rows.index_of(SkinSettingsRowId::JudgementLineType) + 1 == rows.index_of(SkinSettingsRowId::JudgementLineType2Color));
        CHECK(skin_settings_category(SkinSettingsRowId::TargetLane) == SkinSettingsCategory::Notes);
        CHECK(rows.index_of(SkinSettingsRowId::Back) == rows.count() - 1);
        for (const auto id : kSkinSettingsRowOrder) {
            if (!rows.visible(id)) continue;
            CHECK(rows.index_of(id) >= 0);
        }
    }
}

TEST_CASE("7+1 presentation moves only the visual scratch lane") {
    const int scratch_lane = 1;
    const auto left = tenriff::app::resolve_lane_presentation_layout(
        8, &scratch_lane, 1, "left");
    CHECK(left.seven_plus_one);
    CHECK(left.visual_lane_for_source(1) == 1);
    CHECK(left.visual_lane_for_source(8) == 8);

    const auto right = tenriff::app::resolve_lane_presentation_layout(
        8, &scratch_lane, 1, "right");
    CHECK(right.seven_plus_one);
    CHECK(right.visual_lane_for_source(1) == 8);
    CHECK(right.visual_lane_for_source(2) == 1);
    CHECK(right.visual_lane_for_source(8) == 7);
    CHECK(right.source_lane_for_visual(8) == 1);
    const std::vector<int> canonical_lanes{10, 20, 30, 40, 50, 60, 70, 80};
    const std::vector<int> expected_visual_lanes{20, 30, 40, 50, 60, 70, 80, 10};
    CHECK(tenriff::app::lane_values_in_visual_order(canonical_lanes, right) ==
          expected_visual_lanes);
    const std::vector<int> canonical_gaps{1, 2, 3, 4, 5, 6, 7};
    const std::vector<int> expected_visual_gaps{2, 3, 4, 5, 6, 7, 1};
    CHECK(tenriff::app::lane_gap_values_in_visual_order(canonical_gaps, right) ==
          expected_visual_gaps);

    const auto ordinary = tenriff::app::resolve_lane_presentation_layout(
        8, nullptr, 0, "right");
    CHECK_FALSE(ordinary.seven_plus_one);
    CHECK(ordinary.visual_lane_for_source(1) == 1);
}

TEST_CASE("TenRiff skins can override 7+1 separately from ordinary 8K") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "SevenPlusOne";
    std::filesystem::create_directories(skin);
    std::ofstream manifest(skin / "skin.json", std::ios::binary);
    REQUIRE(manifest.good());
    manifest << "{\n"
                "  \"format\": \"tenriff-skin\",\n"
                "  \"version\": 1,\n"
                "  \"name\": \"Seven Plus One\",\n"
                "  \"gameplay\": {\n"
                "    \"note_width_ratio\": 1.0,\n"
                "    \"modes\": {\n"
                "      \"8k\": { \"note_width_ratio\": 1.25 },\n"
                "      \"7+1\": { \"note_width_ratio\": 2.0 }\n"
                "    }\n"
                "  }\n"
                "}\n";
    manifest.close();

    const auto ordinary = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 8);
    const auto seven_plus_one =
        tenriff::app::load_tenriff_skin_folder(skin.u8string(), 8, "7+1");
    REQUIRE(ordinary.found);
    REQUIRE(seven_plus_one.found);
    CHECK(ordinary.gameplay.imported_note_width_ratio == doctest::Approx(1.25));
    CHECK(seven_plus_one.gameplay.imported_note_width_ratio == doctest::Approx(2.0));
}

TEST_CASE("Native gameplay manifest preserves mode entries and explicit empty sprites") {
    TempDirGuard directory{make_temp_dir()};
    REQUIRE_FALSE(directory.path.empty());
    write_file(directory.path / "skin.json",R"({"format":"tenriff-skin","version":1,"name":"Native Instrument",
      "gameplay":{"renderer":"native","note_shape":"circle","native":{
        "metrics":{"key_max_height":200,"judgement_line_width":5},"colors":{"chassis":"#12345680"},
        "motion":{"press_response":120},"fonts":{"combo":"Consolas"},"rects":{"score":[10,-20,30,40]},
        "sprites":{"note":[{"x":0,"y":0,"width":128,"height":32,"color":"#33669980"}]}
      },"modes":{"16k":{"native":{"metrics":{"key_max_height":110},"sprites":{"note":[]}}}}}})");
    const auto ten=tenriff::app::load_tenriff_skin_folder(directory.path.u8string(),10);
    REQUIRE(ten.found);
    CHECK(ten.warnings.empty());
    CHECK(ten.native_gameplay_fallback);
    CHECK(ten.gameplay.native_renderer);
    CHECK(ten.gameplay.native.metrics.at("key_max_height")==200);
    CHECK(ten.gameplay.native.fonts.at("combo")=="Consolas");
    REQUIRE(ten.gameplay.native.sprites.at("note").size()==1);
    CHECK(ten.gameplay.native.sprites.at("note")[0].color==0x336699);
    CHECK(ten.gameplay.native.sprites.at("note")[0].alpha==doctest::Approx(128.0/255));
    const auto sixteen=tenriff::app::load_tenriff_skin_folder(directory.path.u8string(),16);
    CHECK(sixteen.warnings.empty());
    CHECK(sixteen.native_gameplay_fallback);
    CHECK(sixteen.gameplay.native.metrics.at("key_max_height")==110);
    CHECK(sixteen.gameplay.native.metrics.at("judgement_line_width")==5);
    CHECK(sixteen.gameplay.native.motion.at("press_response")==120);
    CHECK(sixteen.gameplay.native.sprites.count("note")==1);
    CHECK(sixteen.gameplay.native.sprites.at("note").empty());
    TempDirGuard imported{make_temp_dir()};
    const auto copy=tenriff::app::import_tenriff_skin(directory.path.u8string(),imported.path.u8string());
    REQUIRE(copy.success());
    const auto reloaded=tenriff::app::resolve_tenriff_skin(copy.install_root,copy.skin_name,16);
    CHECK(reloaded.gameplay.native.metrics==sixteen.gameplay.native.metrics);
    CHECK(reloaded.gameplay.native.sprites.at("note").empty());
}

TEST_CASE("Native gameplay invalid overrides fall back without changing legacy renderer") {
    TempDirGuard directory{make_temp_dir()};
    REQUIRE_FALSE(directory.path.empty());
    write_file(directory.path / "skin.json",R"({"format":"tenriff-skin","version":1,"name":"Invalid Instrument",
      "gameplay":{"native":{"metrics":{"key_max_height":999999,"unknown_metric":10},
      "motion":{"press_response":0},"colors":{"key_well":"invalid"},"fonts":{"combo":""},
      "rects":{"score":[1,2,"bad",4]},"sprites":{"note":[{"x":127,"y":0,"width":2,"height":32,"color":"#FFFFFF"}],
      "key_idle":[],"hold_head":[{"x":0,"y":0,"width":128,"height":32}],"hold_tail":[true]}}}})");
    const auto skin=tenriff::app::load_tenriff_skin_folder(directory.path.u8string(),4);
    REQUIRE(skin.found);
    CHECK_FALSE(skin.native_gameplay_fallback);
    CHECK_FALSE(skin.gameplay.native_renderer);
    CHECK(skin.warnings.size()>=9);
    CHECK(skin.gameplay.native.metrics.empty());
    CHECK(skin.gameplay.native.motion.empty());
    CHECK(skin.gameplay.native.colors.empty());
    CHECK(skin.gameplay.native.rects.empty());
    CHECK(skin.gameplay.native.fonts.empty());
    CHECK(skin.gameplay.native.sprites.count("note")==0);
    CHECK(skin.gameplay.native.sprites.count("hold_head")==0);
    CHECK(skin.gameplay.native.sprites.count("key_idle")==1);
}

TEST_CASE("Explicit legacy gameplay renderer overrides native-menu automatic fallback") {
    TempDirGuard directory{make_temp_dir()};
    REQUIRE_FALSE(directory.path.empty());
    write_file(directory.path / "skin.json",R"({"format":"tenriff-skin","version":1,"name":"Explicit Legacy",
        "lobby":{"renderer":"native"},"gameplay":{"renderer":"legacy"}})");
    const auto skin=tenriff::app::load_tenriff_skin_folder(directory.path.u8string(),10);
    REQUIRE(skin.found);
    CHECK(skin.warnings.empty());
    CHECK(skin.native_menu_renderer);
    CHECK_FALSE(skin.native_gameplay_fallback);
}

TEST_CASE("TenRiff key backdrop manifest fields preserve explicit off and per mode opacity") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    write_file(temp.path / "skin.json", R"({
        "format":"tenriff-skin","version":1,"name":"Backdrop",
        "gameplay":{"key_backdrop":false,"key_backdrop_opacity":0,"key_backdrop_brightness":1.5,"key_backdrop_height":0.8,
            "modes":{"4k":{"key_backdrop":true,"key_backdrop_opacity":0.65,"key_backdrop_brightness":0.5,"key_backdrop_height":0.25}}}
    })");
    const auto four = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), 4);
    const auto ten = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), 10);
    REQUIRE(four.found);
    CHECK(four.warnings.empty());
    CHECK(four.gameplay_style.key_backdrop_enabled.value_or(false));
    CHECK(four.gameplay_style.key_backdrop_opacity.value_or(-1) == doctest::Approx(0.65f));
    CHECK(four.gameplay_style.key_backdrop_brightness.value_or(-1) == doctest::Approx(0.5f));
    CHECK(four.gameplay_style.key_backdrop_height.value_or(-1) == doctest::Approx(0.25f));
    CHECK(ten.gameplay_style.key_backdrop_brightness.value_or(-1) == doctest::Approx(1.5f));
    CHECK(ten.gameplay_style.key_backdrop_height.value_or(-1) == doctest::Approx(0.8f));
    REQUIRE(ten.gameplay_style.key_backdrop_enabled.has_value());
    REQUIRE(ten.gameplay_style.key_backdrop_opacity.has_value());
    CHECK_FALSE(*ten.gameplay_style.key_backdrop_enabled);
    CHECK(*ten.gameplay_style.key_backdrop_opacity == doctest::Approx(0.0f));
}

TEST_CASE("native judgement line thickness scales with note height and preserves authored zero") {
    tenriff::app::NativeGameplaySkinStyle style;
    CHECK(tenriff::render::native_gameplay_judgement_line_width(style, 1.8) == doctest::Approx(2.0f));
    const auto thin = tenriff::render::native_gameplay_judgement_line_width(style, 0.5);
    const auto thick = tenriff::render::native_gameplay_judgement_line_width(style, 4.0);
    CHECK(thick / thin == doctest::Approx(8.0f));
    style.metrics["judgement_line_width"] = 5.0f;
    CHECK(tenriff::render::native_gameplay_judgement_line_width(style, 3.6) == doctest::Approx(10.0f));
    style.metrics["judgement_line_width"] = 0.0f;
    CHECK(tenriff::render::native_gameplay_judgement_line_width(style, 4.0) == doctest::Approx(0.0f));
    CHECK(tenriff::render::gameplay_key_backdrop_alpha(false, true, 1.0) == 0.0f);
    CHECK(tenriff::render::gameplay_key_backdrop_alpha(true, false, 1.0) == 0.0f);
    CHECK(tenriff::render::gameplay_key_backdrop_alpha(true, true, 0.4) == doctest::Approx(0.4f));
    CHECK(tenriff::render::gameplay_key_backdrop_alpha(true, true, 2.0) == 1.0f);
    CHECK(tenriff::render::gameplay_key_backdrop_alpha(true, true, std::numeric_limits<double>::quiet_NaN()) == 0.0f);
}

TEST_CASE("key backdrop RGB brightness and bottom anchored height are independent of alpha") {
    using namespace tenriff::render;
    CHECK(gameplay_key_backdrop_color(0x4080C0, 1.0) == 0x4080C0u);
    CHECK(gameplay_key_backdrop_color(0x4080C0, 0.5) == 0x204060u);
    CHECK(gameplay_key_backdrop_color(0x4080C0, 2.0) == 0x80FFFFu);
    CHECK(gameplay_key_backdrop_color(0x4080C0, 0.0) == 0u);
    CHECK(gameplay_key_backdrop_color(0x4080C0, std::numeric_limits<double>::quiet_NaN()) == 0x4080C0u);
    CHECK(gameplay_key_backdrop_top(2.0f, 1078.0f, 1.0) == doctest::Approx(2.0f));
    CHECK(gameplay_key_backdrop_top(2.0f, 1078.0f, 0.5) == doctest::Approx(540.0f));
    CHECK(gameplay_key_backdrop_top(2.0f, 1078.0f, 0.0) == doctest::Approx(1078.0f));
    CHECK(gameplay_key_backdrop_top(50.0f, 1060.0f, 0.25) == doctest::Approx(807.5f));
    CHECK(gameplay_key_backdrop_alpha(true, true, 0.4) == doctest::Approx(0.4f));
}

TEST_CASE("modern bundled skins share a solid readable native core in every key mode") {
    const auto root = std::filesystem::u8path(tenriff::app::find_bundled_tenriff_skin_root());
    REQUIRE_FALSE(root.empty());
    const char* names[] = {"Tencircle", "Tengear", "Tenmania", "TenRiff_AgentPrism_Universal_1K-16K",
        "TenRiff_NeonStep_Arcade_Universal_4K-10K", "TenRiff_Pulse_Mania_Universal_4K-10K",
        "TenRiff_SimpleSquare", "TenRiff_Studio", "TenRiff_VelocityCircuit_DanceGrid_4K-16K",
        "LUNAR-FRACTURE-Void-Garden-7K", "SunShine-Light-er-Arrow-7K", "TenRiff_NativeEditable"};
    for (const auto* name : names) for (int lanes = 1; lanes <= 16; ++lanes) {
        const auto skin = tenriff::app::load_tenriff_skin_folder((root / name).u8string(), lanes);
        REQUIRE(skin.found);
        CHECK(skin.warnings.empty());
        CHECK(skin.native_menu_renderer);
        CHECK(skin.native_gameplay_fallback);
        CHECK(skin.referenced_asset_paths.size() == 1);
        CHECK(skin.gameplay_style.note_shape.value() == "rect");
        CHECK(skin.gameplay_style.lane_colors.size() == static_cast<std::size_t>(lanes));
        CHECK(skin.gameplay.gear_overlay_image.path.empty());
        CHECK(skin.gameplay_background_path.empty());
        const auto& sprite = skin.gameplay.native.sprites.at("note");
        REQUIRE(sprite.size() == 4);
        // Border opacity only scales layers0/1. The main body must remain opaque.
        CHECK(sprite[2].alpha == 1);
        CHECK(sprite[2].width >= 120);
        CHECK(sprite[2].height >= 24);
        CHECK(skin.gameplay.native.metrics.at("key_max_height") == 126);
        CHECK(skin.gameplay.native.motion.at("press_depth") == 3);
    }
}

TEST_CASE("all procedural note shapes survive native skin load and per-mode overrides") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    for (const std::string shape : {"rect", "simple_rect", "circle", "triangle", "pentagon", "hexagon",
                                    "square", "diamond", "arrow", "hex"}) {
        write_file(temp.path / "skin.json",
            "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Shapes\","
            "\"gameplay\":{\"renderer\":\"native\",\"note_shape\":\"" + shape +
            "\",\"modes\":{\"4k\":{\"note_shape\":\"" + shape + "\"}}}}");
        for (const int lanes : {4, 10}) {
            const auto skin = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), lanes);
            REQUIRE(skin.found);
            CHECK(skin.warnings.empty());
            REQUIRE(skin.gameplay_style.note_shape.has_value());
            CHECK(skin.gameplay_style.note_shape.value() == (shape == "hex" ? "hexagon" : shape));
        }
    }
}

TEST_CASE("skin timing text and bar overrides load independently including modes") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    write_file(temp.path / "skin.json", R"({"format":"tenriff-skin","version":1,"name":"Timing",
      "gameplay":{"renderer":"native","show_timing_feedback":false,"show_timing_bar":true,"timing_bar_always_visible":true,
      "native":{"rects":{"timing_label":[100,-20,0,0],"timing":[-80,60,0,0]}},
      "modes":{"4k":{"show_timing_feedback":true,"show_timing_bar":false,"timing_bar_always_visible":false}}}})");
    for (int lanes : {4, 10}) {
        const auto skin = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), lanes);
        REQUIRE(skin.found);
        CHECK(skin.warnings.empty());
        CHECK(skin.gameplay_style.show_timing_feedback.value() == (lanes == 4));
        CHECK(skin.gameplay_style.show_timing_bar.value() == (lanes != 4));
        CHECK(skin.gameplay_style.timing_bar_always_visible.value() == (lanes != 4));
        CHECK(skin.gameplay.native.rects.at("timing_label")[0] == 100);
    }
}

TEST_CASE("skin judgement line gain composes with the profile dimmer") {
    using tenriff::app::resolve_skin_judgement_line_brightness;
    tenriff::app::TenRiffSkinGameplayStyle authored;
    CHECK(resolve_skin_judgement_line_brightness(0.5, nullptr) == doctest::Approx(0.5));
    CHECK(resolve_skin_judgement_line_brightness(0.5, &authored) == doctest::Approx(0.5));
    for (const float gain : {0.0f, 0.5f, 0.8f, 1.0f}) {
        authored.judgement_line_brightness = gain;
        for (const double profile : {0.0, 0.25, 0.5, 1.0})
            CHECK(resolve_skin_judgement_line_brightness(profile, &authored) == doctest::Approx(profile * gain));
    }
    authored.judgement_line_brightness = 0.5f;
    CHECK(resolve_skin_judgement_line_brightness(2.0, &authored) == doctest::Approx(0.5));
    CHECK(resolve_skin_judgement_line_brightness(-1.0, &authored) == 0.0);
    CHECK(resolve_skin_judgement_line_brightness(std::numeric_limits<double>::quiet_NaN(), &authored) == doctest::Approx(0.5));
    authored.judgement_line_brightness = std::numeric_limits<float>::infinity();
    CHECK(resolve_skin_judgement_line_brightness(0.5, &authored) == doctest::Approx(0.5));
}

TEST_CASE("skin judgement line gain loads common and per-mode including scratch layouts") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    write_file(temp.path / "skin.json", R"({"format":"tenriff-skin","version":1,"name":"Line Gain",
      "gameplay":{"renderer":"native","judgement_line_brightness":0.8,
      "modes":{"4k":{"judgement_line_brightness":0},"8k":{"judgement_line_brightness":1},
      "7+1":{"judgement_line_brightness":0.25}}}})");
    for (int lanes : {4, 8, 16}) {
        const auto skin = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), lanes);
        REQUIRE(skin.found);
        CHECK(skin.warnings.empty());
        CHECK(skin.native_gameplay_fallback);
        REQUIRE(skin.gameplay_style.judgement_line_brightness.has_value());
        CHECK(skin.gameplay_style.judgement_line_brightness.value() == doctest::Approx(lanes == 4 ? 0.0 : lanes == 8 ? 1.0 : 0.8));
    }
    const auto scratch = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), 8, "7+1");
    CHECK(scratch.warnings.empty());
    CHECK(scratch.gameplay_style.judgement_line_brightness.value() == doctest::Approx(0.25));
    for (const std::string value : {"-0.01", "1.01", "\"0.5\"", "null"}) {
        write_file(temp.path / "skin.json", "{\"format\":\"tenriff-skin\",\"version\":1,\"name\":\"Invalid Gain\","
            "\"gameplay\":{\"renderer\":\"native\",\"judgement_line_brightness\":" + value + "}}");
        const auto invalid = tenriff::app::load_tenriff_skin_folder(temp.path.u8string(), 4);
        REQUIRE(invalid.found);
        CHECK_FALSE(invalid.warnings.empty());
        CHECK_FALSE(invalid.gameplay_style.judgement_line_brightness.has_value());
        CHECK(tenriff::app::resolve_skin_judgement_line_brightness(0.5, &invalid.gameplay_style) == doctest::Approx(0.5));
    }
}

TEST_CASE("AI authoring example loads without assets and retains visible note cores") {
    // The test executable has a copied skin catalog, but examples stay in source.
    const auto example = std::filesystem::u8path(TENRIFF_SOURCE_DIR) / "examples/skins/TenRiff-Example";
    for (int lanes = 1; lanes <= 16; ++lanes) {
        const auto skin = tenriff::app::load_tenriff_skin_folder(example.u8string(), lanes);
        REQUIRE(skin.found);
        CHECK(skin.warnings.empty());
        CHECK(skin.native_menu_renderer);
        CHECK(skin.native_gameplay_fallback);
        CHECK(skin.referenced_asset_paths.empty());
        for (const auto* slot : {"note", "hold_head", "hold_tail"}) {
            const auto& layers = skin.gameplay.native.sprites.at(slot);
            REQUIRE(layers.size() >= 3);
            CHECK(layers[2].alpha == 1);
            CHECK(layers[2].height >= 24);
        }
    }
}

TEST_CASE("TenRiff skin background videos resolve explicitly, per screen and per mode") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "Loops";
    write_file(skin / "cli" / "menu.mp4", "mp4");
    write_file(skin / "cli" / "title.webm", "webm");
    write_file(skin / "cli" / "play.MP4", "mp4");
    write_file(skin / "cli" / "wide.mkv", "mkv");
    // A conventional-looking file is never picked up without a manifest entry.
    write_file(skin / "lobby" / "background.mp4", "mp4");
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\", \"version\": 1, \"name\": \"Loops\",\n"
               "  \"lobby\": {\n"
               "    \"renderer\": \"native\",\n"
               "    \"background_video\": \"cli/menu.mp4\",\n"
               "    \"screen_videos\": { \"title\": \"cli/title.webm\", \"result\": \"\" }\n"
               "  },\n"
               "  \"gameplay\": {\n"
               "    \"background_video\": \"cli/play.MP4\",\n"
               "    \"modes\": { \"16k\": { \"background_video\": \"cli/wide.mkv\" },\n"
               "               \"4k\": { \"background_video\": \"\" } }\n"
               "  }\n"
               "}\n");

    const auto seven = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 7);
    REQUIRE(seven.found);
    CHECK(seven.warnings.empty());
    CHECK(seven.lobby_background_video_path.find("menu.mp4") != std::string::npos);
    REQUIRE(seven.screen_background_video_paths.count("title") == 1u);
    CHECK(seven.screen_background_video_paths.at("title").find("title.webm") != std::string::npos);
    REQUIRE(seven.screen_background_video_paths.count("result") == 1u);
    CHECK(seven.screen_background_video_paths.at("result").empty());
    CHECK(seven.lobby_background_path.empty());
    CHECK(seven.gameplay_background_video_path.find("play.MP4") != std::string::npos);
    const auto& refs = seven.referenced_asset_paths;
    CHECK(std::count_if(refs.begin(), refs.end(), [](const std::string& path) {
              return path.find(".mp4") != std::string::npos || path.find(".MP4") != std::string::npos ||
                     path.find(".webm") != std::string::npos;
          }) == 3);
    // Like a gameplay background image, a gameplay video is gameplay presentation:
    // a native-menu skin then needs gameplay.renderer=native to keep Luma Keys.
    CHECK(seven.native_menu_renderer);
    CHECK_FALSE(seven.native_gameplay_fallback);

    const auto sixteen = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 16);
    CHECK(sixteen.gameplay_background_video_path.find("wide.mkv") != std::string::npos);
    const auto four = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    CHECK(four.gameplay_background_video_path.empty());
    CHECK(four.native_gameplay_fallback);
    CHECK(four.warnings.empty());

    const auto imports = temp.path / "imports";
    const auto imported = tenriff::app::import_tenriff_skin(skin.u8string(), imports.u8string());
    REQUIRE(imported.success());
    for (const auto* name : {"menu.mp4", "title.webm", "play.MP4", "wide.mkv"}) {
        CHECK(std::filesystem::is_regular_file(imports / imported.skin_name / "cli" / name));
    }
    CHECK_FALSE(std::filesystem::exists(imports / imported.skin_name / "lobby" / "background.mp4"));
}

TEST_CASE("TenRiff skin background videos reject images, escapes, missing files and bad types") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE(!temp.path.empty());
    const auto skin = temp.path / "BadLoops";
    write_file(skin / "still.png");
    write_file(temp.path / "outside.mp4", "mp4");
    write_file(skin / "skin.json",
               "{\n"
               "  \"format\": \"tenriff-skin\", \"version\": 1, \"name\": \"Bad Loops\",\n"
               "  \"lobby\": {\n"
               "    \"background_video\": \"still.png\",\n"
               "    \"screen_videos\": { \"title\": \"../outside.mp4\", \"options\": 3,\n"
               "                         \"song_select\": \"missing.mp4\", \"gameplay\": \"x.mp4\" }\n"
               "  },\n"
               "  \"gameplay\": { \"background_video\": [\"cli/a.mp4\"] }\n"
               "}\n");
    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 7);
    REQUIRE(loaded.found);
    CHECK(loaded.lobby_background_video_path.empty());
    CHECK(loaded.screen_background_video_paths.empty());
    CHECK(loaded.gameplay_background_video_path.empty());
    CHECK(loaded.referenced_asset_paths.empty());
    const auto has = [&](const std::string& text) {
        return std::any_of(loaded.warnings.begin(), loaded.warnings.end(),
                           [&](const std::string& warning) { return warning.find(text) != std::string::npos; });
    };
    CHECK(has("lobby.background_video video was not found or is unsupported"));
    CHECK(has("lobby.screen_videos.title cannot leave the skin folder"));
    CHECK(has("lobby.screen_videos.options must be a video path"));
    CHECK(has("lobby.screen_videos.song_select video was not found"));
    CHECK(has("lobby.screen_videos.gameplay is not a supported field"));
    CHECK(has("gameplay.background_video must be a video path"));
}

TEST_CASE("bundled TenRiff Code CLI skin loads every key mode with its looping videos") {
    const auto root = std::filesystem::u8path(tenriff::app::find_bundled_tenriff_skin_root());
    REQUIRE_FALSE(root.empty());
    const auto folder = root / "TenRiff_Code_CLI";
    for (int lanes = 1; lanes <= 17; ++lanes) {
        const auto skin = lanes == 17
            ? tenriff::app::load_tenriff_skin_folder(folder.u8string(), 8, "7+1")
            : tenriff::app::load_tenriff_skin_folder(folder.u8string(), lanes);
        REQUIRE(skin.found);
        CHECK(skin.warnings.empty());
        CHECK(skin.native_menu_renderer);
        CHECK(skin.gameplay.native_renderer);
        CHECK(skin.lobby_background_video_path.find("lobby-loop.mp4") != std::string::npos);
        REQUIRE(skin.screen_background_video_paths.count("title") == 1u);
        CHECK(skin.screen_background_video_paths.at("title").find("title-loop.mp4") != std::string::npos);
        CHECK(skin.gameplay_background_video_path.find("gameplay-loop.mp4") != std::string::npos);
        CHECK(!skin.lobby_background_path.empty());
        CHECK(!skin.gameplay_background_path.empty());
        CHECK(skin.gameplay_style.lane_colors.size() == static_cast<std::size_t>(lanes == 17 ? 8 : lanes));
        const auto& sprite = skin.gameplay.native.sprites.at("note");
        REQUIRE(sprite.size() >= 3);
        CHECK(sprite[2].alpha == 1);  // The body survives disabled note borders.
        CHECK(skin.native_menu.fonts.at("body") == "Cascadia Mono");
    }
}
