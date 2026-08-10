#include "doctest/doctest.h"

#include "app/MenuAppSettingsUtils.h"

TEST_CASE("Song Select gauge clicks follow the player-facing gauge order") {
    using tenriff::app::cycle_gauge_mode;

    CHECK(cycle_gauge_mode("shift", 1) == "easy");
    CHECK(cycle_gauge_mode("easy", 1) == "normal");
    CHECK(cycle_gauge_mode("normal", 1) == "hard");
    CHECK(cycle_gauge_mode("hard", 1) == "ex_hard");
    CHECK(cycle_gauge_mode("ex_hard", 1) == "easy");
    CHECK(cycle_gauge_mode("easy", -1) == "ex_hard");
}

TEST_CASE("Song Select random clicks cycle both directions") {
    using tenriff::app::cycle_random_mode;

    CHECK(cycle_random_mode("off", 1) == "mirror");
    CHECK(cycle_random_mode("mirror", 1) == "fr");
    CHECK(cycle_random_mode("fr", 1) == "rr");
    CHECK(cycle_random_mode("rr", 1) == "sr");
    CHECK(cycle_random_mode("sr", 1) == "off");
    CHECK(cycle_random_mode("off", -1) == "sr");
}

TEST_CASE("Song Select quick settings map left and right clicks to all four values") {
    tenriff::config::RuntimeConfig runtime;
    runtime.visual_offset_ms = 0.0;
    runtime.speed.hi_speed = 13.25;
    runtime.mode.gauge = "ex_hard";
    runtime.mode.random = "off";

    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 0, 1));
    CHECK(runtime.visual_offset_ms == doctest::Approx(5.0));
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 0, -1));
    CHECK(runtime.visual_offset_ms == doctest::Approx(0.0));
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 1, 1));
    CHECK(runtime.speed.hi_speed == doctest::Approx(13.26));
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 1, -1));
    CHECK(runtime.speed.hi_speed == doctest::Approx(13.25));
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 2, 1));
    CHECK(runtime.mode.gauge == "easy");
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 2, -1));
    CHECK(runtime.mode.gauge == "ex_hard");
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 3, 1));
    CHECK(runtime.mode.random == "mirror");
    CHECK(tenriff::app::adjust_song_quick_setting(runtime, 3, -1));
    CHECK(runtime.mode.random == "off");
    CHECK_FALSE(tenriff::app::adjust_song_quick_setting(runtime, 4, 1));
}

TEST_CASE("difficulty table selection upgrades hashless Fast indexing") {
    tenriff::config::RuntimeConfig runtime;
    runtime.mode.song_index_profile = "fast";

    CHECK(tenriff::app::ensure_difficulty_table_indexing(runtime));
    CHECK(runtime.mode.song_index_profile == "safe");
    CHECK_FALSE(tenriff::app::ensure_difficulty_table_indexing(runtime));
    CHECK(runtime.mode.song_index_profile == "safe");

    runtime.mode.song_index_profile = "invalid";
    CHECK_FALSE(tenriff::app::ensure_difficulty_table_indexing(runtime));
}
