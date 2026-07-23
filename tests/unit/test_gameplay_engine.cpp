#include "doctest/doctest.h"

#include <cmath>

#include "app/ModeManager.h"
#include "gameplay/GameplayEngine.h"

using tenriff::gameplay::GameplayChart;
using tenriff::gameplay::GameplayConfig;
using tenriff::gameplay::NoteEvent;
using tenriff::gameplay::GameplayEngine;
using tenriff::input::InputState;
using tenriff::game::Judgement;

TEST_CASE("gameplay engine scores a basic hit") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(1500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().combo == 1);
    CHECK(engine.stats().max_combo == 1);
}

TEST_CASE("gameplay engine keeps real-time judge windows unchanged at faster rate") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.5;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    const int64_t playback_note_sample = static_cast<int64_t>(std::llround(1000.0 / config.rate));
    chart.notes.push_back(NoteEvent{1, playback_note_sample, std::nullopt});

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, playback_note_sample + 30);
    engine.advance(playback_note_sample + 100);

    CHECK(engine.stats().counts.pg == 0);
    CHECK(engine.stats().counts.gr == 0);
    CHECK(engine.stats().counts.gd == 1);
    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.live_feedback().has_value);
    CHECK(engine.live_feedback().delta_ms == doctest::Approx(30.0));
}

TEST_CASE("gameplay engine treats LR2-style early non-consuming input as poor") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 100);

    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.stats().counts.pr == 1);
    CHECK(engine.stats().combo == 0);
}

TEST_CASE("gameplay engine raw score keeps earlier penalties after later hits") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 100);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(1500);

    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.stats().counts.pr == 1);
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().raw_score == 1000);
}

TEST_CASE("gameplay engine ignores inputs that are too early for LR2 poor") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes.push_back(NoteEvent{1, 2500, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);

    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.stats().counts.pr == 0);
}

TEST_CASE("gameplay engine does not add LR2 poor after consuming a miss as bad") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1500);

    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().counts.pr == 0);
}

TEST_CASE("gameplay engine scores hold tail based on release timing") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 20.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 2000);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine gives charge holds a grace window around the tail") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 40.0;
    config.judge.hold_break_ms = 100.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1970);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.gr == 0);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine gives charge holds a softer bad window before the tail") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 35.0;
    config.judge.hold_break_ms = 100.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1935);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine auto-clears standard hold tails without release timing") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.stats().raw_score == 1000);
}

TEST_CASE("gameplay engine keeps the latest one hundred timing deltas in order") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 130000;
    for (int i = 0; i < 120; ++i) {
        chart.notes.push_back(NoteEvent{1, 1000 + static_cast<int64_t>(i) * 1000, std::nullopt});
    }

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 30.0;
    config.judge.gd_ms = 60.0;
    config.judge.bd_ms = 100.0;

    GameplayEngine engine(chart, config);
    for (int i = 0; i < 120; ++i) {
        const int64_t start_sample = 1000 + static_cast<int64_t>(i) * 1000;
        const int64_t delta_samples = static_cast<int64_t>(i - 60);
        (void)engine.handle_input(1, InputState::Pressed, start_sample + delta_samples);
    }
    engine.advance(130000);

    std::array<double, tenriff::kGameplayTimingHistoryMaxEntries> timing_history{};
    std::size_t timing_history_count = 0;
    engine.collect_recent_timing_deltas(timing_history, &timing_history_count);

    CHECK(timing_history_count == tenriff::kGameplayTimingHistoryMaxEntries);
    CHECK(timing_history.front() == doctest::Approx(-40.0));
    CHECK(timing_history.back() == doctest::Approx(59.0));
}

TEST_CASE("gameplay engine applies hold judgement weight to gauge deltas") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1500);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.gauge_state().value == doctest::Approx(96.87500000));
}

TEST_CASE("gameplay engine applies easy low-gauge softening to weighted hold bads") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 5000;
    for (int i = 0; i < 19; ++i) {
        chart.notes.push_back(NoteEvent{1, 1000 + static_cast<int64_t>(i) * 100, std::nullopt});
    }
    chart.notes.push_back(NoteEvent{1, 3000, 4000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.initial_gauge = tenriff::game::GaugeType::Easy;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 20.0;
    config.judge.hold_break_ms = 100.0;

    GameplayEngine engine(chart, config);
    engine.advance(2841);
    CHECK(engine.gauge_state().value == doctest::Approx(22.1));

    (void)engine.handle_input(1, InputState::Pressed, 3000);
    (void)engine.handle_input(1, InputState::Released, 3200);
    engine.advance(4100);

    CHECK(engine.stats().counts.bd == 20);
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.gauge_state().value == doctest::Approx(20.38));
}

TEST_CASE("gameplay engine marks early hold release as bad") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 20.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1500);
    engine.advance(2500);

    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().counts.pg == 1);
}

