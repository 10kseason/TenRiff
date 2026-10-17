#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "app/ModeManager.h"
#include "gameplay/GameplayEngine.h"
#include "render/GameplayTimingIndicator.h"

using tenriff::gameplay::GameplayChart;
using tenriff::gameplay::GameplayConfig;
using tenriff::gameplay::MineEvent;
using tenriff::gameplay::NoteEvent;
using tenriff::gameplay::GameplayEngine;
using tenriff::input::InputState;
using tenriff::game::Judgement;

TEST_CASE("result fast slow aggregate excludes top judgement timing") {
    tenriff::gameplay::ResultStats stats;
    stats.record_note_total(4);
    stats.record_judgement(Judgement::PG, -10.0, tenriff::gameplay::ComboImpact::Increment);
    stats.record_judgement(Judgement::PG, 10.0, tenriff::gameplay::ComboImpact::Increment);
    stats.record_judgement(Judgement::GR, -30.0, tenriff::gameplay::ComboImpact::Increment);
    stats.record_judgement(Judgement::GD, 70.0, tenriff::gameplay::ComboImpact::Increment);

    CHECK(stats.negative_delta_count == 1);
    CHECK(stats.positive_delta_count == 1);
    CHECK(tenriff::gameplay::maximum_detail_score(stats.total_notes) == 20);
}

TEST_CASE("casual score uses a ten-thousand maximum and six-three-one judgement weights") {
    const auto score_for = [](Judgement judgement) {
        tenriff::gameplay::ResultStats stats;
        stats.record_note_total(1);
        stats.record_judgement(judgement, 0.0, tenriff::gameplay::ComboImpact::Increment);
        return stats.raw_score;
    };

    CHECK(tenriff::gameplay::kNativeScoreMaximum == 10'000);
    CHECK(score_for(Judgement::PG) == 10'000);
    CHECK(score_for(Judgement::GR) == 5'000);
    CHECK(score_for(Judgement::GD) == 1'667);
    CHECK(score_for(Judgement::BD) == 0);
    CHECK(score_for(Judgement::PR) == 0);
}

TEST_CASE("gameplay engine accepts a carried course gauge value") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 1000;

    GameplayConfig config;
    config.sample_rate = 1000;
    config.initial_gauge = tenriff::game::GaugeType::Normal;
    config.initial_gauge_value = 37.5;
    config.gauge_policy.course_lr2_deltas = true;

    GameplayEngine engine(chart, config);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Normal);
    CHECK(engine.gauge_state().value == doctest::Approx(37.5));
    CHECK_FALSE(engine.is_game_over());
}

TEST_CASE("BMS LR2 engine divisor counts complete LN once and excludes invalid lanes and mines") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 4000;
    chart.notes = {NoteEvent{1, 1000, std::nullopt}, NoteEvent{2, 2000, 2500},
                   NoteEvent{0, 3000, std::nullopt}, NoteEvent{3, 3000, std::nullopt}};
    chart.mines.push_back(MineEvent{1, 3500, 7.0});
    for (bool enabled : {false, true}) {
        GameplayConfig config;
        config.sample_rate = 1000;
        config.initial_gauge_value = 50.0;
        config.gauge_policy.bms_lr2_deltas = enabled;
        config.gauge_policy.bms_total = 1.0;
        config.gauge_policy.bms_note_count = 9999; // Engine derives the actual valid-head count.
        GameplayEngine engine(chart, config);
        const double per_note = enabled ? 0.5 : 0.19;
        (void)engine.handle_input(1, InputState::Pressed, 1000);
        CHECK(engine.gauge_state().value == doctest::Approx(50.0 + per_note));
        (void)engine.handle_input(2, InputState::Pressed, 2000);
        CHECK(engine.gauge_state().value == doctest::Approx(50.0 + per_note * 1.5));
        engine.advance(2500);
        CHECK(engine.gauge_state().value == doctest::Approx(50.0 + per_note * 2.0));
        CHECK(engine.stats().total_notes == 2);
        CHECK(engine.stats().max_combo == 3);
        CHECK_FALSE(engine.is_game_over());
    }
}

TEST_CASE("BMS LR2 engine retains delta policy while parallel gauge shifts discard refill transitions") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 21000;
    for (int i = 1; i <= 20; ++i) chart.notes.push_back(NoteEvent{1, i * 1000, std::nullopt});
    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 5.0;
    config.judge.gr_ms = 10.0;
    config.judge.gd_ms = 20.0;
    config.judge.bd_ms = 40.0;
    config.gauge_shift_enabled = true;
    config.initial_gauge = tenriff::game::GaugeType::ExHard;
    config.gauge_policy.bms_lr2_deltas = true;
    config.gauge_policy.bms_total = 10.0;
    config.gauge_policy.hard_to_normal_shift = true; // Parallel gauge shift must suppress this.
    config.gauge_policy.normal_to_easy_shift = true;
    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1030);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Hard);
    CHECK(engine.gauge_state().value == doctest::Approx(40.0));
    (void)engine.handle_input(1, InputState::Released, 1100);
    (void)engine.handle_input(1, InputState::Pressed, 2030);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Normal);
    CHECK(engine.gauge_state().value == doctest::Approx(92.0));
    (void)engine.handle_input(1, InputState::Released, 2100);
    (void)engine.handle_input(1, InputState::Pressed, 3000);
    CHECK(engine.gauge_state().value == doctest::Approx(92.5));
    CHECK_FALSE(engine.is_game_over());
}

TEST_CASE("BMS LR2 engine preserves the selected judgement mode consumed-miss classification") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    for (bool indirect : {false, true}) {
        GameplayConfig config;
        config.sample_rate = 1000;
        config.initial_gauge_value = 50.0;
        config.gauge_policy.bms_lr2_deltas = true;
        config.gauge_policy.bms_total = 300.0;
        config.judge.indirect_miss_enabled = indirect;
        GameplayEngine engine(chart, config);
        engine.advance(1500);
        CHECK(engine.gauge_state().value == doctest::Approx(indirect ? 44.0 : 46.0));
        CHECK(engine.stats().counts.bd == (indirect ? 0 : 1));
        CHECK(engine.stats().counts.pr == (indirect ? 1 : 0));
    }
}

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

TEST_CASE("native score is fixed by judgement value and independent of combo order") {
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
    CHECK(engine.stats().raw_score == 6'667);
    CHECK(engine.stats().detail_score == 10);
}

TEST_CASE("native accuracy is categorical while detail accuracy rewards steadier timing") {
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
    CHECK(exact.stats().detailed_accuracy_percent() == doctest::Approx(100.0));

    GameplayEngine loose(chart, config);
    (void)loose.handle_input(1, InputState::Pressed, 995);
    (void)loose.handle_input(1, InputState::Pressed, 2005);
    CHECK(loose.stats().counts.pg == 2);
    CHECK(loose.stats().accuracy_percent() == doctest::Approx(100.0));
    CHECK(loose.stats().detailed_accuracy_percent() < exact.stats().detailed_accuracy_percent());
    CHECK(loose.stats().detailed_accuracy_percent() == doctest::Approx(96.9233).epsilon(0.0001));

    GameplayChart gr_chart;
    gr_chart.lane_count = 1;
    gr_chart.duration_samples = 2000;
    gr_chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});
    GameplayEngine gr_engine(gr_chart, config);
    (void)gr_engine.handle_input(1, InputState::Pressed, 1015);
    CHECK(gr_engine.stats().counts.gr == 1);
    CHECK(gr_engine.stats().accuracy_percent() == doctest::Approx(80.0));
    CHECK(gr_engine.stats().detailed_accuracy_percent() == doctest::Approx(75.4839).epsilon(0.0001));
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
    CHECK(engine.stats().accuracy_percent() == doctest::Approx(90.0));
    CHECK(engine.stats().detail_score == 13);
    CHECK(engine.stats().detailed_accuracy_percent() == doctest::Approx(87.7419).epsilon(0.0001));
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

