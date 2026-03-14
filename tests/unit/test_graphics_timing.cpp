#include "doctest/doctest.h"

#include "app/GraphicsTiming.h"

namespace {

using tenriff::app::effective_present_refresh_hz;
using tenriff::app::effective_render_fps_limit;

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

} // namespace
