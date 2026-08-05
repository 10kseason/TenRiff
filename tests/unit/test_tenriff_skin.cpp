#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "app/ImportedGameplaySkin.h"
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
               "      \"left_panel\": [486, 152, 38, 922],\n"
               "      \"sidebar\": [0, 0, 10, 10]\n"
               "    },\n"
               "    \"title\": { \"buttons\": [470, 360, 1450] }\n"
               "  }\n"
               "}\n");

    const auto loaded = tenriff::app::load_tenriff_skin_folder(skin.u8string(), 4);
    REQUIRE(loaded.found);
    REQUIRE(loaded.layout_rects.count("song_select.center_panel") == 1u);
    const auto& rect = loaded.layout_rects.at("song_select.center_panel");
    CHECK(rect.left == doctest::Approx(1126.0f));
    CHECK(rect.top == doctest::Approx(152.0f));
    CHECK(rect.right == doctest::Approx(1882.0f));
    CHECK(rect.bottom == doctest::Approx(922.0f));

    // right <= left, an unknown slot name, and a three-number rect are all dropped.
    CHECK(loaded.layout_rects.count("song_select.left_panel") == 0u);
    CHECK(loaded.layout_rects.count("song_select.sidebar") == 0u);
    CHECK(loaded.layout_rects.count("title.buttons") == 0u);
    CHECK(loaded.layout_rects.size() == 1u);
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