TEST_CASE("empty POOR damages gauge without changing current accuracy score or combo") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, std::nullopt}, NoteEvent{1, 2000, std::nullopt}};
    GameplayConfig modern;
    modern.sample_rate = 1000;
    auto legacy = modern;
    legacy.empty_poor_affects_accuracy = true;
    GameplayEngine current(chart, modern), historical(chart, legacy), clean(chart, modern);
    const double initial_gauge = current.gauge_state().value;
    for (auto* engine : {&current, &historical}) {
        (void)engine->handle_input(1, InputState::Pressed, 100);
        (void)engine->handle_input(1, InputState::Released, 101);
    }
    CHECK(current.gauge_state().value < initial_gauge);
    CHECK(current.gauge_state().value == historical.gauge_state().value);
    CHECK(current.stats().counts.pr == 1);
    CHECK(current.stats().accuracy_weight == 0.0);
    for (auto* engine : {&current, &historical, &clean}) {
        for (const int sample : {1000, 2000}) {
            (void)engine->handle_input(1, InputState::Pressed, sample);
            (void)engine->handle_input(1, InputState::Released, sample + 1);
        }
        engine->advance(3000);
    }
    CHECK(current.stats().accuracy_percent() == doctest::Approx(100.0));
    CHECK(current.stats().detailed_accuracy_percent() == doctest::Approx(100.0));
    CHECK(historical.stats().accuracy_percent() == doctest::Approx(200.0 / 3.0));
    CHECK(historical.stats().detailed_accuracy_percent() == doctest::Approx(200.0 / 3.0));
    CHECK(current.stats().raw_score == clean.stats().raw_score);
    CHECK(current.stats().raw_score == historical.stats().raw_score);
    CHECK(current.stats().detail_score == historical.stats().detail_score);
    CHECK(current.stats().combo == historical.stats().combo);
    CHECK(current.stats().combo == clean.stats().combo);
    CHECK(current.gauge_state().value == historical.gauge_state().value);
    CHECK(current.stats().analysis.total.event_count == 3);
}

TEST_CASE("missed-note POOR continues to lower current accuracy") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, std::nullopt}, NoteEvent{1, 2000, std::nullopt}};
    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.indirect_miss_enabled = true;
    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1001);
    engine.advance(3000);
    CHECK(engine.stats().counts.pr == 1);
    CHECK(engine.stats().accuracy_percent() == doctest::Approx(50.0));
    CHECK(engine.stats().detailed_accuracy_percent() == doctest::Approx(50.0));
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
    CHECK(engine.stats().raw_score == 10'000);
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

TEST_CASE("gameplay engine never adds empty poor for an input behind a consumed note") {
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
    REQUIRE(engine.handle_input(1, InputState::Pressed, 1000).has_value());
    (void)engine.handle_input(1, InputState::Released, 1010);
    (void)engine.handle_input(1, InputState::Pressed, 1500);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.bd == 0);
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

TEST_CASE("gameplay engine judges charge hold tails with the standard windows") {
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
    (void)engine.handle_input(1, InputState::Released, 1985);
    engine.advance(2500);

    // Tails run through the same windows as taps, so a 15 ms early release is a
    // GREAT rather than being snapped up to PGREAT by a hold-only grace window.
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("gameplay engine breaks charge hold tails released past the good window") {
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

    // 65 ms early is past the GOOD window, so the tail breaks like any other note.
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 0);
    CHECK(engine.stats().counts.bd == 1);
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
    CHECK(engine.stats().raw_score == 10'000);
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
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.
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

TEST_CASE("LN release regression commits an early drop before the tail for live and synced input") {
    for (const bool release_required : {false, true}) {
        for (const bool synced : {false, true}) {
            GameplayChart chart;
            chart.lane_count = 1;
            chart.duration_samples = 3000;
            chart.notes = {NoteEvent{1, 1000, 2000, release_required}};
            GameplayConfig config;
            config.sample_rate = 1000;
            config.judge.pg_ms = 10.0;
            config.judge.gr_ms = 20.0;
            config.judge.gd_ms = 30.0;
            config.judge.bd_ms = 40.0;
            GameplayEngine engine(chart, config);
            (void)engine.handle_input(1, InputState::Pressed, 1000);
            if (synced) engine.sync_input_state(1, InputState::Released, 1500);
            else (void)engine.handle_input(1, InputState::Released, 1500);

            // The key-up itself must publish the miss; waiting for the tail
            // leaves the player with a success feedback for a dropped LN.
            CHECK(engine.stats().counts.bd == 1);
            CHECK(engine.stats().counts.pg == 1);
            CHECK(engine.stats().combo == 0);
            CHECK(engine.gauge_state().value == doctest::Approx(96.875));
            CHECK(engine.live_feedback().judgement == Judgement::BD);
            CHECK(engine.live_feedback().sample == 1500);
            std::vector<tenriff::gameplay::ActiveHoldView> holds;
            engine.collect_active_holds(holds);
            CHECK(holds.empty());
        }
    }
}

TEST_CASE("LN release regression cannot erase a drop by regrabbing in one audio batch") {
    for (const bool release_required : {false, true}) {
        for (const bool synced : {false, true}) {
            for (const bool advance_between_inputs : {false, true}) {
                GameplayChart chart;
                chart.lane_count = 1;
                chart.duration_samples = 3000;
                chart.notes = {NoteEvent{1, 1000, 2000, release_required}};
                GameplayConfig config;
                config.sample_rate = 1000;
                config.judge.pg_ms = 10.0;
                config.judge.gr_ms = 20.0;
                config.judge.gd_ms = 30.0;
                config.judge.bd_ms = 40.0;
                GameplayEngine engine(chart, config);
                (void)engine.handle_input(1, InputState::Pressed, 1000);
                const auto transition = [&](InputState state, int64_t sample) {
                    if (synced) engine.sync_input_state(1, state, sample);
                    else (void)engine.handle_input(1, state, sample);
                };
                transition(InputState::Released, 1500);
                if (advance_between_inputs) engine.advance(1501);
                transition(InputState::Pressed, 1510);
                transition(InputState::Released, 2000);
                engine.advance(2100);
                engine.advance(3000);

                CHECK(engine.stats().counts.pg == 1);
                CHECK(engine.stats().counts.bd == 1);
                CHECK(engine.stats().raw_score == 5000);
                CHECK(engine.stats().combo == 0);
                CHECK(engine.stats().osu_od8.judged_objects == 1);
                CHECK(engine.stats().osu_od8.counts.miss == 1);
                CHECK(engine.gauge_state().value == doctest::Approx(96.875));
            }
        }
    }
}

TEST_CASE("LN release compatibility preserves the deferred ruleset one and two result") {
    for (const bool release_required : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 1;
        chart.duration_samples = 3000;
        chart.notes = {NoteEvent{1, 1000, 2000, release_required}};
        GameplayConfig config;
        config.sample_rate = 1000;
        config.legacy_hold_release = true;
        config.chronological_judgement = false;
        config.judge.pg_ms = 10.0;
        config.judge.gr_ms = 20.0;
        config.judge.gd_ms = 30.0;
        config.judge.bd_ms = 40.0;
        GameplayEngine engine(chart, config);
        (void)engine.handle_input(1, InputState::Pressed, 1000);
        (void)engine.handle_input(1, InputState::Released, 1500);
        CHECK(engine.stats().counts.bd == 0);
        CHECK(engine.stats().combo == 1);
        (void)engine.handle_input(1, InputState::Pressed, 1510);
        (void)engine.handle_input(1, InputState::Released, 2000);
        engine.advance(2100);

        // Historical verification must reproduce the old native score even
        // though new gameplay correctly commits this drop as a BAD.
        CHECK(engine.stats().counts.pg == 2);
        CHECK(engine.stats().counts.bd == 0);
        CHECK(engine.stats().raw_score == 10000);
        CHECK(engine.stats().osu_od8.counts.meh == 1);
        CHECK(engine.gauge_state().value == doctest::Approx(100.0));
    }
}

TEST_CASE("legacy LN release keeps historical late standard hold timing grades") {
    for (const bool release_required : {false, true}) {
        for (const int delta : {-30, -20, -10, 0, 15}) {
            GameplayChart chart;
            chart.lane_count = 1;
            chart.duration_samples = 3000;
            chart.notes = {NoteEvent{1, 1000, 2000, release_required}};
            GameplayConfig config;
            config.sample_rate = 1000;
            config.chronological_judgement = false;
            config.judge.pg_ms = 10.0;
            config.judge.gr_ms = 20.0;
            config.judge.gd_ms = 30.0;
            config.judge.bd_ms = 40.0;
            GameplayEngine engine(chart, config);
            (void)engine.handle_input(1, InputState::Pressed, 1000);
            (void)engine.handle_input(1, InputState::Released, 2000 + delta);
            engine.advance(2500);
            CHECK(engine.stats().counts.pg == (std::abs(delta) <= 10 ? 2 : 1));
            CHECK(engine.stats().counts.gr == (std::abs(delta) > 10 && std::abs(delta) <= 20 ? 1 : 0));
            CHECK(engine.stats().counts.gd == (std::abs(delta) > 20 ? 1 : 0));
            CHECK(engine.stats().counts.bd == 0);
            CHECK(engine.stats().osu_od8.judged_objects == 1);
        }
    }
}

TEST_CASE("gameplay engine still judges standard hold tails after an early release") {
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

    // The hold is not dropped: the tail is still scored, just on the same windows
    // as any other note, so a 60 ms early release lands outside GOOD.
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gr == 0);
    CHECK(engine.stats().counts.bd == 1);
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
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.

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
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.
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

TEST_CASE("sudden death catches an OD8 hold-tail miss even when native timing still scores") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, 2000});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 15.5;
    config.judge.gr_ms = 31.0;
    // Wide enough that the 150 ms early release is still a native GOOD, so the
    // game over below can only come from the OD8 miss.
    config.judge.gd_ms = 200.0;
    config.judge.bd_ms = 340.0;
    config.judge.hold_grace_ms = 80.0;
    config.judge.hold_break_ms = 200.0;
    config.one_miss_fail_enabled = true;

    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1850);
    engine.advance(2000);

    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.gd == 1);
    CHECK(engine.stats().counts.bd == 0);
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
    config.initial_gauge = tenriff::game::GaugeType::ExHard;

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
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.
    config.gauge_shift_enabled = true;
    config.initial_gauge = tenriff::game::GaugeType::ExHard;
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

