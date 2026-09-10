#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>

#include "app/SkinPreset.h"
#include "app/Lr2Skin.h"
#include "app/TenRiffSkin.h"
#include "config/Config.h"

namespace {
namespace fs = std::filesystem;
void require_preset_success(const tenriff::app::SkinPresetResult& result) {
    if (!result.success()) std::cerr << "Skin preset failure: " << result.error << '\n';
    REQUIRE(result.success());
}
struct PresetDirectory {
    fs::path path;
    PresetDirectory() {
        const auto root = fs::current_path() / "skin-preset-test-scratch";
        fs::create_directories(root);
        for (int n = 0; n < 10000; ++n) {
            const auto candidate = root / std::to_string(n);
            if (fs::create_directory(candidate)) { path = candidate; break; }
        }
        REQUIRE(!path.empty());
    }
    ~PresetDirectory() {
        std::error_code ec;
        if (!path.empty()) fs::remove_all(path, ec);
        fs::remove(path.parent_path(), ec);
    }
};
void write_preset_test_file(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(out.good());
}
std::string read_preset_test_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void append_u32(std::string& bytes, std::uint32_t number) {
    for (int n = 0; n < 4; ++n) bytes.push_back(static_cast<char>((number >> (n * 8)) & 255));
}
std::string raw_preset(const std::string& json, std::uint32_t count = 0) {
    std::string data = "TRSKIN\r\n";
    append_u32(data, 1);
    append_u32(data, static_cast<std::uint32_t>(json.size()));
    data += json;
    append_u32(data, count);
    return data;
}
void append_empty_asset(std::string& data, const std::string& name) {
    append_u32(data, static_cast<std::uint32_t>(name.size()));
    data += name;
    append_u32(data, 0);
    append_u32(data, 0);
    append_u32(data, 0);  // CRC32 of an empty payload.
}
void make_manifest_preset_skin(const fs::path& folder) {
    write_preset_test_file(folder / "skin.json", R"({
      "format":"tenriff-skin", "version":1, "name":"Portable Test",
      "gameplay":{"note":"note.png", "modes":{"7+1":{"note":"scratch.png"}}}
    })");
    write_preset_test_file(folder / "note.png", "portable-note-bytes");
    write_preset_test_file(folder / "scratch.png", "portable-scratch-bytes");
}
}  // namespace

TEST_CASE("skin preset native settings round trip only appearance in Unicode paths") {
    PresetDirectory dir;
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.note_height_scales["7k"] = 1.35;
    skin.lane_colors["5k"] = {"rose", "azure", "ice", "azure", "rose"};
    skin.judgement_position = 0.6;
    skin.judgement_offset_x = 42;
    skin.combo_offset_x = -52;
    skin.lr2_skin_name = "irrelevant private catalog name";
    const auto file = dir.path / fs::u8path(u8"공유 프리셋.trskin");
    const auto saved = tenriff::app::export_skin_preset(file.u8string(), skin, {});
    require_preset_success(saved);
    CHECK(saved.file_count == 0);
    const auto contents = read_preset_test_file(file);
    CHECK(contents.find("irrelevant private catalog name") == std::string::npos);
    CHECK(contents.find("account") == std::string::npos);
    CHECK(contents.find("audio") == std::string::npos);
    CHECK(contents.find("offsets") == std::string::npos);
    const auto imported = tenriff::app::import_skin_preset(file.u8string(), (dir.path / fs::u8path(u8"다른 PC")).u8string());
    require_preset_success(imported);
    CHECK(imported.skin.judgement_position == doctest::Approx(0.6));
    CHECK(imported.skin.judgement_offset_x == 42);
    CHECK(imported.skin.combo_offset_x == -52);
    CHECK(imported.skin.note_height_scales.at("7k") == doctest::Approx(1.35));
    CHECK(imported.skin.lane_colors.at("5k") == skin.lane_colors.at("5k"));
    CHECK_FALSE(tenriff::app::export_skin_preset(file.u8string(), skin, {}).success());
    CHECK(read_preset_test_file(file) == contents);
}

