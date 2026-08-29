#include "doctest/doctest.h"

#include "app/GraphicsTiming.h"

namespace {

using tenriff::app::effective_present_refresh_hz;
using tenriff::app::effective_render_fps_limit;
using tenriff::app::cycle_graphics_refresh_hz;
using tenriff::app::normalize_graphics_refresh_hz;
using tenriff::app::should_allow_tearing_present;
using tenriff::app::should_record_presented_frame;
using tenriff::app::should_treat_present_failure_as_transient;

TEST_CASE("match display follows the detected refresh when vsync is disabled") {
    CHECK(effective_present_refresh_hz(false, -1, 144, false) == 144);
    CHECK(effective_render_fps_limit(false, -1, 144, false) == 144);
    CHECK(effective_present_refresh_hz(false, -1, 240, true) == 240);
    CHECK(effective_render_fps_limit(false, -1, 240, true) == 240);

    // Legacy fixed caps are interpreted as Match Display until persisted again.
    CHECK(effective_render_fps_limit(false, 1050, 144, true) == 144);
}

TEST_CASE("graphics timing exposes only match display and unlimited") {
    CHECK(cycle_graphics_refresh_hz(60, -1) == 0);
    CHECK(cycle_graphics_refresh_hz(0, -1) == -1);
    CHECK(cycle_graphics_refresh_hz(1050, 1) == 0);
    CHECK(cycle_graphics_refresh_hz(0, 1) == -1);
    CHECK(normalize_graphics_refresh_hz(0) == 0);
    CHECK(normalize_graphics_refresh_hz(-1) == -1);
    CHECK(normalize_graphics_refresh_hz(5000) == -1);
}

TEST_CASE("unlimited caps off-vsync gameplay rendering at 1500 fps") {
    CHECK(effective_present_refresh_hz(false, 0, 144, true) == 144);
    CHECK(effective_render_fps_limit(false, 0, 144, true) ==
          tenriff::app::kGraphicsUnlimitedFpsCap);
    CHECK(effective_render_fps_limit(false, 0, 144, false) == 300);
    CHECK(effective_present_refresh_hz(true, 0, 144, true) == 144);
    CHECK(effective_render_fps_limit(true, 0, 144, true) == 288);
}

TEST_CASE("graphics timing uses the detected monitor refresh when vsync is enabled") {
    CHECK(effective_present_refresh_hz(true, -1, 144, false) == 144);
    CHECK(effective_render_fps_limit(true, -1, 144, false) == 288);
    CHECK(effective_present_refresh_hz(true, 0, 240, true) == 240);
    CHECK(effective_render_fps_limit(true, 0, 240, true) == 480);
}

TEST_CASE("graphics timing clamps doubled vsync fps into the supported max") {
    CHECK(effective_present_refresh_hz(true, 120, 600, false) == 600);
    CHECK(effective_render_fps_limit(true, 120, 600, false) == 1050);
}

TEST_CASE("fullscreen present disables tearing even when vsync is off") {
    CHECK(should_allow_tearing_present(false, false, true));
    CHECK_FALSE(should_allow_tearing_present(false, true, true));
    CHECK_FALSE(should_allow_tearing_present(true, false, true));
    CHECK_FALSE(should_allow_tearing_present(false, false, false));
}

TEST_CASE("performance tracking accepts only fully presented GPU frames") {
    CHECK(should_record_presented_frame(0x00000000u));
    CHECK_FALSE(should_record_presented_frame(0x087A0001u));
    CHECK_FALSE(should_record_presented_frame(0x087A0007u));
    CHECK_FALSE(should_record_presented_frame(0x887A0001u));
}

TEST_CASE("alt-tab fullscreen present failures are treated as transient") {
    CHECK(should_treat_present_failure_as_transient(0x887A0001u, true, false, false));
    CHECK(should_treat_present_failure_as_transient(0x087A0001u, false, false, false));
    CHECK(should_treat_present_failure_as_transient(0x087A0007u, false, true, false));
    CHECK(should_treat_present_failure_as_transient(0x887A0001u, false, true, true));
    CHECK_FALSE(should_treat_present_failure_as_transient(0x887A0001u, false, true, false));
}

} // namespace