TEST_CASE("parallel gauge shift begins at the selected tier and only shifts downward") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.
    config.gauge_shift_enabled = true;
    config.initial_gauge = tenriff::game::GaugeType::Normal;
    config.gauge.normal.bd = -100.0;
    config.gauge.easy.bd = -50.0;

    GameplayEngine engine(chart, config);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Normal);

    engine.advance(1041);

    REQUIRE(engine.stats().shifts.size() == 1u);
    CHECK(engine.stats().shifts[0].from == tenriff::game::GaugeType::Normal);
    CHECK(engine.stats().shifts[0].to == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().type == tenriff::game::GaugeType::Easy);
    CHECK(engine.gauge_state().value == doctest::Approx(50.0));
    CHECK_FALSE(engine.is_game_over());
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
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.
    config.gauge_shift_enabled = true;
    config.initial_gauge = tenriff::game::GaugeType::ExHard;
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
    config.judge.indirect_miss_ms = 40.0; // This fixture tests a custom short miss deadline.
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

TEST_CASE("judge easy mod expands charge hold tail judgement during gameplay") {
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
    (void)engine.handle_input(1, InputState::Released, 2012);
    engine.advance(2500);

    // Tails ride the normal windows, so Judge Easy's 1.35x PGREAT (13.5 ms) is what
    // turns this 12 ms late release into a PGREAT instead of a GREAT.
    CHECK(engine.stats().counts.pg == 2);
    CHECK(engine.stats().counts.gr == 0);
}

TEST_CASE("judge hard mod tightens charge hold tail windows without changing hold tolerances") {
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
    (void)engine.handle_input(1, InputState::Released, 2012);
    engine.advance(2500);

    CHECK(mode_result.judge.pg_ms == doctest::Approx(judge.pg_ms * 17.5 / 21.0));
    CHECK(mode_result.judge.gr_ms == doctest::Approx(judge.gr_ms * 18.0 / 21.0));
    CHECK(mode_result.judge.hold_grace_ms == doctest::Approx(judge.hold_grace_ms));
    CHECK(mode_result.judge.hold_break_ms == doctest::Approx(judge.hold_break_ms));
    // The 12 ms release lies between Hard's 8.33 ms PG and 17.14 ms GR.
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
    CHECK(engine.stats().raw_score == 10'000);
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

TEST_CASE("current modes defer automatic misses until after 340 real milliseconds") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 1000});
    for (const auto token : {"", "judge_easy", "judge_hard"}) {
        tenriff::config::ModeConfig mode;
        if (*token) mode.mods = {token};
        const auto managed = tenriff::app::manage_modes(
            chart, tenriff::app::ChartFormat::Bms, mode, tenriff::config::JudgeConfig{}, 1.0);
        GameplayConfig config;
        config.sample_rate = 1000;
        config.judge = managed.judge;
        GameplayEngine engine(chart, config);
        engine.advance(1340);
        CHECK(engine.stats().counts.bd == 0);
        CHECK(engine.stats().counts.pr == 0);
        engine.advance(1341);
        CHECK(engine.stats().counts.bd == (mode.mods == std::vector<std::string>{"judge_hard"} ? 0 : 1));
        CHECK(engine.stats().counts.pr == (mode.mods == std::vector<std::string>{"judge_hard"} ? 1 : 0));
    }
}

TEST_CASE("hard BAD boundary and the gap before automatic miss do not swallow the next exact note") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000}};
    tenriff::config::ModeConfig mode;
    mode.mods = {"judge_hard"};
    const auto managed = tenriff::app::manage_modes(
        chart, tenriff::app::ChartFormat::Bms, mode, tenriff::config::JudgeConfig{}, 1.0);
    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge = managed.judge;
    GameplayEngine boundary(chart, config);
    CHECK(boundary.handle_input(1, InputState::Pressed, 1225).has_value());
    CHECK(boundary.stats().counts.bd == 1);
    GameplayEngine outside(chart, config);
    CHECK_FALSE(outside.handle_input(1, InputState::Pressed, 1226).has_value());
    CHECK(outside.stats().counts.bd == 0);
    CHECK(outside.stats().counts.pr == 1);

    chart.notes.push_back(NoteEvent{1, 1250});
    GameplayEngine dense(chart, config);
    dense.advance(1249);
    CHECK(dense.stats().counts.pr == 0);
    REQUIRE(dense.handle_input(1, InputState::Pressed, 1250).has_value());
    CHECK(dense.stats().counts.pg == 1);
    CHECK(dense.stats().counts.pr == 1);
    dense.advance(1600);
    CHECK(dense.stats().counts.pr == 1); // The skipped note must not miss again.
}

