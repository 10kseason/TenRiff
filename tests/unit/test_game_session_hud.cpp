#include "doctest/doctest.h"

#include <optional>

#include "app/GameSessionInputTiming.h"
#include "app/JudgementLoopTiming.h"
#include "app/GameplayHudRevisions.h"
#include "app/GameplayHudWindow.h"
#include "gameplay/GameplayEngine.h"
#include "gameplay/GameplayChart.h"

namespace {

int64_t note_visible_end_sample(const tenriff::gameplay::NoteEvent& note) {
    return note.end_sample.value_or(note.start_sample);
}

bool note_is_expired_for_hud(const tenriff::gameplay::NoteEvent& note, int64_t current_sample, int64_t past_samples) {
    return note_visible_end_sample(note) < current_sample - past_samples;
}

}  // namespace

TEST_CASE("short note expires once its head leaves the HUD past window") {
    tenriff::gameplay::NoteEvent note{1, 1000, std::nullopt};

    CHECK_FALSE(note_is_expired_for_hud(note, 1170, 180));
    CHECK(note_is_expired_for_hud(note, 1181, 180));
}

TEST_CASE("hold note stays visible until its tail leaves the HUD past window") {
    tenriff::gameplay::NoteEvent note{1, 1000, 2000};

    CHECK_FALSE(note_is_expired_for_hud(note, 2100, 180));
    CHECK_FALSE(note_is_expired_for_hud(note, 2180, 180));
    CHECK(note_is_expired_for_hud(note, 2181, 180));
}

TEST_CASE("display offset expands HUD lookahead so notes do not spawn late") {
    const auto window = tenriff::app::expand_gameplay_hud_window(1000, 180, 2200, 120.0, 24.0);

    CHECK(window.past_samples == 204);
    CHECK(window.lookahead_samples == 2344);
}

TEST_CASE("negative display offset keeps past notes alive longer in the HUD") {
    const auto window = tenriff::app::expand_gameplay_hud_window(1000, 180, 2200, -150.0, 24.0);

    CHECK(window.past_samples == 354);
    CHECK(window.lookahead_samples == 2224);
}

TEST_CASE("gameplay chart sample offsets move notes audio visuals and duration together") {
    tenriff::gameplay::GameplayChart chart;
    chart.duration_samples = 4000;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 1000, std::nullopt});
    chart.notes.push_back(tenriff::gameplay::NoteEvent{2, 2000, 2800});
    chart.audio_cues.push_back(tenriff::gameplay::AudioCueEvent{500, 0});
    chart.visual_cues.push_back(
        tenriff::gameplay::VisualCueEvent{750, 0, tenriff::gameplay::VisualLayer::Base});

    tenriff::gameplay::offset_gameplay_chart_samples(chart, 3000);

    REQUIRE(chart.notes.size() == 2);
    REQUIRE(chart.audio_cues.size() == 1);
    REQUIRE(chart.visual_cues.size() == 1);
    CHECK(chart.duration_samples == 7000);
    CHECK(chart.notes[0].start_sample == 4000);
    CHECK(chart.notes[1].start_sample == 5000);
    REQUIRE(chart.notes[1].end_sample.has_value());
    CHECK(chart.notes[1].end_sample.value() == 5800);
    CHECK(chart.audio_cues[0].start_sample == 3500);
    CHECK(chart.visual_cues[0].start_sample == 3750);
}

TEST_CASE("gameplay hud revisions ignore sample-only updates for text caches") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.current_sample = 1200;
    previous.audio_sample_time_ns = 10'000'000LL;
    previous.audio_buffer_frames = 256;
    previous.note_count = 1;
    previous.notes[0] = {1, 1300, 1300, false, true};

    auto next = previous;
    next.current_sample = 1216;
    next.audio_sample_time_ns = 12'000'000LL;

    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.motion_changed);
    CHECK_FALSE(diff.text_changed);
}

TEST_CASE("gameplay hud revisions bump text caches for score and feedback changes") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.combo = 7;
    previous.max_combo = 19;
    previous.counts.pg = 5;
    previous.gauge = 48.0;
    previous.gauge_type = tenriff::game::GaugeType::Normal;
    previous.rate = 1.0;
    previous.hispeed = 3.5;
    previous.has_feedback = true;
    previous.feedback = tenriff::game::Judgement::GR;

    auto next = previous;
    next.combo = 8;
    next.max_combo = 20;
    next.counts.pg = 6;
    next.osu_od8_score_available = true;
    next.osu_od8_score = 500'000;
    next.gauge = 49.5;
    next.feedback = tenriff::game::Judgement::PG;

    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.text_changed);
}

