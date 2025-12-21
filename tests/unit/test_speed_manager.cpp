#include <cmath>

#include "doctest/doctest.h"

#include "game/SpeedManager.h"

using tenriff::game::SpeedManager;

namespace {
bool almost_equal(double lhs, double rhs, double eps = 1e-6) {
    return std::abs(lhs - rhs) <= eps;
}
}  // namespace

TEST_CASE("speed manager scales judge windows by rate") {
    SpeedManager manager;
    CHECK(manager.setRate(1.2));
    CHECK(manager.setHiSpeed(3.0));

    CHECK(almost_equal(manager.scaleJudgeWindow(15.5), 12.9166667));
    auto scroll = manager.scrollBps(150.0);
    CHECK(scroll.has_value());
    CHECK(almost_equal(scroll.value(), 375.0));
}

TEST_CASE("speed manager recommends hi-speed from target scroll") {
    SpeedManager manager;
    manager.setRate(1.0);

    auto hs = manager.recommendHiSpeed(150.0, 380.0);
    CHECK(hs.has_value());
    CHECK(almost_equal(hs.value(), 2.5333333));

    manager.setRate(1.2);
    auto hs_scaled = manager.recommendHiSpeed(150.0, 380.0);
    CHECK(hs_scaled.has_value());
    CHECK(almost_equal(hs_scaled.value(), 3.04));

    CHECK_FALSE(manager.recommendHiSpeed(0.0, 300.0).has_value());
    CHECK_FALSE(manager.recommendHiSpeed(180.0, -1.0).has_value());
}