TEST_CASE("easy allows a 26ms PGREAT while normal remains GREAT") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000}};
    for (bool easy : {false, true}) {
        tenriff::config::ModeConfig mode;
        if (easy) mode.mods = {"judge_easy"};
        GameplayConfig config;
        config.sample_rate = 1000;
        config.judge = tenriff::app::manage_modes(
            chart, tenriff::app::ChartFormat::Bms, mode, tenriff::config::JudgeConfig{}, 1.0).judge;
        GameplayEngine engine(chart, config);
        REQUIRE(engine.handle_input(1, InputState::Pressed, 1026).has_value());
        CHECK(engine.stats().counts.pg == (easy ? 1 : 0));
        CHECK(engine.stats().counts.gr == (easy ? 0 : 1));
    }
}

TEST_CASE("gameplay landmines damage held lanes without affecting score or judgement counts") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 2500, std::nullopt});
    chart.mines.push_back(MineEvent{1, 1000, 5.0, false, 7, 42});

    GameplayConfig config;
    config.sample_rate = 1000;
    GameplayEngine engine(chart, config);

    (void)engine.handle_input(1, InputState::Pressed, 500);
    engine.advance(1000);

    CHECK(engine.gauge_state().value == doctest::Approx(95.0));
    CHECK(engine.stats().counts.pg + engine.stats().counts.gr + engine.stats().counts.gd +
              engine.stats().counts.bd + engine.stats().counts.pr == 0);
    CHECK(engine.stats().raw_score == 0);
    CHECK_FALSE(engine.is_mine_pending(1, 42));

    std::vector<tenriff::gameplay::MineTrigger> triggers;
    engine.drain_mine_triggers(triggers);
    REQUIRE(triggers.size() == 1u);
    CHECK(triggers[0].mine_id == 42u);
    CHECK(triggers[0].audio_asset_id == 7u);
}

TEST_CASE("gameplay landmine boundary detonates on press and is avoided on release") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 2500, std::nullopt});
    chart.mines.push_back(MineEvent{1, 1000, 5.0, false, 7, 1});

    GameplayConfig config;
    config.sample_rate = 1000;

    GameplayEngine pressed(chart, config);
    (void)pressed.handle_input(1, InputState::Pressed, 1000);
    CHECK(pressed.gauge_state().value == doctest::Approx(95.0));

    GameplayEngine released(chart, config);
    (void)released.handle_input(1, InputState::Pressed, 500);
    (void)released.handle_input(1, InputState::Released, 1000);
    CHECK(released.gauge_state().value == doctest::Approx(100.0));

    std::vector<tenriff::gameplay::MineTrigger> triggers;
    released.drain_mine_triggers(triggers);
    CHECK(triggers.empty());
}

TEST_CASE("ZZ gameplay landmine forces game over only when triggered") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes.push_back(NoteEvent{1, 2500, std::nullopt});
    chart.mines.push_back(MineEvent{1, 1000, 647.5, true, 9, 2});

    GameplayConfig config;
    config.sample_rate = 1000;

    GameplayEngine safe(chart, config);
    safe.advance(1000);
    CHECK_FALSE(safe.is_game_over());
    CHECK(safe.gauge_state().value == doctest::Approx(100.0));

    GameplayEngine triggered(chart, config);
    (void)triggered.handle_input(1, InputState::Pressed, 1000);
    CHECK(triggered.is_game_over());
    CHECK(triggered.gauge_state().value == doctest::Approx(0.0));
}

TEST_CASE("gameplay replay samples stay monotonic when realtime mapping regresses") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 5000;
    chart.notes.push_back(NoteEvent{1, 4000, std::nullopt});
    chart.notes.push_back(NoteEvent{2, 4500, std::nullopt});

    GameplayConfig config;
    config.sample_rate = 1000;
    GameplayEngine engine(chart, config);

    engine.sync_input_state(1, InputState::Pressed, 1000);
    (void)engine.handle_input(2, InputState::Pressed, 900);
    engine.sync_input_state(1, InputState::Released, 800);

    const auto& events = engine.replay().events;
    REQUIRE(events.size() == 3u);
    CHECK(events[0].sample == 1000);
    CHECK(events[1].sample == 1000);
    CHECK(events[2].sample == 1000);
}

namespace {
GameplayConfig lr2_course_test_config(double initial = 50.0) {
    GameplayConfig config;
    config.sample_rate = 1000;
    config.initial_gauge_value = initial;
    config.gauge_policy.course_lr2_deltas = true;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 40.0;
    config.judge.bd_ms = 80.0;
    config.judge.indirect_miss_ms = 210.0;
    config.judge.indirect_miss_enabled = false; // Course must retain missed-note POOR.
    return config;
}
}

TEST_CASE("LR2 course gauge preserves missed and empty poor as distinct damage events") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes = {NoteEvent{1, 1000}, NoteEvent{1, 2000}, NoteEvent{1, 3000}};
    auto config = lr2_course_test_config();
    config.gauge_shift_enabled = true; // Course policy always disables parallel tiers.
    GameplayEngine engine(chart, config);
    REQUIRE(engine.handle_input(1, InputState::Pressed, 1000).has_value());
    (void)engine.handle_input(1, InputState::Released, 1001);
    CHECK(engine.gauge_state().value == doctest::Approx(50.1));
    (void)engine.handle_input(1, InputState::Pressed, 1500);
    CHECK(engine.gauge_state().value == doctest::Approx(48.1));
    CHECK(engine.stats().combo == 1);
    engine.advance(2211);
    CHECK(engine.gauge_state().value == doctest::Approx(45.1));
    CHECK(engine.stats().counts.pr == 2);
    CHECK(engine.stats().counts.bd == 0);
    CHECK(engine.stats().combo == 0);
    CHECK(engine.stats().shifts.empty());
}

TEST_CASE("LR2 course gauge carries exact HP between stages and keeps failed carry dead") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 2000;
    chart.notes = {NoteEvent{1, 1000}};
    auto first_config = lr2_course_test_config(31.9);
    GameplayEngine first(chart, first_config);
    first.advance(1211);
    REQUIRE_FALSE(first.is_game_over());
    CHECK(first.gauge_state().value == doctest::Approx(30.1));
    auto second_config = lr2_course_test_config(first.gauge_state().value);
    GameplayEngine second(chart, second_config);
    CHECK(second.gauge_state().value == first.gauge_state().value);
    (void)second.handle_input(1, InputState::Pressed, 1000);
    CHECK(second.gauge_state().value == doctest::Approx(30.2));
    GameplayEngine minimum(chart, lr2_course_test_config(2.0));
    CHECK_FALSE(minimum.is_game_over());
    GameplayEngine dead(chart, lr2_course_test_config(1.99));
    CHECK(dead.is_game_over());
    CHECK_FALSE(dead.handle_input(1, InputState::Pressed, 1000).has_value());
    CHECK(dead.gauge_state().value == doctest::Approx(1.99));
}

TEST_CASE("LR2 course normal LN gauge waits for completion and uses the head judgement once") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, 2000}};
    for (int delta : {0, 15, 30, 60}) {
        GameplayEngine engine(chart, lr2_course_test_config());
        REQUIRE(engine.handle_input(1, InputState::Pressed, 1000 + delta).has_value());
        CHECK(engine.gauge_state().value == doctest::Approx(50.0));
        engine.advance(2000);
        const double outcome = delta <= 20 ? 0.1 : (delta <= 40 ? 0.04 : -2.0);
        CHECK(engine.gauge_state().value == doctest::Approx(50.0 + outcome));
        engine.advance(2500);
        CHECK(engine.gauge_state().value == doctest::Approx(50.0 + outcome));
        CHECK(engine.stats().total_notes == 1);
        CHECK(engine.stats().total_combo_steps == 2); // Native score still counts head/tail.
    }
}