TEST_CASE("gameplay hud revisions refresh peer text and spectator presentation") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.peer_revision = 10;

    auto next = previous;
    next.peer_revision = 11;
    auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.text_changed);
    CHECK_FALSE(diff.motion_changed);

    next.spectating_peer = true;
    diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.text_changed);
    CHECK(diff.motion_changed);
}

TEST_CASE("gameplay hud revisions treat timing history updates as motion changes") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.timing_history_count = 2;
    previous.timing_history_delta_ms[0] = -6.0;
    previous.timing_history_delta_ms[1] = 3.0;

    auto next = previous;
    next.timing_history_delta_ms[1] = 4.0;

    const auto diff = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(diff.motion_changed);
}

TEST_CASE("judgement loop timing plan clamps to supported polling range") {
    const auto plan = tenriff::app::build_judgement_loop_timing_plan(48000, 12000);

    CHECK(plan.target_hz == 8000);
    CHECK(plan.base_step_samples == 6);
    CHECK(plan.remainder_samples == 0);
}

TEST_CASE("judgement loop timing preserves the requested average hz at 44.1k") {
    const auto plan = tenriff::app::build_judgement_loop_timing_plan(44100, 8000);

    REQUIRE(plan.target_hz == 8000);
    CHECK(plan.base_step_samples == 5);
    CHECK(plan.remainder_samples == 4100);

    int64_t carry = 0;
    int64_t accumulated_samples = 0;
    int five_sample_steps = 0;
    int six_sample_steps = 0;
    for (int i = 0; i < plan.target_hz; ++i) {
        const int64_t step = tenriff::app::next_judgement_loop_step_samples(plan, carry);
        accumulated_samples += step;
        if (step == 5) {
            ++five_sample_steps;
        } else if (step == 6) {
            ++six_sample_steps;
        }
    }

    CHECK(accumulated_samples == 44100);
    CHECK(five_sample_steps == 3900);
    CHECK(six_sample_steps == 4100);
    CHECK(carry == 0);
}

