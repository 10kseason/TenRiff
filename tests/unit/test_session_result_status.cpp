#include "doctest/doctest.h"

#include "app/SessionResultStatus.h"

TEST_CASE("session result status labels the final surviving gauge shift tier") {
    CHECK(tenriff::app::gameplay_session_clear_status(
              true, false, false, tenriff::game::GaugeType::ExHard,
              false, false, false, true) == "GAUGE SHIFT EX CLEAR");
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
              true, false, false, tenriff::game::GaugeType::ExHard) == "EX CLEAR");
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

TEST_CASE("pacemaker clears only when the selected end target is met") {
    using namespace tenriff::app;

    CHECK(pacemaker_target_met("accuracy", 95.0, 7000, 95.0, 8000));
    CHECK_FALSE(pacemaker_target_met("accuracy", 94.99, 9000, 95.0, 8000));
    CHECK(pacemaker_target_met("score", 80.0, 8500, 95.0, 8500));
    CHECK_FALSE(pacemaker_target_met("score", 100.0, 8499, 95.0, 8500));

    CHECK(gameplay_session_pacemaker_cleared(true, false, false, false, true));
    CHECK_FALSE(gameplay_session_pacemaker_cleared(true, false, false, false, false));
    CHECK(gameplay_session_pacemaker_status(
              true, false, false, false, "accuracy", true) ==
          "PACEMAKER ACCURACY CLEAR");
    CHECK(gameplay_session_pacemaker_status(
              true, false, false, false, "score", false) ==
          "PACEMAKER SCORE FAILED");
    CHECK(gameplay_session_pacemaker_status(
              true, false, false, true, "score", true) ==
          "AUTOPLAY");
}

TEST_CASE("live pacemaker delta uses judged score weight and remains neutral before notes") {
    tenriff::gameplay::ResultStats stats;
    stats.record_note_total(10);
    CHECK(tenriff::app::pacemaker_live_delta("accuracy", stats, 1.0, 90.0, 8000) == 0.0);
    stats.record_judgement(tenriff::game::Judgement::PG, 0.0, tenriff::gameplay::ComboImpact::Increment, 0.5);
    CHECK(tenriff::app::pacemaker_live_delta("score", stats, 1.0, 90.0, 8000) == doctest::Approx(100.0));
    CHECK(tenriff::app::pacemaker_live_delta("accuracy", stats, 1.0, 90.0, 8000) == doctest::Approx(10.0));
    CHECK(tenriff::app::pacemaker_live_delta("score", stats, 0.5, 90.0, 8000) == doctest::Approx(-150.0));
}