TEST_CASE("skin preset carries all manifest assets and collision safe import survives source removal") {
    PresetDirectory dir;
    const auto catalog = dir.path / fs::u8path(u8"원본 스킨");
    const auto folder = catalog / "Portable";
    make_manifest_preset_skin(folder);
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.source = "tenriff";
    skin.tenriff_skin_name = "Portable";
    const auto file = dir.path / "portable.trskin";
    const auto saved = tenriff::app::export_skin_preset(file.u8string(), skin, catalog.u8string());
    require_preset_success(saved);
    REQUIRE(saved.file_count == 3);
    fs::remove_all(catalog);  // Only this fixture's original assets are removed.
    const auto profile = dir.path / fs::u8path(u8"받는 사용자");
    const auto imported = tenriff::app::import_skin_preset(file.u8string(), profile.u8string());
    require_preset_success(imported);
    const auto second = tenriff::app::import_skin_preset(file.u8string(), profile.u8string());
    require_preset_success(second);
    CHECK(imported.path != second.path);
    CHECK(imported.skin.tenriff_skin_name != second.skin.tenriff_skin_name);
    const auto first_folder = fs::u8path(imported.path);
    CHECK(read_preset_test_file(first_folder / "note.png") == "portable-note-bytes");
    CHECK(read_preset_test_file(first_folder / "scratch.png") == "portable-scratch-bytes");
    const auto resolved = tenriff::app::load_tenriff_skin_folder(imported.path, 8, "7+1");
    REQUIRE(resolved.found);
    CHECK(resolved.warnings.empty());
    CHECK(resolved.referenced_asset_paths.size() >= 1);
}

TEST_CASE("skin preset rejects path traversal device files duplicate names and invalid lengths") {
    PresetDirectory dir;
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.source = "tenriff";
    skin.tenriff_skin_name = "Imported";
    const auto json = tenriff::config::serialize_skin_config(skin);
    const auto profile = dir.path / "profile";
    const auto file = dir.path / "bad.trskin";
    for (const auto* name : {"../outside.txt", "C:/outside.txt", "//server/file", "CON.txt", "file:stream", "a/../escape", "bad\\path", "trailing. ", "x//y"}) {
        auto bytes = raw_preset(json, 1);
        append_empty_asset(bytes, name);
        write_preset_test_file(file, bytes);
        CHECK_FALSE(tenriff::app::import_skin_preset(file.u8string(), profile.u8string()).success());
    }
    auto duplicate = raw_preset(json, 2);
    append_empty_asset(duplicate, "NOTE.png");
    append_empty_asset(duplicate, "note.png");
    write_preset_test_file(file, duplicate);
    CHECK_FALSE(tenriff::app::import_skin_preset(file.u8string(), profile.u8string()).success());
    auto excessive = raw_preset(json, 4097);
    write_preset_test_file(file, excessive);
    CHECK_FALSE(tenriff::app::import_skin_preset(file.u8string(), profile.u8string()).success());
    const auto root = profile / "skins" / "tenriff";
    CHECK(fs::is_empty(root));
    CHECK_FALSE(fs::exists(dir.path / "outside.txt"));
}

TEST_CASE("skin preset corruption or truncation never leaves an activated partial skin") {
    PresetDirectory dir;
    const auto catalog = dir.path / "catalog";
    make_manifest_preset_skin(catalog / "Portable");
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.source = "tenriff";
    skin.tenriff_skin_name = "Portable";
    const auto file = dir.path / "good.trskin";
    REQUIRE(tenriff::app::export_skin_preset(file.u8string(), skin, catalog.u8string()).success());
    auto bytes = read_preset_test_file(file);
    const auto bad = dir.path / "bad.trskin";
    const auto profile = dir.path / "receiver";
    bytes.back() ^= 0x40;
    write_preset_test_file(bad, bytes);
    CHECK_FALSE(tenriff::app::import_skin_preset(bad.u8string(), profile.u8string()).success());
    bytes.resize(bytes.size() / 2);
    write_preset_test_file(bad, bytes);
    CHECK_FALSE(tenriff::app::import_skin_preset(bad.u8string(), profile.u8string()).success());
    CHECK(fs::is_empty(profile / "skins" / "tenriff"));
}

TEST_CASE("skin preset rejects deeply nested untrusted settings before JSON parsing") {
    PresetDirectory dir;
    const auto file = dir.path / "deep.trskin";
    write_preset_test_file(file, raw_preset(std::string(1000, '[') + "0" + std::string(1000, ']')));
    CHECK_FALSE(tenriff::app::import_skin_preset(file.u8string(), (dir.path / "profile").u8string()).success());
}