TEST_CASE("startup input anchor maps callback time to playback sample") {
    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 48'000;
    anchor.callback_time_ns = 1'000'000'000LL;
    anchor.valid = true;

    auto mapped =
        tenriff::app::estimate_input_sample_from_startup_anchor(1'000'000'000LL, anchor, 48'000);

    REQUIRE(mapped.has_value());
    CHECK(*mapped == 48'000);
}

TEST_CASE("startup input anchor linearly maps nearby timestamps and clamps before zero") {
    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 48'000;
    anchor.callback_time_ns = 1'000'000'000LL;
    anchor.valid = true;

    auto earlier =
        tenriff::app::estimate_input_sample_from_startup_anchor(750'000'000LL, anchor, 48'000);
    auto later =
        tenriff::app::estimate_input_sample_from_startup_anchor(1'125'000'000LL, anchor, 48'000);

    REQUIRE(earlier.has_value());
    REQUIRE(later.has_value());
    CHECK(*earlier == 36'000);
    CHECK(*later == 54'000);

    tenriff::app::StartupInputTimingAnchor near_zero_anchor;
    near_zero_anchor.playback_sample = 500;
    near_zero_anchor.callback_time_ns = 1'000'000'000LL;
    near_zero_anchor.valid = true;

    auto clamped =
        tenriff::app::estimate_input_sample_from_startup_anchor(0, near_zero_anchor, 48'000);
    REQUIRE(clamped.has_value());
    CHECK(*clamped == 0);
}

TEST_CASE("startup input anchor keeps callback deltas at multi-day QPC values") {
    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 1'000;
    anchor.callback_time_ns = 4LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
    anchor.valid = true;

    const auto mapped = tenriff::app::estimate_input_sample_from_startup_anchor(
        anchor.callback_time_ns + 10'000'000LL, anchor, 44'100);
    REQUIRE(mapped.has_value());
    CHECK(*mapped == 1'441);
}

TEST_CASE("startup gameplay input sample prefers clock sync estimate over startup anchor") {
    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 1'000;
    anchor.callback_time_ns = 1'000'000'000LL;
    anchor.valid = true;

    const int64_t sample = tenriff::app::resolve_startup_gameplay_input_sample(
        1'320, 1'050'000'000LL, anchor, 1'000, 0);

    CHECK(sample == 1'320);
}

TEST_CASE("startup mapping without clock fit still hits the first note") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3'000;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 1'000, std::nullopt});

    tenriff::gameplay::GameplayConfig config;
    config.sample_rate = 1'000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 1'000;
    anchor.callback_time_ns = 1'000'000'000LL;
    anchor.valid = true;

    const int64_t mapped_sample = tenriff::app::resolve_startup_gameplay_input_sample(
        std::nullopt, 1'000'000'000LL, anchor, config.sample_rate, 0);

    tenriff::gameplay::GameplayEngine engine(chart, config);
    auto hit_note = engine.handle_input(1, tenriff::input::InputState::Pressed, mapped_sample);

    REQUIRE(hit_note.has_value());
    CHECK(hit_note->start_sample == 1'000);
    CHECK(engine.stats().counts.pg == 1);
    CHECK(engine.stats().counts.bd == 0);
}

TEST_CASE("startup mapping without clock fit uses playback head instead of future write cursor fallback") {
    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 1'000;
    anchor.callback_time_ns = 1'000'000'000LL;
    anchor.valid = true;

    const int64_t mapped_sample = tenriff::app::resolve_startup_gameplay_input_sample(
        std::nullopt, 1'000'000'000LL, anchor, 1'000, 1'256);

    CHECK(mapped_sample == 1'000);
}

TEST_CASE("startup mapping stays stable across repeated session-style starts") {
    tenriff::gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 3'000;
    chart.notes.push_back(tenriff::gameplay::NoteEvent{1, 1'020, std::nullopt});

    tenriff::gameplay::GameplayConfig config;
    config.sample_rate = 1'000;
    config.rate = 1.0;
    config.judge.pg_ms = 10.0;
    config.judge.gr_ms = 20.0;
    config.judge.gd_ms = 30.0;
    config.judge.bd_ms = 40.0;

    for (int session = 0; session < 2; ++session) {
        tenriff::app::StartupInputTimingAnchor anchor;
        anchor.playback_sample = 1'000;
        anchor.callback_time_ns = 1'000'000'000LL;
        anchor.valid = true;

        const int64_t mapped_sample = tenriff::app::resolve_startup_gameplay_input_sample(
            std::nullopt, 1'020'000'000LL, anchor, config.sample_rate, 0);

        tenriff::gameplay::GameplayEngine engine(chart, config);
        auto hit_note = engine.handle_input(1, tenriff::input::InputState::Pressed, mapped_sample);

        REQUIRE(hit_note.has_value());
        CHECK(hit_note->start_sample == 1'020);
        CHECK(engine.stats().counts.pg == 1);
    }
}

TEST_CASE("gameplay input backlog stale window follows 0.999 bad-window logic") {
    CHECK(tenriff::app::gameplay_input_backlog_stale_window_ms(80.0) == doctest::Approx(96.0));
    CHECK(tenriff::app::gameplay_input_backlog_stale_window_ms(96.0) == doctest::Approx(96.0));
    CHECK(tenriff::app::gameplay_input_backlog_stale_window_ms(140.0) == doctest::Approx(140.0));
}

TEST_CASE("gameplay input backlog uses QPC event age instead of the audio write cursor") {
    constexpr int64_t callback_time_ns = 10'000'000'000LL;

    CHECK_FALSE(tenriff::app::gameplay_input_event_is_stale(
        callback_time_ns - 20'000'000LL, callback_time_ns, 80.0));
    CHECK(tenriff::app::gameplay_input_event_is_stale(
        callback_time_ns - 150'000'000LL, callback_time_ns, 80.0));
    CHECK_FALSE(tenriff::app::gameplay_input_event_is_stale(
        callback_time_ns + 1'000'000LL, callback_time_ns, 80.0));
}

TEST_CASE("fresh gameplay input recovers from a drifted ClockSync estimate") {
    tenriff::app::StartupInputTimingAnchor anchor;
    anchor.playback_sample = 1'000;
    anchor.callback_time_ns = 10'000'000'000LL;
    anchor.valid = true;

    const int64_t input_time_ns = anchor.callback_time_ns + 10'000'000LL;
    const auto anchor_sample =
        tenriff::app::estimate_input_sample_from_startup_anchor(input_time_ns, anchor, 1'000);
    REQUIRE(anchor_sample.has_value());
    REQUIRE(*anchor_sample == 1'010);

    CHECK(tenriff::app::reconcile_fresh_gameplay_input_sample(
              500, anchor_sample, false, 340.0, 1'000) == 1'010);
    CHECK(tenriff::app::reconcile_fresh_gameplay_input_sample(
              900, anchor_sample, false, 340.0, 1'000) == 900);
    CHECK(tenriff::app::reconcile_fresh_gameplay_input_sample(
              500, anchor_sample, true, 340.0, 1'000) == 500);
}
