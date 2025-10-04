#include "doctest/doctest.h"

#include "chart/NoteLaneMapping.h"

using tenriff::chart::NoteLaneMapping;

TEST_CASE("default 10-key dual player mapping covers both players") {
    auto mapping = NoteLaneMapping::TenKeyDualPlayerDefault();

    auto lane_p1_first = mapping.laneForChannel("11");
    CHECK(lane_p1_first.has_value());
    CHECK_EQ(lane_p1_first.value(), 1);

    auto lane_p1_last = mapping.laneForChannel("15");
    CHECK(lane_p1_last.has_value());
    CHECK_EQ(lane_p1_last.value(), 5);

    auto lane_p2_first = mapping.laneForChannel("21");
    CHECK(lane_p2_first.has_value());
    CHECK_EQ(lane_p2_first.value(), 6);

    auto lane_p2_last = mapping.laneForChannel("25");
    CHECK(lane_p2_last.has_value());
    CHECK_EQ(lane_p2_last.value(), 10);
}

TEST_CASE("lane lookup is case-insensitive and configurable") {
    NoteLaneMapping mapping;
    mapping.setMapping({{"aa", 3}});

    auto lane = mapping.laneForChannel("Aa");
    CHECK(lane.has_value());
    CHECK_EQ(lane.value(), 3);

    CHECK_FALSE(mapping.laneForChannel("BB").has_value());
}
