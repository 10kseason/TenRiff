#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "app/ModeManager.h"

namespace {

struct Span {
    int lane = 0;
    int64_t start = 0;
    int64_t end = 0;
};

bool contains_warning(const std::vector<std::string>& warnings, std::string_view needle) {
    for (const auto& warning : warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool contains_token(const std::vector<std::string>& tokens, std::string_view needle) {
    return std::find(tokens.begin(), tokens.end(), std::string(needle)) != tokens.end();
}

void append_note(tenriff::gameplay::GameplayChart& chart,
                 int lane,
                 int64_t start_sample,
                 std::optional<int64_t> end_sample = std::nullopt,
                 bool release_required = false) {
    tenriff::gameplay::NoteEvent note;
    note.lane = lane;
    note.start_sample = start_sample;
    note.end_sample = end_sample;
    note.release_required = release_required;
    chart.notes.push_back(note);
    chart.duration_samples = std::max(chart.duration_samples, note.end_sample.value_or(note.start_sample));
}

bool has_lane_overlap(const tenriff::gameplay::GameplayChart& chart) {
    const int lane_count = chart.lane_count;
    if (lane_count <= 0) {
        return false;
    }

    std::vector<Span> spans;
    spans.reserve(chart.notes.size());
    for (const auto& note : chart.notes) {
        int64_t end = note.end_sample.value_or(note.start_sample);
        if (end < note.start_sample) {
            end = note.start_sample;
        }
        spans.push_back({note.lane, note.start_sample, end});
    }

    std::sort(spans.begin(), spans.end(), [](const Span& lhs, const Span& rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });

    std::vector<int64_t> lane_end(static_cast<std::size_t>(lane_count), std::numeric_limits<int64_t>::min());
    for (const auto& span : spans) {
        if (span.lane <= 0 || span.lane > lane_count) {
            continue;
        }
        const auto index = static_cast<std::size_t>(span.lane - 1);
        if (span.start <= lane_end[index]) {
            return true;
        }
        lane_end[index] = std::max(lane_end[index], span.end);
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

tenriff::gameplay::GameplayChart make_dense_tap_chart(int lane_count) {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = lane_count;
    chart.duration_samples = 1400;

    for (int lane = 1; lane <= lane_count; ++lane) {
        append_note(chart, lane, 100);
        append_note(chart, lane, 520 + lane * 7);
        append_note(chart, lane, 980 + lane * 5);
    }

    return chart;
}

tenriff::gameplay::GameplayChart make_hold_mix_chart(int lane_count) {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = lane_count;
    chart.duration_samples = 1200;

    append_note(chart, 1, 100, 260, true);
    append_note(chart, std::max(2, lane_count / 2), 140);
    append_note(chart, std::max(2, lane_count / 2), 420, 620, true);
    append_note(chart, lane_count, 680);
    append_note(chart, std::max(1, lane_count - 1), 760, 1040, false);
    append_note(chart, 2, 930);

    return chart;
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

TEST_CASE("mode manager converts an exact deterministic percentage of taps into mixed long notes") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 1200;
    for (int i = 0; i < 10; ++i) {
        append_note(chart, 1, 100 + static_cast<int64_t>(i) * 100);
    }
    append_note(chart, 2, 150, 260, true);

    tenriff::config::ModeConfig mode;
    mode.random_seed = 12345;
    mode.mods = {"ln_mix_30"};

    const auto first = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        1000);
    const auto repeated = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        1000);

    REQUIRE(first.active_mods.size() == 1u);
    CHECK(first.active_mods[0] == "ln_mix_30");
    CHECK(first.mod_multiplier == doctest::Approx(1.0));
    CHECK_FALSE(has_lane_overlap(first.chart));

    int converted_taps = 0;
    std::vector<int64_t> converted_starts;
    std::vector<int64_t> repeated_starts;
    for (const auto& note : first.chart.notes) {
        if (note.lane == 1 && note.end_sample.has_value()) {
            ++converted_taps;
            converted_starts.push_back(note.start_sample);
            CHECK_FALSE(note.release_required);
        }
        if (note.lane == 2) {
            REQUIRE(note.end_sample.has_value());
            CHECK(note.end_sample.value() == 260);
            CHECK(note.release_required);
        }
    }
    for (const auto& note : repeated.chart.notes) {
        if (note.lane == 1 && note.end_sample.has_value()) {
            repeated_starts.push_back(note.start_sample);
        }
    }
    CHECK(converted_taps == 3);
    CHECK(converted_starts == repeated_starts);

    mode.random_seed = 54321;
    const auto reseeded = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        1000);
    std::vector<int64_t> reseeded_starts;
    for (const auto& note : reseeded.chart.notes) {
        if (note.lane == 1 && note.end_sample.has_value()) {
            reseeded_starts.push_back(note.start_sample);
        }
    }
    CHECK(reseeded_starts.size() == 3u);
    CHECK(reseeded_starts != converted_starts);
}

TEST_CASE("mode manager keeps LN mix exclusive with full note structure conversions") {
    const auto active = tenriff::app::normalize_mode_mod_tokens(
        {"full_long_notes", "full_short_notes", "ln_mix_40"});

    REQUIRE(active.size() == 1u);
    CHECK(active[0] == "ln_mix_40");
    CHECK(tenriff::app::mode_mod_summary(active) == "LN 40%");
    CHECK(tenriff::app::equivalent_mode_mod_tokens(active, {"LN Mix 40%"}));
    CHECK_FALSE(tenriff::app::equivalent_mode_mod_tokens(active, {"ln_mix_50"}));
}

TEST_CASE("mode manager LN mix leaves fifty milliseconds before the next same-lane note") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 500;
    append_note(chart, 1, 100);
    append_note(chart, 1, 120);
    append_note(chart, 1, 300);

    tenriff::config::ModeConfig mode;
    mode.mods = {"ln_mix_90"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        1000);

    REQUIRE(result.chart.notes.size() == 3u);
    CHECK_FALSE(result.chart.notes[0].end_sample.has_value());
    REQUIRE(result.chart.notes[1].end_sample.has_value());
    REQUIRE(result.chart.notes[2].end_sample.has_value());
    CHECK(result.chart.notes[1].end_sample.value() == 250);
    CHECK(result.chart.notes[2].start_sample - result.chart.notes[1].end_sample.value() == 50);
    CHECK(result.chart.notes[2].end_sample.value() == 500);
    CHECK_FALSE(has_lane_overlap(result.chart));
}

TEST_CASE("mode manager LN mix defaults its tail clearance to 44.1 kHz") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 44100;
    append_note(chart, 1, 4410);
    append_note(chart, 1, 13230);
    append_note(chart, 1, 26460);

