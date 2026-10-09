#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>

#include "doctest/doctest.h"

#include "config/KeycodeMap.h"
#include "config/Keymap.h"
#include "input/LaneBindingState.h"
#include "gameplay/GameplayEngine.h"

namespace tenriff::app {
std::string resolve_keymap_edit_mode_for_menu(std::optional<int> selected_chart_key_count,
                                              std::string_view runtime_key_mode);
std::vector<uint32_t> build_menu_probe_keycodes(const std::vector<uint32_t>& fixed_menu_keys,
                                                const config::Keymap& working_keymap,
                                                std::string_view keymap_edit_mode,
                                                bool include_keymap_bindings,
                                                bool capture_all_keys);
}

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
    std::filesystem::path original = std::filesystem::current_path();

    ~CurrentPathGuard() {
        std::error_code ec;
        std::filesystem::current_path(original, ec);
    }
};

std::filesystem::path make_temp_dir() {
    auto base = std::filesystem::temp_directory_path();
    std::mt19937_64 rng{123456789ULL};
    for (int attempt = 0; attempt < 32; ++attempt) {
        auto candidate = base / ("tenriff_keymap_test_" + std::to_string(rng()));
        std::error_code ec;
        if (std::filesystem::create_directories(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    REQUIRE(file.good());
    file << content;
}

}  // namespace

TEST_CASE("default keymap exposes separate mode bindings from 4K through 16K") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();

    CHECK(keymap.mode_bindings.count("4k") == 1u);
    CHECK(keymap.mode_bindings.count("5k") == 1u);
    CHECK(keymap.mode_bindings.count("7k") == 1u);
    CHECK(keymap.mode_bindings.count("8k") == 1u);
    CHECK(keymap.mode_bindings.count("10k") == 1u);
    CHECK(keymap.mode_bindings.count("16k") == 1u);
    CHECK(manager.lane_ids_for_mode("4k").size() == 4u);
    CHECK(manager.lane_ids_for_mode("9k").size() == 9u);
    CHECK(manager.lane_ids_for_mode("16k").size() == 16u);
    CHECK(manager.bindings_for_mode(keymap, "4k").at("lane4") == "Semicolon");
    CHECK(manager.bindings_for_mode(keymap, "5k").at("lane3") == "K");
    CHECK(manager.bindings_for_mode(keymap, "7k").at("lane4") == "M");
    CHECK(manager.bindings_for_mode(keymap, "8k").at("lane4") == "V");
    CHECK(manager.bindings_for_mode(keymap, "10k").at("lane1") == "Q");
    CHECK(manager.bindings_for_mode(keymap, "10k").at("lane5") == "V");
    CHECK(manager.bindings_for_mode(keymap, "10k").at("lane10") == "LBracket");
    CHECK(manager.bindings_for_mode(keymap, "16k").at("lane8") == "F");
    CHECK(manager.bindings_for_mode(keymap, "16k").at("lane9") == "U");
    CHECK(manager.bindings_for_mode(keymap, "16k").at("lane16") == "Semicolon");
}

TEST_CASE("keymap save and load preserve per-mode bindings") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.mode_bindings["4k"]["lane1"] = "A";
    keymap.mode_bindings["4k"]["lane4"] = "Semicolon";
    keymap.mode_bindings["7k"]["lane4"] = "Enter";
    keymap.mode_bindings["16k"]["lane16"] = "Apostrophe";

    std::string error;
    REQUIRE(manager.save_profile("profiles/test", keymap, &error));
    CHECK(error.empty());

    const auto result = manager.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane1") == "A");
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane4") == "Semicolon");
    CHECK(manager.bindings_for_mode(result.keymap, "7k").at("lane4") == "Enter");
    CHECK(manager.bindings_for_mode(result.keymap, "16k").at("lane16") == "Apostrophe");
    CHECK(manager.bindings_for_mode(result.keymap, "10k").at("lane5") == "V");
}

TEST_CASE("keymap save and load preserve arrow-key bindings") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.mode_bindings["7k"]["lane1"] = "Up";
    keymap.mode_bindings["7k"]["lane2"] = "Left";
    keymap.mode_bindings["7k"]["lane3"] = "Right";

    std::string error;
    REQUIRE(manager.save_profile("profiles/test", keymap, &error));
    CHECK(error.empty());

    const auto result = manager.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(manager.bindings_for_mode(result.keymap, "7k").at("lane1") == "Up");
    CHECK(manager.bindings_for_mode(result.keymap, "7k").at("lane2") == "Left");
    CHECK(manager.bindings_for_mode(result.keymap, "7k").at("lane3") == "Right");
}

