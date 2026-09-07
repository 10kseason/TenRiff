#include "render/GameplayFeedbackText.h"
#include "doctest/doctest.h"

#include "render/BgaVideoDecoder.h"
#include "render/GameplayGaugePalette.h"
#include "render/GameplayGearLayout.h"
#include "render/GameplayMotion.h"
#include "render/GameplayNativeDigitalKey.h"
#include "render/GameplayBackgroundPolicy.h"
#include "render/RenderPacing.h"
#include "render/RenderThread.h"
#include "render/OnnxBackgroundUpscaler.h"

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

TEST_CASE("render wait policy shrinks the busy tail for high off-vsync fps") {
    const auto baseline = tenriff::render::render_wait_policy(false, 300);
    CHECK(baseline.coarse_sleep_min_ns == tenriff::render::kRenderDefaultCoarseSleepMinNs);
    CHECK(baseline.spin_guard_ns == tenriff::render::kRenderDefaultSpinGuardNs);
    CHECK(baseline.yield_threshold_ns == tenriff::render::kRenderDefaultYieldThresholdNs);

    const auto high_fps = tenriff::render::render_wait_policy(false, 1050);
    CHECK(high_fps.coarse_sleep_min_ns < baseline.coarse_sleep_min_ns);
    CHECK(high_fps.spin_guard_ns < baseline.spin_guard_ns);
    CHECK(high_fps.yield_threshold_ns < baseline.yield_threshold_ns);
    CHECK(high_fps.yield_threshold_ns <= high_fps.spin_guard_ns);

    const auto vsync_policy = tenriff::render::render_wait_policy(true, 1050);
    CHECK(vsync_policy.coarse_sleep_min_ns == baseline.coarse_sleep_min_ns);
    CHECK(vsync_policy.spin_guard_ns == baseline.spin_guard_ns);
    CHECK(vsync_policy.yield_threshold_ns == baseline.yield_threshold_ns);
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

    for (const double endpoint : {0.0, 1.0}) {
        const double just_before = tenriff::render::compute_gameplay_note_y_normalized(
            1001, 1000, 2200, 180, endpoint);
        const double on_time = tenriff::render::compute_gameplay_note_y_normalized(
            1000, 1000, 2200, 180, endpoint);
        const double just_after = tenriff::render::compute_gameplay_note_y_normalized(
            999, 1000, 2200, 180, endpoint);

        CHECK(on_time == doctest::Approx(endpoint));
        CHECK(just_before <= on_time);
        CHECK(just_after >= on_time);
        CHECK(on_time - just_before < 0.02);
        CHECK(just_after - on_time < 0.02);
    }
}

TEST_CASE("gameplay rendering keeps a hold body continuous across the active-hold handoff") {
    constexpr int64_t handoff_grace_samples = 32;
    CHECK(tenriff::render::gameplay_hold_handoff_grace_samples(1000, 24) == handoff_grace_samples);
    CHECK(tenriff::render::gameplay_hold_handoff_grace_samples(1000, 1000) == 64);

    CHECK(tenriff::render::should_render_gameplay_note(1000, 1000, false, true, false, 1000, 1000,
                                                       handoff_grace_samples));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note(999, 999, false, true, false, 999, 1000,
                                                             handoff_grace_samples));

    CHECK(tenriff::render::should_render_gameplay_note(1000, 1600, true, true, false, 999, 1032,
                                                       handoff_grace_samples));
    CHECK(tenriff::render::should_render_gameplay_note(1000, 1600, true, false, false, 1032, 1032,
                                                       handoff_grace_samples));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note(1000, 1600, true, true, false, 999, 1033,
                                                             handoff_grace_samples));
    CHECK(tenriff::render::should_render_gameplay_note(1000, 1600, true, true, true, 1001, 1001,
                                                       handoff_grace_samples));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note(1000, 1600, true, true, false, 1001, 1001,
                                                             handoff_grace_samples));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note(999, 999, true, true, true, 999, 1000,
                                                             handoff_grace_samples));
    CHECK(tenriff::render::should_render_gameplay_note(999, 1600, true, false, false, 1600, 1600,
                                                       handoff_grace_samples));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note(999, 1600, true, false, false, 1601, 1601,
                                                             handoff_grace_samples));

    CHECK(tenriff::render::should_render_gameplay_note_head(1000, true, 1000));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note_head(999, true, 1000));
    CHECK_FALSE(tenriff::render::should_render_gameplay_note_head(1000, false, 1000));
}