TEST_CASE("LR2 course LN early release damages once and regrab cannot refund it") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, 2000}};
    GameplayEngine engine(chart, lr2_course_test_config(31.9));
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Released, 1500);
    CHECK(engine.gauge_state().value == doctest::Approx(30.7));
    (void)engine.handle_input(1, InputState::Pressed, 1510);
    engine.advance(2000);
    CHECK(engine.gauge_state().value == doctest::Approx(30.7));
    GameplayEngine fatal(chart, lr2_course_test_config(3.19));
    (void)fatal.handle_input(1, InputState::Pressed, 1000);
    (void)fatal.handle_input(1, InputState::Released, 1500);
    CHECK(fatal.is_game_over());
    CHECK(fatal.gauge_state().value == doctest::Approx(1.99));
}

TEST_CASE("LR2 course LN release at the GOOD grace boundary retains the head judgement") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, 2000}};
    GameplayEngine engine(chart, lr2_course_test_config());
    (void)engine.handle_input(1, InputState::Pressed, 1015);
    (void)engine.handle_input(1, InputState::Released, 1960);
    CHECK(engine.gauge_state().value == doctest::Approx(50.1));
    engine.advance(2000);
    CHECK(engine.gauge_state().value == doctest::Approx(50.1));
}

TEST_CASE("LR2 course missed LN consumes one full poor and never earns a tail recovery") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, 2000}};
    GameplayEngine engine(chart, lr2_course_test_config());
    engine.advance(1211);
    CHECK(engine.gauge_state().value == doctest::Approx(47.0));
    CHECK(engine.stats().counts.pr == 1);
    engine.advance(2500);
    CHECK(engine.gauge_state().value == doctest::Approx(47.0));
}

TEST_CASE("LR2 course charge notes retain TenRiff split head and release gauge weights") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3000;
    chart.notes = {NoteEvent{1, 1000, 2000, true}};
    GameplayEngine engine(chart, lr2_course_test_config());
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    CHECK(engine.gauge_state().value == doctest::Approx(50.05));
    (void)engine.handle_input(1, InputState::Released, 2030);
    engine.advance(2030);
    CHECK(engine.gauge_state().value == doctest::Approx(50.07));
}

TEST_CASE("initial released-key synchronization does not invalidate a native replay") {
    GameplayChart chart;
    chart.lane_count = 10;
    chart.duration_samples = 5000;
    chart.notes.push_back(NoteEvent{1, 1000});
    GameplayConfig config;
    config.sample_rate = 48000;
    GameplayEngine engine(chart, config);
    for (int lane = 1; lane <= 10; ++lane) engine.sync_input_state(lane, InputState::Released, 0);
    CHECK(engine.replay().events.empty());
    engine.sync_input_state(2, InputState::Pressed, 10);
    engine.sync_input_state(2, InputState::Pressed, 20);
    engine.sync_input_state(2, InputState::Released, 30);
    engine.sync_input_state(2, InputState::Released, 40);
    REQUIRE(engine.replay().events.size() == 2u);
    CHECK(engine.replay().events[0].state == InputState::Pressed);
    CHECK(engine.replay().events[1].state == InputState::Released);
    CHECK(engine.stats().raw_score == 0);
    tenriff::gameplay::ReplayFile replay;
    replay.trace = engine.replay();
    replay.sample_rate = config.sample_rate;
    replay.chart_sha256 = std::string(64, 'a');
    replay.ruleset_id = "tenriff-native-score-v2-ruleset-1";
    replay.created_utc = "20260913_000000Z";
    replay.chart_format = "bms";
    CHECK(tenriff::gameplay::validate_replay_evidence(replay).success());
}

TEST_CASE("timing guidance refreshes on following PG and expires after 750 milliseconds") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 5000;
    chart.notes = {{1, 1000, std::nullopt}, {2, 1100, std::nullopt}, {1, 3000, std::nullopt}};
    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 30.0;
    config.judge.gd_ms = 70.0;
    config.judge.bd_ms = 100.0;
    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 980);
    REQUIRE(engine.live_feedback().judgement == Judgement::GR);
    (void)engine.handle_input(2, InputState::Pressed, 1100);
    CHECK(engine.live_feedback().judgement == Judgement::PG);
    CHECK(engine.stats().counts.gr == 1);
    CHECK(engine.stats().counts.pg == 1);
    const auto timing = engine.timing_feedback();
    CHECK(timing.judgement == Judgement::PG);
    CHECK(timing.delta_ms == doctest::Approx(0));
    CHECK(timing.sample == 1100);
    CHECK(tenriff::gameplay::timing_feedback_visible(timing, 1850, 1000));
    CHECK_FALSE(tenriff::gameplay::timing_feedback_visible(timing, 1851, 1000));
    (void)engine.handle_input(1, InputState::Released, 2800);
    (void)engine.handle_input(1, InputState::Pressed, 3025);
    CHECK(engine.timing_feedback().delta_ms == doctest::Approx(25));
    CHECK(engine.timing_feedback().sample == 3025);
}

TEST_CASE("a missed BAD shows the non PG bar without inventing a timing error") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 5000;
    chart.notes = {{1, 1000, std::nullopt}, {1, 3000, std::nullopt}};
    GameplayConfig config;
    config.sample_rate = 1000;
    config.judge.indirect_miss_ms = 0.0;
    config.judge.bd_ms = 100.0;
    GameplayEngine engine(chart, config);
    engine.advance(1101);
    CHECK(engine.non_pg_feedback().has_value);
    CHECK(engine.non_pg_feedback().judgement == Judgement::BD);
    CHECK_FALSE(engine.timing_feedback().has_value);
    CHECK(tenriff::gameplay::timing_feedback_visible(engine.non_pg_feedback(), 1101, 1000));
}


TEST_CASE("timing audit standard LN completion is independent of advance before late release") {
    int mismatches = 0;
    for (const int late_samples : {1, 30, 70, 140}) {
        GameplayChart chart;
        chart.lane_count = 2;
        chart.duration_samples = 10000;
        chart.notes = {NoteEvent{1, 1000, 2000, false}, NoteEvent{2, 9000}};
        GameplayConfig config;
        config.sample_rate = 1000;
        config.practice_no_fail_enabled = true;
        GameplayEngine batched(chart, config);
        GameplayEngine stepped(chart, config);

        (void)batched.handle_input(1, InputState::Pressed, 1000);
        (void)batched.handle_input(1, InputState::Released, 2000 + late_samples);
        batched.advance(2000 + late_samples);
        (void)stepped.handle_input(1, InputState::Pressed, 1000);
        stepped.advance(2000);
        (void)stepped.handle_input(1, InputState::Released, 2000 + late_samples);
        stepped.advance(2000 + late_samples);

        // A standard LN held beyond its tail has already earned automatic PG.
        // Only the placement of an advance call differs between these traces.
        const bool matches = batched.replay().events.size() == 2u &&
            stepped.replay().events.size() == 2u &&
            batched.replay().events[1].sample == stepped.replay().events[1].sample &&
            batched.stats().counts.pg == 2 && batched.stats().counts.gr == 0 &&
            batched.stats().counts.gd == 0 && batched.stats().counts.bd == 0 &&
            batched.stats().raw_score == stepped.stats().raw_score &&
            batched.stats().detailed_accuracy_percent() ==
                doctest::Approx(stepped.stats().detailed_accuracy_percent());
        std::cout << "[timing-audit-ln] late=" << late_samples
                  << " batch_pg=" << batched.stats().counts.pg
                  << " batch_gr=" << batched.stats().counts.gr
                  << " batch_gd=" << batched.stats().counts.gd
                  << " batch_bd=" << batched.stats().counts.bd
                  << " batch_score=" << batched.stats().raw_score
                  << " step_score=" << stepped.stats().raw_score
                  << " batch_detail=" << batched.stats().detailed_accuracy_percent()
                  << " step_detail=" << stepped.stats().detailed_accuracy_percent()
                  << " matches=" << matches << '\n';
        mismatches += matches ? 0 : 1;
    }
    // This repository's CHECK aborts its test case, so assert after every row.
    CHECK(mismatches == 0);
}