TEST_CASE("keymap edit mode resolution prefers selected chart key count") {
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(7, "4k") == "7k");
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(8, "10k") == "8k");
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(std::nullopt, "16k") == "4k");
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(std::nullopt, "auto") == "4k");
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(std::nullopt, "") == "4k");
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(6, "none") == "6k");
    CHECK(tenriff::app::resolve_keymap_edit_mode_for_menu(8, "none") == "8k");
}

TEST_CASE("menu probe keycodes only include menu navigation outside keymap screens") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();
    const std::vector<uint32_t> fixed_menu_keys{
        tenriff::config::KeycodeMap::to_keycode("Up").value(),
        tenriff::config::KeycodeMap::to_keycode("Enter").value(),
    };

    const auto keycodes = tenriff::app::build_menu_probe_keycodes(
        fixed_menu_keys, keymap, "4k", false, false);

    CHECK(keycodes == fixed_menu_keys);
    CHECK(std::find(keycodes.begin(), keycodes.end(),
                    tenriff::config::KeycodeMap::to_keycode("D").value()) == keycodes.end());
}

TEST_CASE("menu probe keycodes include active keymap bindings on keymap screens") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();
    const std::vector<uint32_t> fixed_menu_keys{
        tenriff::config::KeycodeMap::to_keycode("Up").value(),
    };

    const auto keycodes = tenriff::app::build_menu_probe_keycodes(
        fixed_menu_keys, keymap, "4k", true, false);

    CHECK(std::find(keycodes.begin(), keycodes.end(), fixed_menu_keys.front()) != keycodes.end());
    CHECK(std::find(keycodes.begin(), keycodes.end(),
                    tenriff::config::KeycodeMap::to_keycode("D").value()) != keycodes.end());
    CHECK(std::find(keycodes.begin(), keycodes.end(),
                    tenriff::config::KeycodeMap::to_keycode("Semicolon").value()) != keycodes.end());
}

#ifdef _WIN32
TEST_CASE("Windows key capture preserves Shift sides without changing stored generic Shift bindings") {
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.mode_bindings["4k"]["lane1"] = "VK_10";

    const auto capture_keycodes = tenriff::app::build_menu_probe_keycodes(
        {}, keymap, "4k", true, true);
    CHECK(std::find(capture_keycodes.begin(), capture_keycodes.end(), 0x10u) == capture_keycodes.end());
    for (const char* shift : {"LShift", "RShift"}) {
        CHECK(std::find(capture_keycodes.begin(), capture_keycodes.end(),
                        tenriff::config::KeycodeMap::to_keycode(shift).value()) != capture_keycodes.end());
    }

    const auto bound_keycodes = tenriff::app::build_menu_probe_keycodes(
        {}, keymap, "4k", true, false);
    CHECK(std::find(bound_keycodes.begin(), bound_keycodes.end(), 0x10u) != bound_keycodes.end());
}
#endif

TEST_CASE("menu probe keycodes expand to full polling range during key capture") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();
    const std::vector<uint32_t> fixed_menu_keys{
        tenriff::config::KeycodeMap::to_keycode("Up").value(),
    };

    const auto keycodes = tenriff::app::build_menu_probe_keycodes(
        fixed_menu_keys, keymap, "4k", true, true);

    CHECK(keycodes.size() > 100u);
    CHECK(std::find(keycodes.begin(), keycodes.end(),
                    tenriff::config::KeycodeMap::to_keycode("F12").value()) != keycodes.end());
    CHECK(std::find(keycodes.begin(), keycodes.end(),
                    tenriff::config::KeycodeMap::to_keycode("Delete").value()) != keycodes.end());
    CHECK(std::find(keycodes.begin(), keycodes.end(),
                    tenriff::config::KeycodeMap::to_keycode("Semicolon").value()) != keycodes.end());
}

TEST_CASE("legacy keymap bindings migrate into 10K mode without losing other defaults") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "profiles" / "test" / "keymap.json",
               "{\n"
               "  \"layout\": \"10key\",\n"
               "  \"bindings\": {\n"
               "    \"lane1\": \"Q\",\n"
               "    \"lane10\": \"P\"\n"
               "  }\n"
               "}\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    tenriff::config::KeymapManager manager;
    const auto result = manager.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(manager.bindings_for_mode(result.keymap, "10k").at("lane1") == "Q");
    CHECK(manager.bindings_for_mode(result.keymap, "10k").at("lane10") == "P");
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane1") == "D");
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane4") == "Semicolon");
}