TEST_CASE("active hold synthetic notes stay anchored to the judgement line") {
    CHECK_FALSE(tenriff::render::gameplay_note_anchors_to_judgement_line(false, true));
    CHECK_FALSE(tenriff::render::gameplay_note_anchors_to_judgement_line(true, true));
    CHECK(tenriff::render::gameplay_note_anchors_to_judgement_line(true, false));

    CHECK(tenriff::render::gameplay_note_render_sample(1000, false, true, 1024) == 1000);
    CHECK(tenriff::render::gameplay_note_render_sample(1000, true, true, 1024) == 1000);
    CHECK(tenriff::render::gameplay_note_render_sample(1000, true, false, 1024) == 1024);
    constexpr int64_t display_sample = 1024;
    const int64_t render_sample =
        tenriff::render::gameplay_note_render_sample(1000, true, false, display_sample);

    for (const double judge_line : {0.0, 0.5, 1.0}) {
        const double y = tenriff::render::compute_gameplay_note_y_normalized(
            render_sample,
            display_sample,
            2200,
            180,
            judge_line);
        CHECK(y == doctest::Approx(judge_line));
    }
}

TEST_CASE("long-note body touches the rendered head and stops at the tail edge") {
    const auto body = tenriff::render::compute_gameplay_hold_body_geometry(
        100.0f,
        70.0f,
        130.0f,
        900.0f,
        920.0f,
        310.0f,
        300.0f,
        true,
        true,
        1.0);

    CHECK(body.left == doctest::Approx(70.0f));
    CHECK(body.right == doctest::Approx(130.0f));
    CHECK(body.top == doctest::Approx(310.0f));
    CHECK(body.bottom == doctest::Approx(900.0f));

    const auto active = tenriff::render::compute_gameplay_hold_body_geometry(
        100.0f, 70.0f, 130.0f, 900.0f, 920.0f, 310.0f, 300.0f, false, false, 1.0);
    CHECK(active.top == doctest::Approx(300.0f));
    CHECK(active.bottom == doctest::Approx(920.0f));
}

TEST_CASE("skin preview long-note placement never reverses at extreme judgement lines") {
    for (const float judgement_y : {100.0f, 510.0f, 900.0f}) {
        const auto placement = tenriff::render::compute_gameplay_preview_hold_placement(
            100.0f, 900.0f, judgement_y, 22.0f, 18.0f);
        CHECK(placement.head_center_y - 22.0f > placement.tail_center_y + 18.0f);
        CHECK(placement.tail_center_y - 18.0f >= 102.0f);
        CHECK(placement.head_center_y + 22.0f <= 898.0f);
    }
}

}  // namespace
TEST_CASE("external ONNX background policy only targets low-resolution images in ONNX mode") {
    using tenriff::render::OnnxBackgroundUpscaler;

    CHECK(OnnxBackgroundUpscaler::should_upscale(640, 480, "onnx"));
    CHECK(OnnxBackgroundUpscaler::should_upscale(1280, 720, "onnx"));
    CHECK_FALSE(OnnxBackgroundUpscaler::should_upscale(1920, 1080, "onnx"));
    CHECK_FALSE(OnnxBackgroundUpscaler::should_upscale(1280, 720, "off"));

    // Asynchronous video results must not overwrite newer native frames.
    CHECK_FALSE(OnnxBackgroundUpscaler::should_upscale_realtime_video("onnx"));
    CHECK_FALSE(OnnxBackgroundUpscaler::should_upscale_realtime_video("off"));
}

