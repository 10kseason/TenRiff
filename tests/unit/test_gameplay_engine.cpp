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

TEST_CASE("native score is normalized to one hundred thousand with a combo component") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 2000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 3000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(2100);
    (void)engine.handle_input(1, InputState::Pressed, 3000);
    engine.advance(4000);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().combo_score_units == 2);
    CHECK(engine.stats().raw_score == 63'333);
}

TEST_CASE("native accuracy rewards timing inside every judgement band and caps loose PG clusters") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 2000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine exact(chart, config);
    (void)exact.handle_input(1, InputState::Pressed, 1000);
    (void)exact.handle_input(1, InputState::Pressed, 2000);
    CHECK(exact.stats().accuracy_percent() == doctest::Approx(100.0));

    GameplayEngine loose(chart, config);
    (void)loose.handle_input(1, InputState::Pressed, 995);
    (void)loose.handle_input(1, InputState::Pressed, 2005);
    CHECK(loose.stats().counts.pg == 2);
    CHECK(loose.stats().accuracy_percent() == doctest::Approx(99.5));

    GameplayChart gr_chart;
    gr_chart.lane_count = 1;
    gr_chart.duration_samples = 2000;
    gr_chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    GameplayEngine gr_engine(gr_chart, config);
    (void)gr_engine.handle_input(1, InputState::Pressed, 1015);
    CHECK(gr_engine.stats().counts.gr == 1);
    CHECK(gr_engine.stats().accuracy_percent() == doctest::Approx(79.75));
}

TEST_CASE("native accuracy counts a long note as one weighted object") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3500;
    chart.notes.push_back(NoteEvent{1, 1000, 1500});
    chart.notes.push_back(NoteEvent{1, 2500, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(1500);
    (void)engine.handle_input(1, InputState::Pressed, 2515);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().accuracy_weight == doctest::Approx(2.0));
    CHECK(engine.stats().accuracy_percent() == doctest::Approx(89.875));
}
TEST_CASE("gameplay engine reports whether an original note is still pending") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    NoteEvent note;
    note.lane = 1;
    note.start_sample = 1000;
    note.end_sample = 1600;
    note.note_id = 42;
    chart.notes.push_back(note);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    GameplayEngine engine(chart, config);
    CHECK(engine.is_note_pending(1, 42));
    CHECK_FALSE(engine.is_note_pending(2, 42));
    CHECK_FALSE(engine.is_note_pending(1, 7));

    (void)engine.handle_input(1, InputState::Pressed, 1000);
    CHECK_FALSE(engine.is_note_pending(1, 42));
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
    CHECK(engine.stats().raw_score == 100'000);
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
    CHECK(engine.stats().raw_score == 100'000);
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

TEST_CASE("sudden death stops on the first missed object even when practice no-fail is set") {
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

TEST_CASE("sudden death uses OD8 miss boundaries instead of native bad timing") {
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
    config.gauge.normal.bd = -100.0;
    config.practice_no_fail_enabled = true;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 100);
    CHECK(engine.stats().counts.pr == 1);
    CHECK_FALSE(engine.is_game_over());

    (void)engine.handle_input(1, InputState::Released, 200);
    (void)engine.handle_input(1, InputState::Pressed, 1100);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().osu_od8.counts.ok == 1);
    CHECK_FALSE(engine.is_game_over());

    GameplayEngine missed_engine(chart, config);
    (void)missed_engine.handle_input(1, InputState::Pressed, 1104);
    CHECK(missed_engine.stats().counts.bd == 1);
    CHECK(missed_engine.stats().osu_od8.counts.miss == 1);
    CHECK(missed_engine.is_game_over());
}

TEST_CASE("sudden death keeps an OD8-valid hold head alive until its tail") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    config.judge.gd_ms = 75.0;
    config.judge.bd_ms = 340.0;
    config.judge.hold_grace_ms = 80.0;
    config.judge.hold_break_ms = 200.0;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1100);

    std::vector<tenriff::gameplay::ActiveHoldView> active_holds;
    engine.collect_active_holds(active_holds);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().osu_od8.judged_objects == 0);
    CHECK(active_holds.size() == 1u);
    CHECK_FALSE(engine.is_game_over());

    engine.advance(2000);
    CHECK(engine.stats().osu_od8.counts.ok == 1);
    CHECK(engine.stats().osu_od8.counts.miss == 0);
    CHECK_FALSE(engine.is_game_over());
}

TEST_CASE("sudden death rejects an OD8-missed hold head immediately") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    config.judge.gd_ms = 75.0;
    config.judge.bd_ms = 340.0;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1104);

    std::vector<tenriff::gameplay::ActiveHoldView> active_holds;
    engine.collect_active_holds(active_holds);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().osu_od8.counts.miss == 1);
    CHECK(active_holds.empty());
    CHECK(engine.is_game_over());
}

TEST_CASE("sudden death catches an OD8 hold-tail miss even when native timing is great") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    config.judge.gd_ms = 75.0;
    config.judge.bd_ms = 340.0;
    config.judge.hold_grace_ms = 80.0;
    config.judge.hold_break_ms = 200.0;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1850);
    engine.advance(2000);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().osu_od8.counts.miss == 1);
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