TEST_CASE("timing audit early empty poor is independent of automatic miss batching") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 10000;
    chart.notes = {NoteEvent{1, 1000}, NoteEvent{1, 2000}, NoteEvent{2, 9000}};
    GameplayConfig config;
    config.sample_rate = 1000;
    config.practice_no_fail_enabled = true;
    GameplayEngine batched(chart, config);
    GameplayEngine stepped(chart, config);

    (void)batched.handle_input(1, InputState::Pressed, 1400);
    batched.advance(1400);
    stepped.advance(1341);
    (void)stepped.handle_input(1, InputState::Pressed, 1400);
    stepped.advance(1400);

    // The same press falls behind the first missed note and ahead of the next.
    // Its additional empty-POOR damage must not depend on callback partitioning.
    const bool matches = batched.replay().events.size() == 1u &&
        stepped.replay().events.size() == 1u &&
        batched.replay().events[0].sample == stepped.replay().events[0].sample &&
        batched.stats().counts.bd == stepped.stats().counts.bd &&
        batched.stats().counts.pr == stepped.stats().counts.pr &&
        batched.gauge_state().value == doctest::Approx(stepped.gauge_state().value);
    std::cout << "[timing-audit-poor] batch_bd=" << batched.stats().counts.bd
              << " step_bd=" << stepped.stats().counts.bd
              << " batch_pr=" << batched.stats().counts.pr
              << " step_pr=" << stepped.stats().counts.pr
              << " batch_gauge=" << batched.gauge_state().value
              << " step_gauge=" << stepped.gauge_state().value
              << " matches=" << matches << '\n';
    CHECK(matches);
}

TEST_CASE("timing audit automatic hold and miss outcomes follow time across advance partitions") {
    int mismatches = 0;
    for (const bool same_lane : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 2;
        chart.duration_samples = 10000;
        chart.notes = {NoteEvent{1, 1000, 2000, false},
                       NoteEvent{same_lane ? 1 : 2, same_lane ? 2100 : 1658},
                       NoteEvent{2, 9000}};
        GameplayConfig config;
        config.sample_rate = 1000;
        config.practice_no_fail_enabled = true;
        GameplayEngine batched(chart, config);
        GameplayEngine stepped(chart, config);
        const int64_t final_sample = same_lane ? 2441 : 2001;
        const int64_t first_boundary = same_lane ? 2000 : 1999;

        (void)batched.handle_input(1, InputState::Pressed, 1000);
        batched.advance(final_sample);
        (void)stepped.handle_input(1, InputState::Pressed, 1000);
        stepped.advance(first_boundary);
        stepped.advance(final_sample);

        // A lane loop (or miss-before-hold loop) must not reverse timed scoring
        // events. Even when counts match, gauge capping and combo do not commute.
        const bool matches = batched.stats().counts.pg == stepped.stats().counts.pg &&
            batched.stats().counts.bd == stepped.stats().counts.bd &&
            batched.stats().raw_score == stepped.stats().raw_score &&
            batched.gauge_state().value == doctest::Approx(stepped.gauge_state().value) &&
            batched.stats().combo == stepped.stats().combo &&
            batched.stats().max_combo == stepped.stats().max_combo;
        std::cout << "[timing-audit-auto] same_lane=" << same_lane
                  << " batch_gauge=" << batched.gauge_state().value
                  << " step_gauge=" << stepped.gauge_state().value
                  << " batch_combo=" << batched.stats().combo
                  << " step_combo=" << stepped.stats().combo
                  << " batch_max=" << batched.stats().max_combo
                  << " step_max=" << stepped.stats().max_combo
                  << " matches=" << matches << '\n';
        mismatches += matches ? 0 : 1;
    }
    CHECK(mismatches == 0);
}


TEST_CASE("chronological automatic events preserve failure at a cross-lane low-gauge deadline") {
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 10000;
    chart.notes = {NoteEvent{1, 1000, 2000}, NoteEvent{2, 1658}, NoteEvent{2, 9000}};
    GameplayConfig config;
    config.sample_rate = 1000;
    config.initial_gauge_value = 6.1;
    GameplayEngine batched(chart, config);
    GameplayEngine stepped(chart, config);
    (void)batched.handle_input(1, InputState::Pressed, 1000);
    (void)stepped.handle_input(1, InputState::Pressed, 1000);
    batched.advance(2001);
    stepped.advance(1999);
    stepped.advance(2001);

    // The miss becomes due at 1999. A tail due at 2000 cannot rescue a dead gauge.
    CHECK(batched.is_game_over());
    CHECK(stepped.is_game_over());
    CHECK(batched.gauge_state().value == doctest::Approx(0.0));
    CHECK(batched.stats().counts.pg == 1);
    CHECK(batched.stats().counts.bd == 1);
    CHECK(batched.stats().raw_score == stepped.stats().raw_score);
    CHECK(batched.stats().max_combo == stepped.stats().max_combo);
}

TEST_CASE("chronological mine miss CN timeout and LN completion share one event order") {
    GameplayChart chart;
    chart.lane_count = 4;
    chart.duration_samples = 10000;
    chart.notes = {NoteEvent{1, 1000, 2000}, NoteEvent{2, 1000, 1700, true},
                   NoteEvent{3, 1400}, NoteEvent{4, 9000}};
    chart.mines = {MineEvent{4, 1800, 5.0, false, 7, 42}};
    GameplayConfig config;
    config.sample_rate = 1000;
    GameplayEngine batched(chart, config);
    GameplayEngine stepped(chart, config);
    for (GameplayEngine* engine : {&batched, &stepped}) {
        (void)engine->handle_input(4, InputState::Pressed, 500);
        (void)engine->handle_input(1, InputState::Pressed, 1000);
        (void)engine->handle_input(2, InputState::Pressed, 1000);
    }
    batched.advance(2500);
    for (int64_t sample : {1741, 1800, 1901, 2000, 2500}) stepped.advance(sample);

    CHECK(batched.stats().counts.pg == 3);
    CHECK(batched.stats().counts.bd == 2);
    CHECK(batched.gauge_state().value == doctest::Approx(85.72));
    CHECK(batched.gauge_state().value == doctest::Approx(stepped.gauge_state().value));
    CHECK(batched.stats().combo == stepped.stats().combo);
    CHECK(batched.stats().max_combo == stepped.stats().max_combo);
    CHECK(batched.stats().raw_score == stepped.stats().raw_score);
    std::vector<tenriff::gameplay::MineTrigger> batch_mines;
    std::vector<tenriff::gameplay::MineTrigger> step_mines;
    batched.drain_mine_triggers(batch_mines);
    stepped.drain_mine_triggers(step_mines);
    REQUIRE(batch_mines.size() == 1u);
    REQUIRE(step_mines.size() == 1u);
    CHECK(batch_mines[0].sample == 1800);
    CHECK(batch_mines[0].mine_id == step_mines[0].mine_id);
}

TEST_CASE("chronological same-sample input group precedes automatic tail and mine events") {
    for (bool synced_release : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 3;
        chart.duration_samples = 10000;
        chart.notes = {NoteEvent{1, 1000, 2000}, NoteEvent{2, 1000, 2000, true},
                       NoteEvent{3, 9000}};
        chart.mines = {MineEvent{1, 2000, 5.0, false, 7, 42}};
        GameplayConfig config;
        config.sample_rate = 1000;
        GameplayEngine engine(chart, config);
        (void)engine.handle_input(1, InputState::Pressed, 1000);
        (void)engine.handle_input(2, InputState::Pressed, 1000);
        engine.advance(1999);
        if (synced_release) engine.sync_input_state(1, InputState::Released, 2000);
        else (void)engine.handle_input(1, InputState::Released, 2000);
        (void)engine.handle_input(2, InputState::Released, 2000);
        engine.advance(2000);
        engine.advance(2500);
        CHECK(engine.stats().counts.pg == 4);
        CHECK(engine.stats().counts.bd == 0);
        CHECK(engine.gauge_state().value == doctest::Approx(100.0));
        std::vector<tenriff::gameplay::MineTrigger> mines;
        engine.drain_mine_triggers(mines);
        CHECK(mines.empty());
    }
}