TEST_CASE("gameplay BGA policy supports off and on transitions") {
    const auto enabled = tenriff::render::resolve_gameplay_background_policy(
        true, "base.mp4", "overlay.png", 120, 240, "onnx");
    CHECK(enabled.base_path == "base.mp4");
    CHECK(enabled.overlay_path == "overlay.png");
    CHECK(enabled.base_start_sample == 120);
    CHECK(enabled.overlay_start_sample == 240);
    CHECK(enabled.upscale_mode == "onnx");

    const auto disabled = tenriff::render::resolve_gameplay_background_policy(
        false, "base.mp4", "overlay.png", 120, 240, "onnx");
    CHECK(disabled.base_path.empty());
    CHECK(disabled.overlay_path.empty());
    CHECK(disabled.base_start_sample == 0);
    CHECK(disabled.overlay_start_sample == 0);
    CHECK(disabled.upscale_mode == "off");

    const auto reenabled = tenriff::render::resolve_gameplay_background_policy(
        true, "base.mp4", "overlay.png", 120, 240, "onnx");
    CHECK(reenabled.base_path == enabled.base_path);
    CHECK(reenabled.overlay_path == enabled.overlay_path);
    CHECK(reenabled.upscale_mode == enabled.upscale_mode);
}
TEST_CASE("procedural circle and polygon skins use the full 100 percent bar width") {
    using tenriff::render::gameplay_note_shape_extents;

    const auto bar = gameplay_note_shape_extents(72.0f, 24.0f, "rect");
    CHECK(bar.half_width == doctest::Approx(36.0f));
    CHECK(bar.half_height == doctest::Approx(12.0f));

    for (const char* shape : {"circle", "triangle", "pentagon", "hexagon"}) {
        const auto extents = gameplay_note_shape_extents(72.0f, 24.0f, shape);
        CHECK(extents.half_width == doctest::Approx(36.0f));
        CHECK(extents.half_height == doctest::Approx(36.0f));
    }
}
TEST_CASE("note width scale resizes the playfield and notes together") {
    using tenriff::render::compute_gameplay_note_draw_width;
    using tenriff::render::compute_gameplay_playfield_width;

    CHECK(compute_gameplay_playfield_width(980.0f, 0.50) == doctest::Approx(490.0f));
    CHECK(compute_gameplay_playfield_width(980.0f, 1.00) == doctest::Approx(980.0f));
    CHECK(compute_gameplay_playfield_width(980.0f, 1.40) == doctest::Approx(1372.0f));

    CHECK(compute_gameplay_note_draw_width(49.0f, 0.50) == doctest::Approx(37.0f));
    CHECK(compute_gameplay_note_draw_width(98.0f, 1.00) == doctest::Approx(74.0f));
    CHECK(compute_gameplay_note_draw_width(137.2f, 1.40) == doctest::Approx(103.6f));

    // Imported LR2 art may still narrow inside the linked field without changing field scale.
    CHECK(compute_gameplay_note_draw_width(98.0f, 1.00, 0.50) == doctest::Approx(37.0f));
}
TEST_CASE("gameplay progress track stays outside the note fields") {
    using tenriff::render::compute_gameplay_progress_track_layout;

    const auto single = compute_gameplay_progress_track_layout(
        84.0f, 1836.0f, 470.0f, 1534.0f, false, 0.0f, 0.0f, 16.0f);
    CHECK(single.left == doctest::Approx(84.0f));
    CHECK(single.right == doctest::Approx(454.0f));

    const auto shifted_single = compute_gameplay_progress_track_layout(
        84.0f, 1836.0f, 12.0f, 1076.0f, false, 0.0f, 0.0f, 16.0f);
    CHECK(shifted_single.left == doctest::Approx(1092.0f));
    CHECK(shifted_single.right == doctest::Approx(1836.0f));

    const auto ghost = compute_gameplay_progress_track_layout(
        84.0f, 1836.0f, 250.0f, 894.0f, true, 1110.0f, 1670.0f, 16.0f);
    CHECK(ghost.left == doctest::Approx(910.0f));
    CHECK(ghost.right == doctest::Approx(1094.0f));

    const auto wide_ghost = compute_gameplay_progress_track_layout(
        84.0f, 1836.0f, 138.0f, 1006.0f, true, 998.0f, 1782.0f, 16.0f);
    CHECK(wide_ghost.left == doctest::Approx(84.0f));
    CHECK(wide_ghost.right == doctest::Approx(122.0f));
}
TEST_CASE("gameplay text pop animation settles deterministically") {
    using tenriff::render::compute_gameplay_text_pop_animation;

    const auto initial = compute_gameplay_text_pop_animation(0.0, 200.0, 1.20f, -8.0f);
    CHECK(initial.scale == doctest::Approx(1.20f));
    CHECK(initial.offset_y == doctest::Approx(-8.0f));
    CHECK(initial.opacity == doctest::Approx(0.88f));

    const auto midpoint = compute_gameplay_text_pop_animation(100.0, 200.0, 1.20f, -8.0f);
    CHECK(midpoint.scale == doctest::Approx(1.05f));
    CHECK(midpoint.offset_y == doctest::Approx(-2.0f));
    CHECK(midpoint.opacity == doctest::Approx(0.97f));

    const auto settled = compute_gameplay_text_pop_animation(300.0, 200.0, 1.20f, -8.0f);
    CHECK(settled.scale == doctest::Approx(1.0f));
    CHECK(settled.offset_y == doctest::Approx(0.0f));
    CHECK(settled.opacity == doctest::Approx(1.0f));
}
TEST_CASE("Media Foundation BGA video extension policy accepts MPG and common containers") {
    using tenriff::render::BgaVideoDecoder;

    CHECK(BgaVideoDecoder::is_supported_video_path("movie.mpg"));
    CHECK(BgaVideoDecoder::is_supported_video_path("MOVIE.MPEG"));
    CHECK(BgaVideoDecoder::is_supported_video_path("clip.mp4"));
    CHECK(BgaVideoDecoder::is_supported_video_path("clip.webm"));
    CHECK(BgaVideoDecoder::is_supported_video_path("clip.mkv"));
    CHECK(BgaVideoDecoder::is_supported_video_path("clip.mov"));
    CHECK_FALSE(BgaVideoDecoder::is_supported_video_path("still.png"));
}