TEST_CASE("gameplay engine keeps standard holds alive through a short early release") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 35.0;
    config.judge.hold_break_ms = 100.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1940);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine marks unreleased charge hold tails as bad after the tail window") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.hold_grace_ms = 0.0;
    config.judge.hold_break_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(2041);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.bd == 1);
}

TEST_CASE("gameplay engine returns the hit note metadata for successful presses") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    const std::size_t asset_id = chart.intern_audio_asset("sample.wav");

    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.audio_asset_id = asset_id;
    chart.notes.push_back(note);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    auto hit_note = engine.handle_input(1, InputState::Pressed, 1000);

    REQUIRE(hit_note.has_value());
    CHECK(hit_note->audio_asset_id == asset_id);
    REQUIRE(chart.audio_asset_path(hit_note->audio_asset_id) != nullptr);
    CHECK(*chart.audio_asset_path(hit_note->audio_asset_id) == "sample.wav");
    CHECK(hit_note->start_sample == 1000);
}

TEST_CASE("gameplay engine catch-up sync does not score ghost presses") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    engine.sync_input_state(1, InputState::Pressed, 500);

    CHECK(engine.stats().counts.pg == 0);
    CHECK(engine.stats().counts.gr == 0);
    CHECK(engine.stats().counts.gd == 0);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine catch-up sync preserves stale hold release timing") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.sync_input_state(1, InputState::Released, 1500);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.bd == 1);
}

TEST_CASE("gameplay engine prestart baseline press does not block the first real hit") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    engine.sync_input_state(1, InputState::Pressed, 0);
    engine.sync_input_state(1, InputState::Released, 900);

    auto hit_note = engine.handle_input(1, InputState::Pressed, 1000);

    REQUIRE(hit_note.has_value());
    CHECK(hit_note->start_sample == 1000);
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine exposes active holds after a successful hold head hit") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    auto hit_note = engine.handle_input(1, InputState::Pressed, 1000);

    REQUIRE(hit_note.has_value());
    std::vector<tenriff::gameplay::ActiveHoldView> holds;
    engine.collect_active_holds(holds);

    REQUIRE(holds.size() == 1u);
    CHECK(holds[0].lane == 1);
    CHECK(holds[0].end_sample == 2000);
}

TEST_CASE("gameplay engine finishes as soon as the final judged note is cleared") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 10000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(1000);

    CHECK(engine.is_finished());
    CHECK_FALSE(engine.is_game_over());
}

TEST_CASE("gameplay engine auto-misses once the bad window is exceeded") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    engine.advance(1040);
    CHECK(engine.stats().counts.bd == 0);

    engine.advance(1041);
    CHECK(engine.stats().counts.bd == 1);
}

TEST_CASE("practice no-fail prevents hard gauge game over") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 8000;
    for (int i = 0; i < 12; ++i) {
        chart.notes.push_back(NoteEvent{1, 1000 + static_cast<int64_t>(i) * 100, std::nullopt});
    }

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.initial_gauge = tenriff::game::GaugeType::Hard;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine normal_engine(chart, config);
    normal_engine.advance(4000);
    CHECK(normal_engine.is_game_over());

    config.practice_no_fail_enabled = true;
    GameplayEngine practice_engine(chart, config);
    practice_engine.advance(4000);
    CHECK_FALSE(practice_engine.is_game_over());
    CHECK(practice_engine.is_finished());
    CHECK(practice_engine.stats().counts.bd >= 10);
}

TEST_CASE("sudden death stops on the first bad even when practice no-fail is set") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 4000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{2, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.practice_no_fail_enabled = true;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    engine.advance(1041);

    CHECK(engine.is_game_over());
    CHECK_FALSE(engine.is_finished());
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().osu_od8.counts.miss == 1);
    CHECK(engine.gauge_state().value == doctest::Approx(0.0));
}

TEST_CASE("sudden death ignores empty poor presses but kills on a timed bad") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 140.0;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 100);
    CHECK(engine.stats().counts.pr == 1);
    CHECK_FALSE(engine.is_game_over());

    (void)engine.handle_input(1, InputState::Released, 200);
    (void)engine.handle_input(1, InputState::Pressed, 1100);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().osu_od8.counts.ok == 1);
    CHECK(engine.is_game_over());
}

TEST_CASE("gameplay engine exposes OD8 ScoreV1 and scores a hold as one osu object") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent hold;
    hold.lane = 1;
    hold.start_sample = 1000;
    hold.end_sample = 2000;
    chart.notes.push_back(hold);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    config.judge.gd_ms = 75.0;
    config.judge.bd_ms = 340.0;
    config.judge.hold_grace_ms = 80.0;
    config.judge.hold_break_ms = 200.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1010);
    engine.advance(2000);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().osu_od8.total_objects == 1);
    CHECK(engine.stats().osu_od8.judged_objects == 1);
    CHECK(engine.stats().osu_od8.counts.perfect == 1);
    CHECK(engine.stats().osu_od8.score == 1'000'000);
}