TEST_CASE("chronological input and catch-up sync complete older LN tails before a late release") {
    for (bool synced_release : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 2;
        chart.duration_samples = 10000;
        chart.notes = {NoteEvent{1, 1000, 2000}, NoteEvent{2, 9000}};
        GameplayConfig config;
        config.sample_rate = 1000;
        GameplayEngine direct(chart, config);
        GameplayEngine stepped(chart, config);
        for (GameplayEngine* engine : {&direct, &stepped}) {
            (void)engine->handle_input(1, InputState::Pressed, 1000);
        }
        stepped.advance(2029);
        for (GameplayEngine* engine : {&direct, &stepped}) {
            if (synced_release) engine->sync_input_state(1, InputState::Released, 2030);
            else (void)engine->handle_input(1, InputState::Released, 2030);
            engine->advance(2030);
        }
        CHECK(direct.stats().counts.pg == 2);
        CHECK(direct.stats().raw_score == stepped.stats().raw_score);
        CHECK(direct.stats().detailed_accuracy_percent() == doctest::Approx(100.0));
        REQUIRE(direct.replay().events.size() == 2u);
        CHECK(direct.replay().events[1].sample == 2030);
    }
}

TEST_CASE("chronological equal automatic deadlines retain lane and mine-before-hold order") {
    for (bool same_lane : {false, true}) {
        GameplayChart chart;
        chart.lane_count = 3;
        chart.duration_samples = 10000;
        chart.notes = {NoteEvent{same_lane ? 1 : 2, 1000, 2000}, NoteEvent{3, 9000}};
        chart.mines = {MineEvent{1, 2000, 5.0, false, 7, 42}};
        GameplayConfig config;
        config.sample_rate = 1000;
        config.initial_gauge_value = 4.9;
        GameplayEngine batched(chart, config);
        GameplayEngine stepped(chart, config);
        for (GameplayEngine* engine : {&batched, &stepped}) {
            if (!same_lane) engine->sync_input_state(1, InputState::Pressed, 500);
            (void)engine->handle_input(same_lane ? 1 : 2, InputState::Pressed, 1000);
        }
        batched.advance(2001);
        stepped.advance(1999);
        stepped.advance(2000);
        stepped.advance(2001);
        // At an equal sample, lane 1 comes first; in the same lane the mine
        // comes before the hold. Its lethal damage cannot be rescued by a tail.
        CHECK(batched.is_game_over());
        CHECK(stepped.is_game_over());
        CHECK(batched.stats().counts.pg == 1);
        CHECK(batched.stats().raw_score == stepped.stats().raw_score);
        CHECK(batched.gauge_state().value == doctest::Approx(0.0));
    }
}

TEST_CASE("historical engine ordering remains selectable for rulesets one through four") {
    GameplayConfig config;
    config.sample_rate = 1000;
    config.chronological_judgement = false;
    GameplayChart chart;
    chart.lane_count = 2;
    chart.duration_samples = 10000;
    chart.notes = {NoteEvent{1, 1000, 2000}, NoteEvent{2, 9000}};
    GameplayEngine late(chart, config);
    (void)late.handle_input(1, InputState::Pressed, 1000);
    (void)late.handle_input(1, InputState::Released, 2030);
    late.advance(2030);
    CHECK(late.stats().counts.pg == 1);
    CHECK(late.stats().counts.gr == 1);
    CHECK(late.stats().raw_score == 3750);

    chart.notes = {NoteEvent{1, 1000}, NoteEvent{1, 2000}, NoteEvent{2, 9000}};
    GameplayEngine poor(chart, config);
    (void)poor.handle_input(1, InputState::Pressed, 1400);
    poor.advance(1400);
    CHECK(poor.stats().counts.bd == 1);
    CHECK(poor.stats().counts.pr == 0);

    chart.notes = {NoteEvent{1, 1000, 2000}, NoteEvent{2, 1658}, NoteEvent{2, 9000}};
    config.initial_gauge_value = 6.1;
    GameplayEngine lane_order(chart, config);
    (void)lane_order.handle_input(1, InputState::Pressed, 1000);
    lane_order.advance(2001);
    CHECK_FALSE(lane_order.is_game_over());
    CHECK(lane_order.gauge_state().value == doctest::Approx(0.04));
    CHECK(lane_order.stats().counts.pg == 2);
}


TEST_CASE("chronological mixed timelines preserve results and replay prefixes under callback partitions") {
    struct TraceInput { int lane; InputState state; int64_t sample; };
    int mismatches = 0;
    int comparisons = 0;
    for (uint32_t seed = 1; seed <= 32; ++seed) {
        uint32_t random_state = seed;
        const auto next_random = [&]() {
            random_state = random_state * 1664525u + 1013904223u;
            return random_state;
        };
        GameplayChart chart;
        chart.lane_count = 4;
        chart.duration_samples = 101000;
        std::vector<TraceInput> inputs;
        constexpr int head_deltas[] = {-100, -30, -1, 0, 1, 30, 100, 250};
        constexpr int tail_deltas[] = {-150, -30, -1, 0, 1, 30, 150, 250};
        for (int block = 0; block < 8; ++block) {
            for (int lane = 1; lane <= 4; ++lane) {
                const int64_t start = 1000 + block * 1000 + lane * 7 + next_random() % 30;
                const unsigned kind = (next_random() >> 16) % 3;
                const int64_t end = start + 120 + next_random() % 300;
                NoteEvent note{lane, start};
                if (kind != 0) note.end_sample = end;
                note.release_required = kind == 2;
                chart.notes.push_back(note);
                // Some heads are deliberately omitted. The remaining input
                // trace alternates physical press/release per lane, with enough
                // space between blocks that even late CN releases do not overlap.
                if ((next_random() >> 16) % 5 != 0) {
                    const int64_t press = start + head_deltas[(next_random() >> 16) % 8];
                    const int64_t release = kind == 0 ? press + 1 :
                        std::max(press + 1, end + tail_deltas[(next_random() >> 16) % 8]);
                    inputs.push_back({lane, InputState::Pressed, press});
                    inputs.push_back({lane, InputState::Released, release});
                }
                if ((next_random() >> 16) % 3 == 0) {
                    chart.mines.push_back(MineEvent{lane, start + 50 + next_random() % 350,
                                                    5.0, false});
                }
            }
        }
        chart.notes.push_back(NoteEvent{4, 100000});
        std::stable_sort(inputs.begin(), inputs.end(), [](const TraceInput& a, const TraceInput& b) {
            if (a.sample != b.sample) return a.sample < b.sample;
            if (a.state != b.state) return a.state == InputState::Released;
            return a.lane < b.lane;
        });
        GameplayConfig config;
        config.sample_rate = 1000;
        constexpr double initial_gauges[] = {100.0, 45.0, 15.0, 6.1};
        config.initial_gauge_value = initial_gauges[seed % 4];
        // Mix live fail behavior and no-fail traces, so long automatic-event
        // sequences as well as stopping exactly at a lethal event are covered.
        config.practice_no_fail_enabled = seed % 3 == 0;
        GameplayEngine reference(chart, config);
        std::vector<std::size_t> expected_prefix_sizes;
        for (std::size_t index = 0; index < inputs.size();) {
            const int64_t sample = inputs[index].sample;
            do {
                const auto& input = inputs[index++];
                (void)reference.handle_input(input.lane, input.state, input.sample);
            } while (index < inputs.size() && inputs[index].sample == sample);
            expected_prefix_sizes.push_back(reference.replay().events.size());
        }
        reference.advance(100500);

        for (int partitions : {0, 1, 3, 7}) {
            GameplayEngine candidate(chart, config);
            int64_t previous_sample = 0;
            std::size_t group = 0;
            bool prefixes_match = true;
            const auto advance_gap = [&](int64_t from, int64_t through) {
                for (int part = 1; part <= partitions; ++part) {
                    candidate.advance(from + (through - from) * part / (partitions + 1));
                }
            };
            for (std::size_t index = 0; index < inputs.size();) {
                const int64_t sample = inputs[index].sample;
                advance_gap(previous_sample, sample - 1);
                do {
                    const auto& input = inputs[index++];
                    (void)candidate.handle_input(input.lane, input.state, input.sample);
                } while (index < inputs.size() && inputs[index].sample == sample);
                const auto& actual = candidate.replay().events;
                const auto& expected = reference.replay().events;
                prefixes_match = prefixes_match && actual.size() == expected_prefix_sizes[group++];
                if (actual.size() > expected.size()) prefixes_match = false;
                else for (std::size_t i = 0; i < actual.size(); ++i) {
                    prefixes_match = prefixes_match && actual[i].lane == expected[i].lane &&
                        actual[i].state == expected[i].state && actual[i].sample == expected[i].sample;
                }
                previous_sample = sample;
            }
            advance_gap(previous_sample, 100500);
            candidate.advance(100500);
            const auto& a = candidate.stats();
            const auto& b = reference.stats();
            const bool matches = prefixes_match && a.counts.pg == b.counts.pg &&
                a.counts.gr == b.counts.gr && a.counts.gd == b.counts.gd &&
                a.counts.bd == b.counts.bd && a.counts.pr == b.counts.pr &&
                a.raw_score == b.raw_score && a.combo == b.combo && a.max_combo == b.max_combo &&
                a.detailed_accuracy_percent() == doctest::Approx(b.detailed_accuracy_percent()) &&
                candidate.gauge_state().type == reference.gauge_state().type &&
                candidate.gauge_state().value == doctest::Approx(reference.gauge_state().value) &&
                candidate.is_game_over() == reference.is_game_over() &&
                candidate.is_finished() == reference.is_finished();
            ++comparisons;
            if (!matches) {
                ++mismatches;
                std::cout << "[timing-r5-property-mismatch] seed=" << seed
                          << " partitions=" << partitions << " prefixes=" << prefixes_match
                          << " gauge=" << candidate.gauge_state().value
                          << " reference_gauge=" << reference.gauge_state().value << '\n';
            }
        }
    }
    std::cout << "[timing-r5-property] fixtures=32 comparisons=" << comparisons
              << " mismatches=" << mismatches << '\n';
    CHECK(comparisons == 128);
    CHECK(mismatches == 0);
}


