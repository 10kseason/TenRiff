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
    CHECK(std::filesystem::is_regular_file(std::filesystem::path(created.folder_path) / "skin.json"));
    CHECK(std::filesystem::is_directory(std::filesystem::path(created.folder_path) / "lobby"));
    CHECK(std::filesystem::is_directory(std::filesystem::path(created.folder_path) / "lobby" / "screens"));
    CHECK(std::filesystem::is_directory(std::filesystem::path(created.folder_path) / "gameplay"));
    std::ifstream manifest_file(std::filesystem::path(created.folder_path) / "skin.json",
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

TEST_CASE("Skin settings stable row ids account for the optional LR2 row") {
    const tenriff::app::SkinSettingsRows native_rows{false};
    const tenriff::app::SkinSettingsRows lr2_rows{true};
    CHECK(native_rows.count() == 48);
    CHECK(lr2_rows.count() == 49);
    CHECK(native_rows.index_of(tenriff::app::SkinSettingsRowId::KeyMode) == 0);
    CHECK(native_rows.index_of(tenriff::app::SkinSettingsRowId::ScratchPosition) == 1);
    CHECK(native_rows.index_of(tenriff::app::SkinSettingsRowId::SkinSource) == 2);
    CHECK(native_rows.index_of(tenriff::app::SkinSettingsRowId::Lr2Resolution) == -1);
    CHECK(native_rows.index_of(tenriff::app::SkinSettingsRowId::ImportSkin) == 4);
    CHECK(native_rows.index_of(tenriff::app::SkinSettingsRowId::Back) == 47);
    CHECK(lr2_rows.index_of(tenriff::app::SkinSettingsRowId::Lr2Resolution) == 4);
    CHECK(lr2_rows.index_of(tenriff::app::SkinSettingsRowId::ImportSkin) == 5);
    CHECK(lr2_rows.index_of(tenriff::app::SkinSettingsRowId::Back) == 48);
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
