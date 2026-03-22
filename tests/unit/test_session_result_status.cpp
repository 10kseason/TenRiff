#include "doctest/doctest.h"

#include "app/SessionResultStatus.h"

TEST_CASE("completed gameplay without fail counts as clear") {
    CHECK(tenriff::app::gameplay_session_cleared(true, false, false));
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Normal) == "CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Hard) == "HARD CLEAR");
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::Easy) == "EASY CLEAR");
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