TEST_CASE("LR2 Gear fits as one bottom-anchored panel without distortion") {
    using tenriff::render::GameplayGearRect;
    using tenriff::render::fit_gameplay_gear_rect;

    const auto fitted = fit_gameplay_gear_rect(
        GameplayGearRect{0.0f, 0.0f, 1412.0f, 205.0f}, 506.0f, 142.0f);
    CHECK(fitted.left == doctest::Approx(340.7535f).epsilon(0.0001));
    CHECK(fitted.top == doctest::Approx(0.0f));
    CHECK(fitted.right == doctest::Approx(1071.2465f).epsilon(0.0001));
    CHECK(fitted.bottom == doctest::Approx(205.0f));
    CHECK((fitted.right - fitted.left) / (fitted.bottom - fitted.top) ==
          doctest::Approx(506.0f / 142.0f));

    const auto width_limited = fit_gameplay_gear_rect(
        GameplayGearRect{0.0f, 0.0f, 1000.0f, 100.0f}, 1000.0f, 50.0f);
    CHECK(width_limited.left == doctest::Approx(0.0f));
    CHECK(width_limited.top == doctest::Approx(50.0f));
    CHECK(width_limited.right == doctest::Approx(1000.0f));
    CHECK(width_limited.bottom == doctest::Approx(100.0f));
}

TEST_CASE("LR2 Gear can overscale below the judgement line without widening distortion") {
    using tenriff::render::GameplayGearRect;
    using tenriff::render::fit_gameplay_gear_rect;
    using tenriff::render::gameplay_gear_scale_multiplier;

    CHECK(gameplay_gear_scale_multiplier(0.5) == doctest::Approx(1.25f));
    CHECK(gameplay_gear_scale_multiplier(1.0) == doctest::Approx(2.0f));
    CHECK(gameplay_gear_scale_multiplier(1.4) == doctest::Approx(2.8f));

    const auto enlarged = fit_gameplay_gear_rect(
        GameplayGearRect{0.0f, 0.0f, 1000.0f, 205.0f}, 80.0f, 480.0f, 2.0f);
    CHECK(enlarged.left == doctest::Approx(465.8333f).epsilon(0.0001));
    CHECK(enlarged.top == doctest::Approx(-205.0f));
    CHECK(enlarged.right == doctest::Approx(534.1667f).epsilon(0.0001));
    CHECK(enlarged.bottom == doctest::Approx(205.0f));
    CHECK((enlarged.right - enlarged.left) / (enlarged.bottom - enlarged.top) ==
          doctest::Approx(80.0f / 480.0f));
}

TEST_CASE("imported pressed art is a transient hit pulse instead of an LN hold state") {
    CHECK_FALSE(tenriff::render::should_use_imported_pressed_key(0.0f));
    CHECK_FALSE(tenriff::render::should_use_imported_pressed_key(0.05f));
    CHECK(tenriff::render::should_use_imported_pressed_key(0.051f));
    CHECK(tenriff::render::should_use_imported_pressed_key(1.5f));
}