TEST_CASE("indicator history expires each judgement exactly at seven hundred milliseconds") {
    GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 10000;
    chart.notes = {{1,1000,std::nullopt},{1,1100,std::nullopt},{1,1200,std::nullopt}};
    GameplayConfig config; config.sample_rate = 1000;
    GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, InputState::Pressed, 1000);
    (void)engine.handle_input(1, InputState::Pressed, 1110);
    (void)engine.handle_input(1, InputState::Pressed, 1180);
    const auto score = engine.stats().raw_score;
    const auto replay_count = engine.replay().events.size();
    std::array<double, tenriff::kGameplayTimingHistoryMaxEntries> values{};
    std::size_t count = 0;
    engine.collect_recent_timing_deltas(values, &count, 999, 1699);
    CHECK(count == 3);
    engine.collect_recent_timing_deltas(values, &count, 1000, 1700);
    REQUIRE(count == 2); CHECK(values[0] == doctest::Approx(10)); CHECK(values[1] == doctest::Approx(-20));
    engine.collect_recent_timing_deltas(values, &count, 1110, 1810);
    REQUIRE(count == 1); CHECK(values[0] == doctest::Approx(-20));
    engine.collect_recent_timing_deltas(values, &count, 1180, 1880);
    CHECK(count == 0); CHECK(values[0] == 0.0);
    engine.collect_recent_timing_deltas(values, &count, 0, 1099);
    REQUIRE(count == 1); CHECK(values[0] == 0.0);
    CHECK(engine.stats().raw_score == score);
    CHECK(engine.replay().events.size() == replay_count);
}

TEST_CASE("indicator maximum range matches inclusive sample quantized judgement boundaries") {
    using namespace tenriff::render;
    for (const int rate : {44100, 48000}) {
        for (const double pg_ms : {6.0, 8.75, 10.5, 14.0, 17.5, 21.0, 26.25}) {
            for (const double song_rate : {0.5, 1.0, 2.0}) {
                for (const int side : {-1, 1}) {
                    for (const int step : {-1, 0, 1}) {
                        GameplayConfig config; config.sample_rate = rate; config.rate = song_rate;
                        config.judge.pg_ms = pg_ms;
                        GameplayChart chart; chart.lane_count = 1; chart.duration_samples = rate * 4;
                        chart.notes = {{1,rate,std::nullopt}};
                        GameplayEngine engine(chart, config);
                        const auto boundary = static_cast<int64_t>(std::llround(pg_ms * rate / 1000.0));
                        const auto delta = side * (boundary + step);
                        (void)engine.handle_input(1, InputState::Pressed, rate + delta);
                        std::array<double, tenriff::kGameplayTimingHistoryMaxEntries> values{};
                        std::array<bool, tenriff::kGameplayTimingHistoryMaxEntries> maximums{};
                        std::size_t count = 0;
                        engine.collect_recent_timing_deltas(values, &count, 0, rate * 2, &maximums);
                        REQUIRE(count == 1);
                        CHECK(maximums[0] == (step <= 0));
                        CHECK(engine.maximum_judgement_window_ms() == doctest::Approx(boundary * 1000.0 / rate));
                        const auto expected = step <= 0 ? kTimingMaximumColor : side < 0 ? kTimingFastColor : kTimingLateColor;
                        CHECK(timing_marker_color(values[0], engine.maximum_judgement_window_ms(), maximums[0]) == expected);
                        engine.collect_recent_timing_deltas(values, &count, rate + delta, rate * 3, &maximums);
                        CHECK(count == 0); CHECK_FALSE(maximums[0]);
                    }
                }
            }
        }
    }
    CHECK(timing_marker_color(-65.0, 21.0, true) == kTimingMaximumColor);
    CHECK(timing_marker_color(65.0, 21.0, true) == kTimingMaximumColor);
    CHECK(timing_maximum_fraction(20.0) == doctest::Approx(0.25));
    CHECK(timing_maximum_fraction(100.0) == 1.0f);
    CHECK(timing_maximum_fraction(-1.0) == 0.0f);
}

TEST_CASE("consecutive maximum hits refresh FAST SLOW rather than retaining an older error") {
    GameplayChart chart; chart.lane_count = 1; chart.duration_samples = 6000;
    chart.notes = {{1,1000,std::nullopt},{1,2000,std::nullopt},{1,3000,std::nullopt},{1,4000,std::nullopt}};
    GameplayConfig config; config.sample_rate = 1000; config.judge.pg_ms = 10;
    GameplayEngine engine(chart, config);
    for (const auto sample : {980,2005,2997,4000}) {
        (void)engine.handle_input(1, InputState::Pressed, sample);
        const auto& timing=engine.timing_feedback();
        CHECK(timing.sample == sample);
        CHECK(timing.delta_ms == doctest::Approx(sample == 980 ? -20 : sample == 2005 ? 5 : sample == 2997 ? -3 : 0));
        CHECK(timing.judgement == (sample == 980 ? Judgement::GR : Judgement::PG));
        (void)engine.handle_input(1, InputState::Released, sample+1);
    }
}