    tenriff::config::ModeConfig mode;
    mode.mods = {"ln_mix_90"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0);

    REQUIRE(result.chart.notes.size() == 3u);
    for (const auto& note : result.chart.notes) {
        REQUIRE(note.end_sample.has_value());
    }
    CHECK(result.chart.notes[1].start_sample - result.chart.notes[0].end_sample.value() == 2205);
    CHECK(result.chart.notes[2].start_sample - result.chart.notes[1].end_sample.value() == 2205);
    CHECK_FALSE(has_lane_overlap(result.chart));
}

TEST_CASE("mode manager LN mix does not extend duplicate same-lane heads") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 500;
    append_note(chart, 1, 100);
    append_note(chart, 1, 100);
    append_note(chart, 1, 300);

    tenriff::config::ModeConfig mode;
    mode.mods = {"ln_mix_90"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        1000);

    REQUIRE(result.chart.notes.size() == 3u);
    CHECK_FALSE(result.chart.notes[0].end_sample.has_value());
    CHECK_FALSE(result.chart.notes[1].end_sample.has_value());
    REQUIRE(result.chart.notes[2].end_sample.has_value());
    CHECK(result.chart.notes[2].end_sample.value() == 500);
}

TEST_CASE("mode manager LN mix does not extend a tap inside an existing same-lane hold") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 700;
    append_note(chart, 1, 100, 300, true);
    append_note(chart, 1, 200);
    append_note(chart, 1, 500);

    tenriff::config::ModeConfig mode;
    mode.mods = {"ln_mix_90"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        1000);

    REQUIRE(result.chart.notes.size() == 3u);
    REQUIRE(result.chart.notes[0].end_sample.has_value());
    CHECK(result.chart.notes[0].end_sample.value() == 300);
    CHECK(result.chart.notes[0].release_required);
    CHECK_FALSE(result.chart.notes[1].end_sample.has_value());
    REQUIRE(result.chart.notes[2].end_sample.has_value());
    CHECK(result.chart.notes[2].end_sample.value() == 700);
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