TEST_CASE("gameplay engine records one multiplayer gauge shift before Easy game over") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 2000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.gauge.normal.bd = -70.0;
    config.gauge.easy.bd = -100.0;
    config.gauge_policy.normal_to_easy_shift = true;

    GameplayEngine engine(chart, config);
    engine.advance(1041);

    REQUIRE(engine.stats().shifts.size() == 1u);
    CHECK(engine.stats().shifts[0].from == tenriff::game::GaugeType::Normal);
    CHECK(engine.stats().shifts[0].to == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().value == doctest::Approx(100.0));
    CHECK_FALSE(engine.is_game_over());

    engine.advance(2041);

    CHECK(engine.stats().shifts.size() == 1u);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().value == doctest::Approx(0.0));
    CHECK(engine.is_game_over());
}

TEST_CASE("gameplay engine does not shift later notes after a bad on the same lane") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 2000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1500);
    (void)engine.handle_input(1, InputState::Released, 1600);

    auto next_hit = engine.handle_input(1, InputState::Pressed, 2000);

    REQUIRE(next_hit.has_value());
    CHECK(next_hit->start_sample == 2000);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().combo == 1);
}

TEST_CASE("gameplay engine recovers from a missed dense note without chaining bad judgements") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 1210, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 1420, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 1630, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    config.judge.gd_ms = 75.0;
    config.judge.bd_ms = 340.0;
    config.judge.mask_ms = 30.0;

    GameplayEngine engine(chart, config);
    auto first_hit = engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1020);

    REQUIRE(first_hit.has_value());
    CHECK(first_hit->start_sample == 1000);

    // The 1210 note is intentionally missed. Both later presses are exact for their
    // intended notes and must not be consumed as late BADs on the preceding note.
    auto recovered_hit = engine.handle_input(1, InputState::Pressed, 1420);
    (void)engine.handle_input(1, InputState::Released, 1440);
    auto following_hit = engine.handle_input(1, InputState::Pressed, 1630);

    REQUIRE(recovered_hit.has_value());
    CHECK(recovered_hit->start_sample == 1420);
    REQUIRE(following_hit.has_value());
    CHECK(following_hit->start_sample == 1630);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().counts.pg == 3);
    CHECK(engine.stats().combo == 2);
}

TEST_CASE("late miss recovery press scores the clearly closer next note") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 1060, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.mask_ms = 30.0;

    GameplayEngine engine(chart, config);
    auto recovered = engine.handle_input(1, InputState::Pressed, 1045);

    REQUIRE(recovered.has_value());
    CHECK(recovered->start_sample == 1060);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().counts.pg == 0);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().combo == 1);
}

TEST_CASE("judge easy mod expands charge hold tail windows during gameplay") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);

    tenriff::config::ModeConfig mode;
    mode.mods = {"judge_easy"};

    tenriff::config::JudgeConfig judge;
    judge.pg_ms = 10.0;
    judge.gr_ms = 20.0;
    judge.gd_ms = 30.0;
    judge.bd_ms = 40.0;
    judge.hold_grace_ms = 20.0;
    judge.hold_break_ms = 40.0;

    const auto mode_result =
        tenriff::app::manage_modes(chart, tenriff::app::ChartFormat::Bms, mode, judge, 1.0);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge = mode_result.judge;

    GameplayEngine engine(mode_result.chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 2024);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.gr == 0);
}

TEST_CASE("judge hard mod shrinks charge hold tail windows during gameplay") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 2000;
    note.release_required = true;
    chart.notes.push_back(note);

    tenriff::config::ModeConfig mode;
    mode.mods = {"judge_hard"};

    tenriff::config::JudgeConfig judge;
    judge.pg_ms = 10.0;
    judge.gr_ms = 20.0;
    judge.gd_ms = 30.0;
    judge.bd_ms = 40.0;
    judge.hold_grace_ms = 20.0;
    judge.hold_break_ms = 40.0;

    const auto mode_result =
        tenriff::app::manage_modes(chart, tenriff::app::ChartFormat::Bms, mode, judge, 1.0);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge = mode_result.judge;

    GameplayEngine engine(mode_result.chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 2018);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 1);
}

TEST_CASE("full long notes preserve raw score potential during gameplay") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    tenriff::config::ModeConfig mode;
    mode.mods = {"full_long_notes"};

    const auto mode_result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        tenriff::config::JudgeConfig{},
        1.0);

    REQUIRE(mode_result.chart.notes.size() == 1u);
    REQUIRE(mode_result.chart.notes[0].end_sample.has_value());

    GameplayConfig config;
    config.sample_rate = 1000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(mode_result.chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().raw_score == 1000);
}