TEST_CASE("keymap load canonicalizes legacy OEM tokens into current names") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "profiles" / "test" / "keymap.json",
               "{\n"
               "  \"layout\": \"multi\",\n"
               "  \"modes\": {\n"
               "    \"4k\": {\n"
               "      \"lane1\": \"D\",\n"
               "      \"lane2\": \"F\",\n"
               "      \"lane3\": \"L\",\n"
               "      \"lane4\": \"VK_OEM_1\"\n"
               "    },\n"
               "    \"10k\": {\n"
               "      \"lane10\": \"[\"\n"
               "    }\n"
               "  }\n"
               "}\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    tenriff::config::KeymapManager manager;
    const auto result = manager.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.normalized_binding_count >= 2);
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane4") == "Semicolon");
    CHECK(manager.bindings_for_mode(result.keymap, "10k").at("lane10") == "LBracket");
}

TEST_CASE("keymap load repairs invalid bindings with per-lane defaults") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    write_file(temp.path / "profiles" / "test" / "keymap.json",
               "{\n"
               "  \"layout\": \"multi\",\n"
               "  \"modes\": {\n"
               "    \"4k\": {\n"
               "      \"lane1\": \"BogusKey\",\n"
               "      \"lane2\": \"F\",\n"
               "      \"lane3\": \"L\",\n"
               "      \"lane4\": \"Semicolon\"\n"
               "    }\n"
               "  }\n"
               "}\n");

    CurrentPathGuard cwd;
    std::error_code ec;
    std::filesystem::current_path(temp.path, ec);
    REQUIRE_FALSE(static_cast<bool>(ec));

    tenriff::config::KeymapManager manager;
    const auto result = manager.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(result.repaired_binding_count == 1);
    REQUIRE_FALSE(result.warnings.empty());
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane1") == "D");
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane2") == "F");
}

TEST_CASE("secondary key bindings round trip for 5K and native BMS layouts without changing primary bindings") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    CurrentPathGuard cwd;
    std::filesystem::current_path(temp.path);

    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.mode_bindings["5k"]["lane1"] = "A";
    keymap.secondary_mode_bindings["5k"]["lane1"] = "S";
    keymap.secondary_mode_bindings["6k"]["lane6"] = "Space";
    keymap.secondary_mode_bindings["8k"]["lane8"] = "VK_OEM_1";
    keymap.secondary_mode_bindings["16k"]["lane16"] = "Backslash";
    REQUIRE(manager.save_profile("profiles/test", keymap));
    const auto loaded = manager.load_profile("profiles/test");
    REQUIRE(loaded.success());
    CHECK(manager.bindings_for_mode(loaded.keymap, "5k").at("lane1") == "A");
    CHECK(manager.secondary_bindings_for_mode(loaded.keymap, "5k").at("lane1") == "S");
    CHECK(manager.secondary_bindings_for_mode(loaded.keymap, "6k").at("lane6") == "Space");
    CHECK(manager.secondary_bindings_for_mode(loaded.keymap, "8k").at("lane8") == "Semicolon");
    CHECK(manager.secondary_bindings_for_mode(loaded.keymap, "16k").at("lane16") == "Backslash");
    CHECK(manager.secondary_bindings_for_mode(loaded.keymap, "4k").empty());

    auto reset = loaded.keymap;
    manager.reset_mode_bindings(reset, "5k");
    CHECK(manager.secondary_bindings_for_mode(reset, "5k").empty());
    CHECK(manager.bindings_for_mode(reset, "5k").at("lane1") == "D");
    CHECK(manager.secondary_bindings_for_mode(reset, "6k").at("lane6") == "Space");
}

TEST_CASE("invalid optional secondary binding is removed instead of becoming a default key") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    write_file(temp.path / "profiles/test/keymap.json",
               R"({"secondary_modes":{"5k":{"lane1":"VK_OEM_1","lane2":"BogusKey","lane3":"","lane99":"A"}}})");
    CurrentPathGuard cwd;
    std::filesystem::current_path(temp.path);
    tenriff::config::KeymapManager manager;
    const auto loaded = manager.load_profile("profiles/test");
    REQUIRE(loaded.success());
    CHECK(loaded.normalized_binding_count == 1);
    CHECK(loaded.repaired_binding_count == 1);
    const auto secondary = manager.secondary_bindings_for_mode(loaded.keymap, "5k");
    CHECK(secondary.size() == 1);
    CHECK(secondary.at("lane1") == "Semicolon");
    CHECK(manager.bindings_for_mode(loaded.keymap, "5k").at("lane2") == "F");
}