TEST_CASE("mode manager safely combines key mode, super random, full long notes, and judge hard") {
    tenriff::gameplay::GameplayChart chart = make_dense_tap_chart(8);

    tenriff::config::ModeConfig mode;
    mode.key_mode = "4k";
    mode.random = "super_random";
    mode.random_seed = 2024;
    mode.mods = {"full_long_notes", "judge_hard"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        44100);

    CHECK(result.chart.lane_count == 4);
    CHECK_FALSE(result.chart.notes.empty());
    CHECK_FALSE(has_lane_overlap(result.chart));
    CHECK(contains_token(result.active_mods, "full_long_notes"));
    CHECK(contains_token(result.active_mods, "judge_hard"));
    CHECK(result.judge_window_scale == doctest::Approx(0.85));
    CHECK(result.judge.pg_ms == doctest::Approx(8.5));

    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 4);
        REQUIRE(note.end_sample.has_value());
        CHECK(note.end_sample.value() > note.start_sample);
        CHECK_FALSE(note.release_required);
    }
}

TEST_CASE("mode manager keeps the original lane count when key mode is none") {
    tenriff::gameplay::GameplayChart chart = make_hold_mix_chart(7);

    tenriff::config::ModeConfig mode;
    mode.key_mode = "none";
    mode.random = "off";

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        175.0,
        44100);

    CHECK(result.chart.lane_count == 7);
    CHECK(result.settings.key_mode == tenriff::gameplay::KeyMode::Auto);
    REQUIRE(result.chart.notes.size() == chart.notes.size());
    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        CHECK(result.chart.notes[i].lane == chart.notes[i].lane);
        CHECK(result.chart.notes[i].start_sample == chart.notes[i].start_sample);
        CHECK(result.chart.notes[i].end_sample == chart.notes[i].end_sample);
    }
}

TEST_CASE("mode manager resolves key mode with full short notes and removes redundant no-ln-release") {
    tenriff::gameplay::GameplayChart chart = make_hold_mix_chart(8);

    tenriff::config::ModeConfig mode;
    mode.key_mode = "4k";
    mode.random = "full_random";
    mode.random_seed = 77;
    mode.mods = {"no_ln_release", "full_short_notes"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        0.95,
        180.0,
        44100);

    CHECK(result.chart.lane_count == 4);
    REQUIRE(result.active_mods.size() == 1u);
    CHECK(result.active_mods[0] == "full_short_notes");
    CHECK(contains_warning(result.warnings, "removed because"));
    CHECK(result.rate_multiplier == doctest::Approx(0.75));
    CHECK(result.mod_multiplier == doctest::Approx(0.50));
    CHECK(result.final_multiplier == doctest::Approx(0.50));
    CHECK_FALSE(has_lane_overlap(result.chart));

    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 4);
        CHECK_FALSE(note.end_sample.has_value());
        CHECK_FALSE(note.release_required);
    }
}

TEST_CASE("mode manager converts BMS key mode upward while still applying compatible mods") {
    tenriff::gameplay::GameplayChart chart = make_hold_mix_chart(7);

    tenriff::config::ModeConfig mode;
    mode.key_mode = "10k";
    mode.mods = {"no_ln_release", "judge_easy"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        175.0,
        44100);

    CHECK(result.chart.lane_count == 10);
    CHECK(result.settings.key_mode == tenriff::gameplay::KeyMode::Keys10);
    CHECK(contains_token(result.active_mods, "no_ln_release"));
    CHECK(contains_token(result.active_mods, "judge_easy"));
    CHECK(result.judge_window_scale == doctest::Approx(1.25));
    CHECK(result.judge.pg_ms == doctest::Approx(12.5));
    CHECK_FALSE(has_lane_overlap(result.chart));

    bool saw_hold = false;
    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 10);
        if (note.end_sample.has_value()) {
            saw_hold = true;
            CHECK_FALSE(note.release_required);
        }
    }
    CHECK(saw_hold);
}

