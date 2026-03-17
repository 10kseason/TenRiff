#include "doctest/doctest.h"

#include "render/GameplayMotion.h"
#include "render/RenderPacing.h"
#include "render/RenderThread.h"

namespace {

using tenriff::render::PerformanceTracker;

TEST_CASE("performance tracker computes frame metrics from recorded frame starts") {
    PerformanceTracker tracker;

    tracker.record_frame_start_ns(0);
    tracker.record_frame_start_ns(300'000'000);
    tracker.record_frame_start_ns(600'000'000);
    tracker.record_frame_start_ns(900'000'000);

    const auto& snapshot = tracker.snapshot();
    CHECK(snapshot.valid);
    CHECK(snapshot.sample_count == 3u);
    CHECK(snapshot.graph_sample_count == 3u);
    CHECK(snapshot.average_frame_ms == doctest::Approx(300.0));
    CHECK(snapshot.average_fps == doctest::Approx(1000.0 / 300.0));
    CHECK(snapshot.max_fps == doctest::Approx(1000.0 / 300.0));
    CHECK(snapshot.fps_0_1_low == doctest::Approx(1000.0 / 300.0));
    CHECK(snapshot.fps_0_01_low == doctest::Approx(1000.0 / 300.0));
    CHECK(snapshot.frame_times_ms[0] == doctest::Approx(300.0f));
    CHECK(snapshot.frame_times_ms[1] == doctest::Approx(300.0f));
    CHECK(snapshot.frame_times_ms[2] == doctest::Approx(300.0f));
}

TEST_CASE("performance tracker reset clears cached snapshot state") {
    PerformanceTracker tracker;

    tracker.record_frame_start_ns(100'000'000);
    tracker.record_frame_start_ns(400'000'000);
    REQUIRE(tracker.snapshot().valid);

    tracker.reset();

    const auto& snapshot = tracker.snapshot();
    CHECK_FALSE(snapshot.valid);
    CHECK(snapshot.sample_count == 0u);
    CHECK(snapshot.graph_sample_count == 0u);
    CHECK(snapshot.graph_revision == 0u);
    CHECK(snapshot.metrics_revision == 0u);
}

TEST_CASE("performance tracker smooths isolated graph spikes without changing raw metrics") {
    PerformanceTracker tracker;

    tracker.record_frame_start_ns(0);
    tracker.record_frame_start_ns(100'000'000);
    tracker.record_frame_start_ns(200'000'000);
    tracker.record_frame_start_ns(700'000'000);
    tracker.record_frame_start_ns(800'000'000);
    tracker.record_frame_start_ns(900'000'000);
    tracker.record_frame_start_ns(1'000'000'000);

    const auto& snapshot = tracker.snapshot();
    CHECK(snapshot.valid);
    CHECK(snapshot.sample_count == 6u);
    CHECK(snapshot.average_frame_ms == doctest::Approx(166.6666666667));
    CHECK(snapshot.frame_times_ms[0] == doctest::Approx(100.0f));
    CHECK(snapshot.frame_times_ms[1] == doctest::Approx(100.0f));
    CHECK(snapshot.frame_times_ms[2] == doctest::Approx(100.0f));
    CHECK(snapshot.frame_times_ms[3] == doctest::Approx(100.0f));
    CHECK(snapshot.frame_times_ms[4] == doctest::Approx(100.0f));
    CHECK(snapshot.frame_times_ms[5] == doctest::Approx(100.0f));
}

TEST_CASE("render pacing advances to the next aligned deadline after overruns") {
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'050) == 1'100);
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'299) == 1'300);
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'300) == 1'400);
}

TEST_CASE("gameplay motion extrapolates from audio sample time and clamps stale HUD drift") {
    tenriff::render::GameplayMotionState state;
    state.current_sample = 1000;
    state.duration_samples = 5000;
    state.sample_rate = 1000;
    state.audio_sample_time_ns = 1'000'000'000LL;
    state.hud_publish_time_ns = 1'008'000'000LL;
    state.audio_buffer_frames = 10;
    state.visual_offset_ms = 5.0;

    const auto diagnostics =
        tenriff::render::compute_gameplay_motion_diagnostics(state, 1'050'000'000LL);

    CHECK(diagnostics.audio_age_ms == doctest::Approx(50.0));
    CHECK(diagnostics.hud_delta_ms == doctest::Approx(8.0));
    CHECK(diagnostics.buffer_ms == doctest::Approx(10.0));
    CHECK(diagnostics.extrapolation_limit_samples == 24);
    CHECK(diagnostics.extrapolated_samples == 24);
    CHECK(diagnostics.extrapolated_ms == doctest::Approx(24.0));
    CHECK(diagnostics.display_sample == 1029);
}

TEST_CASE("gameplay motion stops extrapolating after gameplay finishes") {
    tenriff::render::GameplayMotionState state;
    state.current_sample = 2400;
    state.duration_samples = 3000;
    state.sample_rate = 1000;
    state.audio_sample_time_ns = 1'000'000'000LL;
    state.audio_buffer_frames = 12;
    state.visual_offset_ms = 6.0;
    state.finished = true;

    const auto diagnostics =
        tenriff::render::compute_gameplay_motion_diagnostics(state, 1'080'000'000LL);

    CHECK(diagnostics.extrapolated_samples == 0);
    CHECK(diagnostics.display_sample == 2406);
}

TEST_CASE("gameplay note y mapping eases notes in from slightly above the field") {
    const double judge_line = 0.82;

    CHECK(tenriff::render::compute_gameplay_note_y_normalized(1000, 1000, 2200, 180, judge_line) ==
          doctest::Approx(judge_line));
    CHECK(tenriff::render::compute_gameplay_note_y_normalized(3200, 1000, 2200, 180, judge_line) ==
          doctest::Approx(-0.12));
    CHECK(tenriff::render::compute_gameplay_note_y_normalized(820, 1000, 2200, 180, judge_line) ==
          doctest::Approx(1.0));
}

TEST_CASE("gameplay rendering cuts regular notes once the head passes the judgement line") {
    CHECK(tenriff::render::should_render_gameplay_note(1000, true, 1000));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note(999, true, 1000));
    CHECK(tenriff::render::should_render_gameplay_note(999, false, 1000));
}

TEST_CASE("active hold synthetic notes stay anchored to the judgement line") {
    CHECK_FALSE(tenriff::render::gameplay_note_anchors_to_judgement_line(false, true));
    CHECK_FALSE(tenriff::render::gameplay_note_anchors_to_judgement_line(true, true));
    CHECK(tenriff::render::gameplay_note_anchors_to_judgement_line(true, false));

    CHECK(tenriff::render::gameplay_note_render_sample(1000, false, true, 1024) == 1000);
    CHECK(tenriff::render::gameplay_note_render_sample(1000, true, true, 1024) == 1000);
    CHECK(tenriff::render::gameplay_note_render_sample(1000, true, false, 1024) == 1024);
}

}  // namespace