TEST_CASE("assigning a physical key transfers primary and secondary slots only in the edited mode") {
    tenriff::config::KeymapManager manager;
    for (const auto& mode : manager.supported_mode_tokens()) {
        for (bool source_secondary : {false, true}) {
            for (bool target_secondary : {false, true}) {
                auto keymap = manager.default_keymap();
                const auto other_modes = keymap.mode_bindings;
                if (source_secondary) keymap.secondary_mode_bindings[mode]["lane1"] = "F12";
                else keymap.mode_bindings[mode]["lane1"] = "F12";
                keymap.secondary_mode_bindings["7k"]["lane7"] = "F11";
                REQUIRE(manager.assign_binding(keymap, mode, "lane2", "F12", target_secondary));
                const auto primary = manager.bindings_for_mode(keymap, mode);
                const auto secondary = manager.secondary_bindings_for_mode(keymap, mode);
                CHECK((target_secondary ? secondary : primary).at("lane2") == "F12");
                if (source_secondary) CHECK(secondary.count("lane1") == 0);
                else CHECK(primary.at("lane1").empty());
                CHECK(manager.validate_unique_bindings(keymap).empty());
                for (const auto& [other_mode, bindings] : other_modes) {
                    if (other_mode != mode) CHECK(keymap.mode_bindings.at(other_mode) == bindings);
                }
                CHECK(keymap.secondary_mode_bindings.at("7k").at("lane7") == "F11");
                CHECK(keymap.bindings == keymap.mode_bindings.at("10k"));
            }
        }
    }
}

TEST_CASE("key transfer compares physical aliases and moves a same-lane primary to secondary") {
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.mode_bindings["4k"]["lane1"] = "VK_OEM_1";
    keymap.secondary_mode_bindings["4k"]["lane2"] = ";";
    keymap.secondary_mode_bindings["4k"]["lane3"] = "LShift";
    REQUIRE_FALSE(manager.validate_unique_bindings(keymap).empty());
    REQUIRE(manager.assign_binding(keymap, "4k", "lane4", ";", true));
    CHECK(keymap.mode_bindings.at("4k").at("lane1").empty());
    CHECK(keymap.mode_bindings.at("4k").at("lane4").empty());
    CHECK(keymap.secondary_mode_bindings.at("4k").count("lane2") == 0);
    CHECK(keymap.secondary_mode_bindings.at("4k").at("lane4") == "Semicolon");
    CHECK(keymap.secondary_mode_bindings.at("4k").at("lane3") == "LShift");
    CHECK(manager.validate_unique_bindings(keymap).empty());
    REQUIRE(manager.assign_binding(keymap, "4k", "lane4", "Semicolon", false));
    CHECK(keymap.mode_bindings.at("4k").at("lane4") == "Semicolon");
    CHECK(keymap.secondary_mode_bindings.at("4k").count("lane4") == 0);
}

TEST_CASE("Shift side assignments move independently and reject invalid capture targets") {
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    REQUIRE(manager.assign_binding(keymap, "10k", "lane1", "LShift", false));
    REQUIRE(manager.assign_binding(keymap, "10k", "lane2", "RShift", false));
    REQUIRE(manager.assign_binding(keymap, "10k", "lane3", "LShift", true));
    CHECK(keymap.mode_bindings.at("10k").at("lane1").empty());
    CHECK(keymap.mode_bindings.at("10k").at("lane2") == "RShift");
    CHECK(keymap.secondary_mode_bindings.at("10k").at("lane3") == "LShift");
    CHECK(keymap.bindings.at("lane1").empty());
    const auto before = keymap;
    CHECK_FALSE(manager.assign_binding(keymap, "10k", "lane99", "RShift", false));
    CHECK_FALSE(manager.assign_binding(keymap, "10k", "lane1", "BogusKey", false));
    CHECK(keymap.mode_bindings == before.mode_bindings);
    CHECK(keymap.secondary_mode_bindings == before.secondary_mode_bindings);
    CHECK(keymap.bindings == before.bindings);
}

TEST_CASE("transferred primary remains unassigned after save and load including legacy 10K mirror") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    REQUIRE(manager.assign_binding(keymap, "4k", "lane2", "D", false));
    REQUIRE(manager.assign_binding(keymap, "10k", "lane2", "Q", true));
    REQUIRE(manager.save_profile(temp.path.u8string(), keymap));
    const auto loaded = manager.load_profile(temp.path.u8string());
    REQUIRE(loaded.success());
    CHECK_FALSE(loaded.rewritten());
    CHECK(loaded.keymap.mode_bindings.at("4k").at("lane1").empty());
    CHECK(loaded.keymap.mode_bindings.at("4k").at("lane2") == "D");
    CHECK(loaded.keymap.mode_bindings.at("10k").at("lane1").empty());
    CHECK(loaded.keymap.bindings.at("lane1").empty());
    CHECK(loaded.keymap.secondary_mode_bindings.at("10k").at("lane2") == "Q");
    CHECK(manager.validate_unique_bindings(loaded.keymap).empty());
}

