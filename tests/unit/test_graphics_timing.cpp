#include "doctest/doctest.h"

#include "app/GraphicsTiming.h"

namespace {

using tenriff::app::effective_present_refresh_hz;
using tenriff::app::effective_render_fps_limit;
using tenriff::app::should_allow_tearing_present;
using tenriff::app::should_treat_present_failure_as_transient;

TEST_CASE("graphics timing keeps the configured cap when vsync is disabled") {
    CHECK(effective_present_refresh_hz(false, 1000, 144, false) == 300);
    CHECK(effective_render_fps_limit(false, 1000, 144, false) == 300);
    CHECK(effective_present_refresh_hz(false, 240, 144, true) == 240);
    CHECK(effective_render_fps_limit(false, 240, 144, true) == 240);
}

TEST_CASE("graphics timing uses the detected monitor refresh when vsync is enabled") {
    CHECK(effective_present_refresh_hz(true, 1050, 144, false) == 144);
    CHECK(effective_render_fps_limit(true, 1050, 144, false) == 288);
    CHECK(effective_present_refresh_hz(true, 300, 240, true) == 240);
    CHECK(effective_render_fps_limit(true, 300, 240, true) == 480);
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

TEST_CASE("alt-tab fullscreen present failures are treated as transient") {
    CHECK(should_treat_present_failure_as_transient(0x887A0001u, true, false, false));
    CHECK(should_treat_present_failure_as_transient(0x087A0001u, false, false, false));
    CHECK(should_treat_present_failure_as_transient(0x087A0007u, false, true, false));
    CHECK(should_treat_present_failure_as_transient(0x887A0001u, false, true, true));
    CHECK_FALSE(should_treat_present_failure_as_transient(0x887A0001u, false, true, false));
}

} // namespace