TEST_CASE("skin preset accepts LR2 parent references that remain inside the skin") {
    PresetDirectory dir;
    const auto catalog = dir.path / "catalog";
    const auto folder = catalog / "Portable LR2";
    write_preset_test_file(folder / "play.lr2skin", "#INFORMATION,0,Portable,Tester\n#ENDOFHEADER\n#INCLUDE,csv/layout.csv\n");
    write_preset_test_file(folder / "csv" / "layout.csv",
        "#IMAGE,../images/note.png\n#SRC_NOTE,0,0,0,0,30,22,1,1,0,0\n"
        "#SRC_NOTE,1,0,30,0,30,22,1,1,0,0\n#DST_NOTE,0,0,0,400,30,22\n#DST_NOTE,1,0,35,400,30,22\n");
    write_preset_test_file(folder / "images" / "note.png", "lr2-note");
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.source = "lr2";
    skin.lr2_skin_name = "Portable LR2";
    const auto file = dir.path / "lr2.trskin";
    const auto saved = tenriff::app::export_skin_preset(file.u8string(), skin, catalog.u8string());
    require_preset_success(saved);
    fs::remove_all(catalog);
    const auto profile = dir.path / "receiver";
    const auto imported = tenriff::app::import_skin_preset(file.u8string(), profile.u8string());
    require_preset_success(imported);
    const auto repeated = tenriff::app::import_skin_preset(file.u8string(), profile.u8string());
    require_preset_success(repeated);
    CHECK(imported.path != repeated.path);
    const auto resolved = tenriff::app::resolve_lr2_play_skin((profile / "skins" / "lr2").u8string(), imported.skin.lr2_skin_name, 4);
    REQUIRE(resolved.found);
    REQUIRE(!resolved.note_images.empty());
    CHECK(resolved.note_images.front().path.find("receiver") != std::string::npos);
}

TEST_CASE("skin preset rejects LR2 references outside its own folder before exporting") {
    PresetDirectory dir;
    const auto catalog = dir.path / "catalog";
    const auto folder = catalog / "Bad LR2";
    write_preset_test_file(folder / "play.lr2skin", "#INFORMATION,0,Portable,Tester\n#ENDOFHEADER\n#IMAGE,../private.png\n");
    write_preset_test_file(catalog / "private.png", "outside");
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.source = "lr2";
    skin.lr2_skin_name = "Bad LR2";
    const auto file = dir.path / "bad.trskin";
    CHECK_FALSE(tenriff::app::export_skin_preset(file.u8string(), skin, catalog.u8string()).success());
    CHECK_FALSE(fs::exists(file));
}

#ifdef _WIN32
TEST_CASE("skin preset follows Windows LR2 CP932 asset name decoding") {
    PresetDirectory dir;
    const auto catalog = dir.path / "catalog";
    const auto folder = catalog / "Japanese";
    const std::string cp932_name = "\x83\x6d\x81\x5b\x83\x67.png";
    write_preset_test_file(folder / "play.lr2skin",
        "#INFORMATION,0,Japanese,Tester\n#ENDOFHEADER\n#IMAGE," + cp932_name + "\n"
        "#SRC_NOTE,0,0,0,0,30,22,1,1,0,0\n#DST_NOTE,0,0,0,400,30,22\n");
    write_preset_test_file(folder / fs::u8path(u8"ノート.png"), "cp932-note");
    auto skin = tenriff::config::ConfigLoader{}.defaults().skin;
    skin.source = "lr2";
    skin.lr2_skin_name = "Japanese";
    const auto file = dir.path / "japanese.trskin";
    const auto saved = tenriff::app::export_skin_preset(file.u8string(), skin, catalog.u8string());
    require_preset_success(saved);
    fs::remove_all(catalog);
    const auto imported = tenriff::app::import_skin_preset(file.u8string(), (dir.path / "receiver").u8string());
    require_preset_success(imported);
    CHECK(read_preset_test_file(fs::u8path(imported.path) / fs::u8path(u8"ノート.png")) == "cp932-note");
}
#endif