TEST_CASE("explicit unassigned primary is preserved while missing and invalid keys still use defaults") {
    TempDirGuard temp;
    temp.path = make_temp_dir();
    REQUIRE_FALSE(temp.path.empty());
    write_file(temp.path / "keymap.json",
               R"({"modes":{"4k":{"lane1":"","lane2":"BogusKey"}}})");
    tenriff::config::KeymapManager manager;
    const auto loaded = manager.load_profile(temp.path.u8string());
    REQUIRE(loaded.success());
    CHECK(loaded.repaired_binding_count == 1);
    CHECK(loaded.keymap.mode_bindings.at("4k").at("lane1").empty());
    CHECK(loaded.keymap.mode_bindings.at("4k").at("lane2") == "F");
    CHECK(loaded.keymap.mode_bindings.at("4k").at("lane3") == "L");
    auto reset = loaded.keymap;
    manager.reset_mode_bindings(reset, "4k");
    CHECK(reset.mode_bindings.at("4k").at("lane1") == "D");
}

TEST_CASE("unique binding validation rejects same-slot aliases but keeps left and right Shift distinct") {
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.secondary_mode_bindings["4k"]["lane4"] = "VK_OEM_1";
    const auto duplicates = manager.validate_unique_bindings(keymap);
    REQUIRE(duplicates.size() == 1);
    CHECK(duplicates.front() == "4k:Semicolon");
    keymap.secondary_mode_bindings["4k"].clear();
    keymap.mode_bindings["4k"]["lane1"] = "LShift";
    keymap.secondary_mode_bindings["4k"]["lane1"] = "RShift";
    CHECK(manager.validate_unique_bindings(keymap).empty());
}

TEST_CASE("alternate physical key releases preserve a held logical key and long note") {
    using tenriff::input::InputState;
    tenriff::input::LaneBindingState state;
    state.configure({{10, 1}, {20, 1}, {30, 2}});
    CHECK(state.apply(10, InputState::Pressed) == InputState::Pressed);
    CHECK_FALSE(state.apply(20, InputState::Pressed).has_value());
    CHECK_FALSE(state.apply(10, InputState::Released).has_value());
    CHECK(state.pressed(1));
    CHECK(state.apply(30, InputState::Pressed) == InputState::Pressed);
    CHECK(state.apply(20, InputState::Released) == InputState::Released);
    CHECK_FALSE(state.pressed(1));
    CHECK(state.pressed(2));
    CHECK_FALSE(state.apply(20, InputState::Released).has_value());
    CHECK_FALSE(state.apply(99, InputState::Pressed).has_value());

    state.reset();
    CHECK_FALSE(state.pressed(2));
    CHECK(state.apply(20, InputState::Pressed) == InputState::Pressed);
    CHECK(state.pressed(1));
    CHECK_FALSE(state.apply(10, InputState::Released).has_value());
    CHECK(state.pressed(1));
}

TEST_CASE("secondary key handoff keeps a real charge long note intact until its tail release") {
    using tenriff::input::InputState;
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 5;
    chart.duration_samples = 2500;
    tenriff::gameplay::NoteEvent note;
    note.lane = 3;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);
    tenriff::gameplay::GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10;
    config.judge.gr_ms = 20;
    config.judge.gd_ms = 30;
    config.judge.bd_ms = 40;
    config.judge.hold_grace_ms = 20;
    tenriff::gameplay::GameplayEngine engine(chart, config);
    tenriff::input::LaneBindingState bindings;
    bindings.configure({{10, 3}, {20, 3}});
    const auto dispatch = [&](std::uint32_t key, InputState state, std::int64_t sample) {
        if (const auto logical = bindings.apply(key, state))
            static_cast<void>(engine.handle_input(3, *logical, sample));
        engine.advance(sample);
    };
    dispatch(10, InputState::Pressed, 1000);
    dispatch(20, InputState::Pressed, 1200);
    dispatch(10, InputState::Released, 1400);
    CHECK(bindings.pressed(3));
    CHECK(engine.stats().counts.bd == 0);
    dispatch(20, InputState::Released, 2000);
    engine.advance(2300);
    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.stats().counts.pr == 0);
}