TEST_CASE("mode manager converts BMS key mode downward with randomization while preserving invariants") {
    tenriff::gameplay::GameplayChart chart = make_hold_mix_chart(7);

    tenriff::config::ModeConfig mode;
    mode.key_mode = "4k";
    mode.random = "super_random";
    mode.random_seed = 77;
    mode.mods = {"judge_hard"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        175.0,
        44100);

    CHECK(result.chart.lane_count == 4);
    CHECK(result.settings.key_mode == tenriff::gameplay::KeyMode::Keys4);
    CHECK(result.judge_window_scale == doctest::Approx(0.85));
    CHECK_FALSE(has_lane_overlap(result.chart));

    for (const auto& note : result.chart.notes) {
        CHECK(note.lane >= 1);
        CHECK(note.lane <= 4);
    }
}

TEST_CASE("mode manager handles empty charts with key mode and mods without crashing") {
    tenriff::gameplay::GameplayChart chart;

    tenriff::config::ModeConfig mode;
    mode.key_mode = "4k";
    mode.random = "super_random";
    mode.random_seed = 1;
    mode.mods = {"full_long_notes", "judge_hard"};

    const auto result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        make_judge_config(),
        1.0,
        180.0,
        44100);

    CHECK(result.chart.lane_count == 4);
    CHECK(result.chart.notes.empty());
    CHECK(contains_token(result.active_mods, "full_long_notes"));
    CHECK(contains_token(result.active_mods, "judge_hard"));
    CHECK(result.judge_window_scale == doctest::Approx(0.85));
}

TEST_CASE("mode manager key mode combo matrix preserves chart invariants") {
    struct Scenario {
        tenriff::gameplay::GameplayChart chart;
        std::string key_mode;
        std::string random;
        std::vector<std::string> mods;
        int expected_lane_count = 0;
        double expected_judge_scale = 1.0;
    };

    const std::vector<Scenario> scenarios = {
        {make_dense_tap_chart(8), "4k", "off", {}, 4, 1.0},
        {make_dense_tap_chart(8), "4k", "super_random", {"full_long_notes"}, 4, 1.0},
        {make_hold_mix_chart(8), "4k", "full_random", {"full_short_notes"}, 4, 1.0},
        {make_hold_mix_chart(8), "6k", "super_random", {"no_ln_release", "judge_hard"}, 6, 0.85},
        {make_hold_mix_chart(7), "4k", "mirror", {"judge_hard"}, 4, 0.85},
        {make_dense_tap_chart(4), "16k", "off", {"judge_easy"}, 16, 1.25}
    };

    for (const auto& scenario : scenarios) {
        tenriff::config::ModeConfig mode;
        mode.key_mode = scenario.key_mode;
        mode.random = scenario.random;
        mode.random_seed = 4040;
        mode.mods = scenario.mods;

        const auto result = tenriff::app::manage_modes(
            scenario.chart,
            tenriff::app::ChartFormat::Bms,
            mode,
            make_judge_config(),
            1.0,
            180.0,
            44100);

        CHECK(result.chart.lane_count == scenario.expected_lane_count);
        CHECK(result.judge_window_scale == doctest::Approx(scenario.expected_judge_scale));
        CHECK_FALSE(has_lane_overlap(result.chart));

        for (const auto& note : result.chart.notes) {
            CHECK(note.lane >= 1);
            CHECK(note.lane <= scenario.expected_lane_count);
        }

        if (contains_token(result.active_mods, "full_short_notes")) {
            for (const auto& note : result.chart.notes) {
                CHECK_FALSE(note.end_sample.has_value());
                CHECK_FALSE(note.release_required);
            }
        }

        if (contains_token(result.active_mods, "full_long_notes")) {
            for (const auto& note : result.chart.notes) {
                REQUIRE(note.end_sample.has_value());
                CHECK(note.end_sample.value() > note.start_sample);
                CHECK_FALSE(note.release_required);
            }
        }

        if (contains_token(result.active_mods, "no_ln_release") &&
            !contains_token(result.active_mods, "full_short_notes")) {
            for (const auto& note : result.chart.notes) {
                if (note.end_sample.has_value()) {
                    CHECK_FALSE(note.release_required);
                }
            }
        }
    }
}