TEST_CASE("parallel gauge shift keeps Ex-Hard when it survives") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.gauge_shift_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    engine.advance(1500);

    CHECK(engine.is_finished());
    CHECK_FALSE(engine.is_game_over());
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::ExHard);
    CHECK(engine.gauge_state().value == doctest::Approx(100.0));
    CHECK(engine.stats().shifts.empty());
}
TEST_CASE("parallel gauge shift selects the highest independently surviving gauge") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 5000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 2000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 3000, std::nullopt});
    chart.notes.push_back(NoteEvent{1, 4000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.gauge_shift_enabled = true;
    config.gauge.ex_hard.bd = -100.0;
    config.gauge.hard.bd = -60.0;
    config.gauge.normal.bd = -40.0;
    config.gauge.easy.bd = -20.0;

    GameplayEngine engine(chart, config);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::ExHard);
    CHECK(engine.gauge_state().value == doctest::Approx(100.0));

    engine.advance(1041);
    REQUIRE(engine.stats().shifts.size() == 1u);
    CHECK(engine.stats().shifts[0].from == tenriff::game::GaugeType::ExHard);
    CHECK(engine.stats().shifts[0].to == tenriff::game::GaugeType::Hard);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Hard);
    CHECK(engine.gauge_state().value == doctest::Approx(40.0));

    engine.advance(2041);
    REQUIRE(engine.stats().shifts.size() == 2u);
    CHECK(engine.stats().shifts[1].from == tenriff::game::GaugeType::Hard);
    CHECK(engine.stats().shifts[1].to == tenriff::game::GaugeType::Normal);
    CHECK(engine.gauge_state().value == doctest::Approx(20.0));

    engine.advance(3041);
    REQUIRE(engine.stats().shifts.size() == 3u);
    CHECK(engine.stats().shifts[2].from == tenriff::game::GaugeType::Normal);
    CHECK(engine.stats().shifts[2].to == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().value == doctest::Approx(40.0));

    engine.advance(4041);
    CHECK(engine.is_finished());
    CHECK_FALSE(engine.is_game_over());
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().value == doctest::Approx(20.0));
}

TEST_CASE("parallel gauge shift fails only after every gauge has died") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.gauge_shift_enabled = true;
    config.gauge.ex_hard.bd = -100.0;
    config.gauge.hard.bd = -100.0;
    config.gauge.normal.bd = -100.0;
    config.gauge.easy.bd = -100.0;

    GameplayEngine engine(chart, config);
    engine.advance(1041);

    CHECK(engine.is_game_over());
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().value == doctest::Approx(0.0));
}
TEST_CASE("gameplay engine records one legacy threshold-policy shift before Easy game over") {
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

TEST_CASE("judge hard mod leaves charge hold tail windows at their base values") {
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

    CHECK(mode_result.judge.hold_grace_ms == doctest::Approx(judge.hold_grace_ms));
    CHECK(mode_result.judge.hold_break_ms == doctest::Approx(judge.hold_break_ms));
    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.gr == 0);
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
    CHECK(engine.stats().raw_score == 100'000);
}

TEST_CASE("LN mix hold heads respect OD8 sudden-death boundaries during gameplay") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    tenriff::config::ModeConfig mode;
    mode.mods = {"ln_mix_90"};

    const auto mode_result = tenriff::app::manage_modes(
        chart,
        tenriff::app::ChartFormat::Bms,
        mode,
        tenriff::config::JudgeConfig{},
        1.0,
        180.0,
        1000);

    REQUIRE(mode_result.chart.notes.size() == 1u);
    REQUIRE(mode_result.chart.notes[0].end_sample.has_value());
    CHECK_FALSE(mode_result.chart.notes[0].release_required);

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    config.judge.gd_ms = 75.0;
    config.judge.bd_ms = 340.0;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(mode_result.chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1100);

    std::vector<tenriff::gameplay::ActiveHoldView> active_holds;
    engine.collect_active_holds(active_holds);
    CHECK(engine.stats().counts.bd == 1);
    CHECK(engine.stats().osu_od8.judged_objects == 0);
    CHECK(active_holds.size() == 1u);
    CHECK_FALSE(engine.is_game_over());
}

TEST_CASE("judge hard records an unplayed note as an indirect poor") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    tenriff::config::ModeConfig mode;
    mode.mods = {"judge_hard"};
    tenriff::config::JudgeConfig judge;
    judge.pg_ms = 10.0;
    judge.gr_ms = 20.0;
    judge.gd_ms = 30.0;
    judge.bd_ms = 40.0;
    judge.indirect_miss_ms = 40.0;

    const auto mode_result = tenriff::app::manage_modes(
        chart, tenriff::app::ChartFormat::Bms, mode, judge, 1.0);
    REQUIRE(mode_result.judge.indirect_miss_enabled);

    GameplayConfig hard_config;
    hard_config.sample_rate = 1000;
    hard_config.judge = mode_result.judge;
    GameplayEngine hard_engine(chart, hard_config);
    hard_engine.advance(1340);
    CHECK(hard_engine.stats().counts.pr == 0);
    hard_engine.advance(1341);
    CHECK(hard_engine.stats().counts.pr == 1);
    CHECK(hard_engine.stats().counts.bd == 0);
    CHECK(hard_engine.stats().osu_od8.counts.miss == 1);

    GameplayConfig normal_config;
    normal_config.sample_rate = 1000;
    normal_config.judge = judge;
    GameplayEngine normal_engine(chart, normal_config);
    normal_engine.advance(1041);
    CHECK(normal_engine.stats().counts.bd == 1);
    CHECK(normal_engine.stats().counts.pr == 0);
}
