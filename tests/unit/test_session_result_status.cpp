#include "doctest/doctest.h"

#include "app/SessionResultStatus.h"

TEST_CASE("session result status labels the final surviving gauge shift tier") {
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::ExHard,
              false, false, false, true) == "GAUGE SHIFT EX-HARD CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Hard,
              false, false, false, true) == "GAUGE SHIFT HARD CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Normal,
              false, false, false, true) == "GAUGE SHIFT NORMAL CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Easy,
              false, false, false, true) == "GAUGE SHIFT EASY CLEAR");
}
TEST_CASE("completed gameplay without fail counts as clear") {
    CHECK(tenriff::app::gameplay_session_cleared(true, false, false));
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Normal) == "CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::ExHard) == "EX-HARD CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Hard) == "HARD CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Easy) == "EASY CLEAR");
}

TEST_CASE("autoplay completion never counts as a clear") {
    CHECK_FALSE(tenriff::app::gameplay_session_cleared(true, false, false, true));
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Normal, true, false) ==
          "AUTOPLAY");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::ExHard, true, true, true, true) ==
          "AUTOPLAY");
}

TEST_CASE("unfinished or aborted gameplay never counts as clear") {
    CHECK_FALSE(tenriff::app::gameplay_session_cleared(false, false, false));
    CHECK(tenriff::app::gameplay_session_clear_status(
              false, false, false, tenriff::game::GaugeType::Normal) == "FAILED");

    CHECK_FALSE(tenriff::app::gameplay_session_cleared(true, true, false));
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, true, false, tenriff::game::GaugeType::Normal) == "FAILED");

    CHECK_FALSE(tenriff::app::gameplay_session_cleared(true, false, true));
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, true, tenriff::game::GaugeType::Normal) == "ABORTED");
}

TEST_CASE("practice assist clears are labeled explicitly") {
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Hard, false, true) ==
          "ASSIST PRACTICE HARD CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              false, false, false, tenriff::game::GaugeType::Normal, true, false) == "FAILED");
}

TEST_CASE("sudden death clears are labeled above the underlying gauge") {
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Normal, false, false, true) ==
          "SUDDEN DEATH CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Hard, true, false, true) ==
          "AUTOPLAY");
}
