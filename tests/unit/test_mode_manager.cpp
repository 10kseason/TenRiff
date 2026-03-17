#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "app/ModeManager.h"

namespace {

bool contains_warning(const std::vector<std::string>& warnings, std::string_view needle) {
    for (const auto& warning : warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

tenriff::config::JudgeConfig make_judge_config() {
    tenriff::config::JudgeConfig judge;
    judge.pg_ms = 10.0;
    judge.gr_ms = 20.0;
    judge.gd_ms = 30.0;
    judge.bd_ms = 40.0;
    judge.hold_grace_ms = 20.0;
    judge.hold_break_ms = 40.0;
    judge.mask_ms = 30.0;
    return judge;
}

}  // namespace

TEST_CASE("mode manager normalizes mods by category and warns on invalid tokens") {
    std::vector<std::string> warnings;
    const auto active = tenriff::app::normalize_mode_mod_tokens(
        {"judge_hard", "Judge Easy", "judge_easy", "remove_speed_changes", "mystery", "no_ln_release"},
        &warnings);

    REQUIRE(active.size() == 2u);
    CHECK(active[0] == "judge_easy");
    CHECK(active[1] == "no_ln_release");
    CHECK(contains_warning(warnings, "Duplicate mod"));
    CHECK(contains_warning(warnings, "excluded"));
    CHECK(contains_warning(warnings, "not recognized"));
}

TEST_CASE("mode manager score multipliers follow the configured boundaries") {
    CHECK(tenriff::app::rate_score_multiplier(0.50) == doctest::Approx(0.50));
    CHECK(tenriff::app::rate_score_multiplier(0.95) == doctest::Approx(0.75));
    CHECK(tenriff::app::rate_score_multiplier(1.00) == doctest::Approx(1.00));
    CHECK(tenriff::app::rate_score_multiplier(1.05) == doctest::Approx(1.05));
    CHECK(tenriff::app::rate_score_multiplier(2.00) == doctest::Approx(1.15));

    CHECK(tenriff::app::mod_score_multiplier({"judge_hard"}) == doctest::Approx(1.10));
    CHECK(tenriff::app::final_score_multiplier({"judge_hard"}, 2.00) == doctest::Approx(1.10));
    CHECK(tenriff::app::final_score_multiplier({"full_short_notes"}, 0.95) == doctest::Approx(0.50));
}

TEST_CASE("mode manager converts taps into standard holds for full long notes") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 500;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 100, std::nullopt});
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 200, std::nullopt});
    chart.notes.push_back(tenriff::gameplay::NoteEvent{2, 300, std::nullopt});

    tenriff::config::ModeConfig mode;
    mode.mods = {"full_long_notes"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0);

    REQUIRE(result.active_mods.size() == 1u);
    CHECK(result.active_mods[0] == "full_long_notes");
    REQUIRE(result.chart.notes.size() == 3u);
    REQUIRE(result.chart.notes[0].end_sample.has_value());
    REQUIRE(result.chart.notes[1].end_sample.has_value());
    REQUIRE(result.chart.notes[2].end_sample.has_value());
    CHECK(result.chart.notes[0].end_sample.value() == 199);
    CHECK(result.chart.notes[1].end_sample.value() == 500);
    CHECK(result.chart.notes[2].end_sample.value() == 500);
    CHECK_FALSE(result.chart.notes[0].release_required);
    CHECK_FALSE(result.chart.notes[1].release_required);
    CHECK_FALSE(result.chart.notes[2].release_required);
}

TEST_CASE("mode manager removes no-ln-release when full short notes is active") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 400;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 100, 250});
    chart.notes.back().release_required = true;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 300, std::nullopt});

    tenriff::config::ModeConfig mode;
    mode.mods = {"no_ln_release", "full_short_notes"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0);

    REQUIRE(result.active_mods.size() == 1u);
    CHECK(result.active_mods[0] == "full_short_notes");
    CHECK(contains_warning(result.warnings, "removed because"));
    for (const auto& note : result.chart.notes) {
        CHECK_FALSE(note.end_sample.has_value());
        CHECK_FALSE(note.release_required);
    }
}

TEST_CASE("mode manager scales judge windows without touching the mask window") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 100;

    tenriff::config::ModeConfig easy_mode;
    easy_mode.mods = {"judge_easy"};
    const auto easy = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        easy_mode,
        make_judge_config(),
        1.0);
    CHECK(easy.judge_window_scale == doctest::Approx(1.25));
    CHECK(easy.judge.pg_ms == doctest::Approx(12.5));
    CHECK(easy.judge.hold_grace_ms == doctest::Approx(25.0));
    CHECK(easy.judge.hold_break_ms == doctest::Approx(50.0));
    CHECK(easy.judge.mask_ms == doctest::Approx(30.0));

    tenriff::config::ModeConfig hard_mode;
    hard_mode.mods = {"judge_hard"};
    const auto hard = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        hard_mode,
        make_judge_config(),
        1.0);
    CHECK(hard.judge_window_scale == doctest::Approx(0.85));
    CHECK(hard.judge.pg_ms == doctest::Approx(8.5));
    CHECK(hard.judge.hold_grace_ms == doctest::Approx(17.0));
    CHECK(hard.judge.hold_break_ms == doctest::Approx(34.0));
    CHECK(hard.judge.mask_ms == doctest::Approx(30.0));
}
