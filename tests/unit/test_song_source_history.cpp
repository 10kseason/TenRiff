#include "doctest/doctest.h"
#include "app/SongSourceHistory.h"
#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("source removal only removes the saved reference and preserves chart files") {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / fs::u8path(u8"소스_목록_삭제_") /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{base};
    fs::create_directories(base / "first");
    fs::create_directories(base / "second");
    const auto chart = base / "first" / "keep.bms";
    { std::ofstream file(chart); file << "#TITLE Keep\n#BPM 120\n"; }
    tenriff::config::RuntimeConfig runtime;
    runtime.ui.active_song_source = (base / "first").u8string();
    runtime.ui.recent_song_sources = {runtime.ui.active_song_source, (base / "second").u8string()};
    int selected = 0;
    CHECK(tenriff::app::remove_song_source_history(runtime.ui, selected));
    CHECK(runtime.ui.recent_song_sources.size() == 1);
    CHECK(runtime.ui.active_song_source.empty());
    CHECK(fs::is_regular_file(chart));
    CHECK(fs::is_directory(base / "first"));
    CHECK_FALSE(tenriff::app::song_source_history_is_intentionally_empty(runtime.ui));
    CHECK(tenriff::app::remove_song_source_history(runtime.ui, selected));
    CHECK(tenriff::app::song_source_history_is_intentionally_empty(runtime.ui));
    CHECK_FALSE(tenriff::app::remove_song_source_history(runtime.ui, selected));
    CHECK(selected == 0);
    const auto profile = base / "profile";
    tenriff::config::ConfigLoader loader;
    std::string error;
    REQUIRE(loader.save_profile(profile.u8string(), runtime, &error));
    const auto restored = loader.load_profile(profile.u8string());
    REQUIRE(restored.success());
    CHECK(tenriff::app::song_source_history_is_intentionally_empty(restored.config.ui));
    CHECK(fs::is_regular_file(chart));
}

TEST_CASE("removing an inactive source keeps the active source and clamps selection") {
    tenriff::config::UiConfig ui;
    ui.active_song_source = "active";
    ui.recent_song_sources = {"active", "inactive"};
    int selected = 1;
    REQUIRE(tenriff::app::remove_song_source_history(ui, selected));
    CHECK(ui.active_song_source == "active");
    CHECK(selected == 0);
    CHECK_FALSE(tenriff::app::song_source_history_is_intentionally_empty(ui));
}
