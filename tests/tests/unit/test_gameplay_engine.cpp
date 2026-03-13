#include "doctest/doctest.h"

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

TEST_CASE("gameplay engine treats ghost input as poor") {
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

    CHECK(engine.stats().counts.pr == 1);
    CHECK(engine.stats().combo == 0);
}

TEST_CASE("gameplay engine scores hold tail based on release timing") {
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
    (void)engine.handle_input(1, InputState::Released, 2000);
    engine.advance(2500);

    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.bd == 0);
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
    CHECK(engine.stats().counts.pr == 0);
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