TEST_CASE("EX-HARD gauge uses its own near-black gray palette") {
    CHECK(tenriff::render::gameplay_gauge_color("EX-HARD") == 0x292C31u);
    CHECK(tenriff::render::gameplay_gauge_color("HARD") == 0xFF4D6Du);
    CHECK(tenriff::render::gameplay_gauge_color("EASY") == 0x89D185u);
    CHECK(tenriff::render::gameplay_gauge_color("NORMAL") == 0xFFB703u);
    CHECK(tenriff::render::gameplay_gauge_color("EX-HARD") !=
          tenriff::render::gameplay_gauge_color("HARD"));
    CHECK(tenriff::render::song_select_gauge_text_color("ex_hard") == 0xFF4D6Du);
    CHECK(tenriff::render::song_select_gauge_text_color("hard") == 0xFF9F43u);
    CHECK(tenriff::render::song_select_gauge_text_color("normal") == 0xFFE45Eu);
    CHECK(tenriff::render::song_select_gauge_text_color("easy") == 0x5EE59Au);
}

TEST_CASE("native digital keys separate held depth from hit glitch") {
    const auto idle = tenriff::render::resolve_native_digital_key_visual(false, 0.0f, 80.0f);
    CHECK(idle.press_offset == doctest::Approx(0.0f));
    CHECK(idle.glitch_strength == doctest::Approx(0.0f));

    const auto held = tenriff::render::resolve_native_digital_key_visual(true, 0.25f, 80.0f);
    CHECK(held.press_offset == doctest::Approx(6.0f));
    CHECK(held.glitch_strength == doctest::Approx(0.0625f));

    const auto released_hit = tenriff::render::resolve_native_digital_key_visual(false, 1.5f, 24.0f);
    CHECK(released_hit.press_offset == doctest::Approx(0.0f));
    CHECK(released_hit.glitch_strength == doctest::Approx(1.0f));
}

TEST_CASE("render thread performance metrics use explicit present completions") {
    tenriff::render::RenderThread render_thread;

    render_thread.record_presented_frame_ns(0);
    CHECK_FALSE(render_thread.performance_snapshot().valid);

    render_thread.record_presented_frame_ns(100'000'000);
    render_thread.record_presented_frame_ns(104'000'000);

    const auto snapshot = render_thread.performance_snapshot();
    CHECK(snapshot.valid);
    CHECK(snapshot.sample_count == 1u);
    CHECK(snapshot.average_frame_ms == doctest::Approx(4.0));
    CHECK(snapshot.average_fps == doctest::Approx(250.0));
}

TEST_CASE("render pacing treats zero fps as unlimited only without vsync") {
    CHECK(tenriff::render::should_use_unlimited_render_pacing(false, 0));
    CHECK_FALSE(tenriff::render::should_use_unlimited_render_pacing(true, 0));
    CHECK_FALSE(tenriff::render::should_use_unlimited_render_pacing(false, 300));
}

TEST_CASE("key beam decay follows elapsed time at both 60 and 144 frames per second") {
    using tenriff::render::gameplay_interpolated_activity;
    constexpr int64_t start = 1000000000;
    for (int fps : {60, 144}) {
        float previous = 1.0f;
        for (int frame = 0; frame <= fps; ++frame) {
            const int64_t now = start + static_cast<int64_t>(frame * 1000000000.0 / fps);
            const float activity = gameplay_interpolated_activity(1.0f, start, now);
            CHECK(activity <= previous);
            CHECK(activity >= 0.0f);
            previous = activity;
        }
    }
    CHECK(gameplay_interpolated_activity(1.0f, start, start + 100000000) == doctest::Approx(0.5));
    CHECK(gameplay_interpolated_activity(1.0f, start, start + 200000000) == 0.0f);
}

TEST_CASE("only P GREAT animates and GOOD uses solid gray") {
    using namespace tenriff::render;
    for (const auto* judgement : {"GR", "G", "BAD", "POOR"}) {
        for (double age : {0.0, 50.0, 150.0, 300.0}) {
            const auto motion = gameplay_judgement_animation(judgement, age);
            CHECK(motion.scale == 1.0f);
            CHECK(motion.offset_y == 0.0f);
            CHECK(motion.opacity == 1.0f);
        }
    }
    CHECK(gameplay_judgement_animation("PG", 0).scale > 1.0f);
    CHECK(gameplay_judgement_animation("PG", 220).scale == 1.0f);
    CHECK(gameplay_judgement_rgb("PG") == 0xFFE18A);
    CHECK(gameplay_judgement_rgb("GR") == 0x6EE7F2);
    CHECK(gameplay_judgement_rgb("G") == 0xAEB5BF);
    CHECK(gameplay_timing_feedback_text(-28, "GR") == L"FAST -28 ms");
    CHECK(gameplay_timing_feedback_text(28, "GR") == L"SLOW +28 ms");
}
