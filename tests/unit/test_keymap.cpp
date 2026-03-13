#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "doctest/doctest.h"

#include "config/Keymap.h"

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

TEST_CASE("default keymap exposes separate mode bindings from 4K through 10K") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();

    CHECK(keymap.mode_bindings.count("4k") == 1u);
    CHECK(keymap.mode_bindings.count("5k") == 1u);
    CHECK(keymap.mode_bindings.count("7k") == 1u);
    CHECK(keymap.mode_bindings.count("8k") == 1u);
    CHECK(keymap.mode_bindings.count("10k") == 1u);
    CHECK(manager.lane_ids_for_mode("4k").size() == 4u);
    CHECK(manager.lane_ids_for_mode("9k").size() == 9u);
    CHECK(manager.bindings_for_mode(keymap, "4k").at("lane4") == "Semicolon");
    CHECK(manager.bindings_for_mode(keymap, "5k").at("lane3") == "K");
    CHECK(manager.bindings_for_mode(keymap, "7k").at("lane4") == "M");
    CHECK(manager.bindings_for_mode(keymap, "8k").at("lane4") == "V");
    CHECK(manager.bindings_for_mode(keymap, "10k").at("lane1") == "Q");
    CHECK(manager.bindings_for_mode(keymap, "10k").at("lane5") == "V");
    CHECK(manager.bindings_for_mode(keymap, "10k").at("lane10") == "LBracket");
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

    std::string error;
    REQUIRE(manager.save_profile("profiles/test", keymap, &error));
    CHECK(error.empty());

    const auto result = manager.load_profile("profiles/test");
    REQUIRE(result.success());
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane1") == "A");
    CHECK(manager.bindings_for_mode(result.keymap, "4k").at("lane4") == "Semicolon");
    CHECK(manager.bindings_for_mode(result.keymap, "7k").at("lane4") == "Enter");
    CHECK(manager.bindings_for_mode(result.keymap, "10k").at("lane5") == "V");
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
