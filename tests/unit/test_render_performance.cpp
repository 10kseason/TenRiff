#include "render/SettingsListLayout.h"
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
#include "render/MenuWindow.h"
#include "render/SkinGameplayPreview.h"
#include "timing/HighResClock.h"
#include <cstdlib>
#include <iostream>
#include <numeric>

#ifdef _WIN32
namespace tenriff::render {
struct MenuWindowResolutionTestAccess {
    static void resize(MenuWindow& window, int width, int height) {
        window.width_ = width;
        window.height_ = height;
        window.update_layout();
    }
    static float scale(const MenuWindow& window) { return window.scale_; }
    static float x(const MenuWindow& window) { return window.offset_x_; }
    static float y(const MenuWindow& window) { return window.offset_y_; }
    static bool point(const MenuWindow& window, int x, int y, float& out_x, float& out_y) {
        return window.translate_window_point(x, y, &out_x, &out_y);
    }
};
}
#endif

namespace {

using tenriff::render::PerformanceTracker;

#ifdef _WIN32
TEST_CASE("render layout and pointer mapping preserve aspect at uncommon resolutions") {
    using Access = tenriff::render::MenuWindowResolutionTestAccess;
    tenriff::render::MenuWindow window;
    for (const auto [width, height] : {std::pair{1600, 900}, std::pair{1366, 768}, std::pair{1280, 800},
                                      std::pair{1024, 768}, std::pair{3440, 1440}, std::pair{900, 1600}}) {
        Access::resize(window, width, height);
        const float scale = Access::scale(window);
        CHECK(scale == doctest::Approx(std::min(width / 1920.0f, height / 1080.0f)));
        float x = 0, y = 0;
        CHECK(Access::point(window, width / 2, height / 2, x, y));
        CHECK(x == doctest::Approx(960).epsilon(0.002));
        CHECK(y == doctest::Approx(540).epsilon(0.002));
        CHECK(Access::point(window, static_cast<int>(Access::x(window) + scale * 400),
                           static_cast<int>(Access::y(window) + scale * 300), x, y));
        // Pointer coordinates are whole physical pixels before inverse scaling.
        CHECK(std::abs(x - 400) <= 1.0f / scale + 0.001f);
        CHECK(std::abs(y - 300) <= 1.0f / scale + 0.001f);
        if (Access::x(window) > 1.0f) CHECK_FALSE(Access::point(window, 0, height / 2, x, y));
        if (Access::y(window) > 1.0f) CHECK_FALSE(Access::point(window, width / 2, 0, x, y));
    }
}
#endif

TEST_CASE("skin preview preserves gameplay positions and imported geometry with a static sample chart") {
    tenriff::render::SkinPreviewData preview;
    preview.lane_count = 16;
    preview.mode_label = "16K";
    preview.selected_lane = 8;
    preview.gameplay_field_offset_x = -260;
    preview.judgement_position = 0.4;
    preview.judgement_offset_x = -120;
    preview.combo_position = 0.5;
    preview.combo_offset_x = 120;
    preview.note_height_scale = 4;
    preview.lane_center_gap_scale = 1.5;
    preview.lane_width_scale_count = 16;
    preview.lane_width_scales.fill(1.75);
    preview.lane_spacing_scale_count = 15;
    preview.lane_spacing_scales.fill(0.5);
    preview.skin_source = "lr2";
    preview.lr2_resolution_override = "fhd";
    preview.key_labels[7] = "Space";
    preview.key_backdrop_brightness = 1.8;
    preview.key_backdrop_height = 0.2;
    preview.note_fade_in = 0.35;
    preview.note_fade_out = 0.65;
    const auto hud = tenriff::render::make_skin_gameplay_preview(preview);
    CHECK(hud.gameplay_field_offset_x == -260);
    CHECK(hud.judgement_offset_x == -120);
    CHECK(hud.combo_offset_x == 120);
    CHECK(hud.judgement_position == 0.4);
    CHECK(hud.combo_position == 0.5);
    CHECK(hud.note_height_scale == 4);
    CHECK(hud.lane_center_gap_scale == 1.5);
    CHECK(hud.lane_spacing_scale_count == 15u);
    CHECK(hud.lane_spacing_scales[7] == 0.5);
    CHECK(hud.lane_width_scales[7] == 1.75);
    CHECK(hud.skin_source == "lr2");
    CHECK(hud.lr2_resolution_override == "fhd");
    CHECK(hud.key_labels[7] == "Space");
    CHECK(hud.key_backdrop_brightness == doctest::Approx(1.8));
    CHECK(hud.key_backdrop_height == doctest::Approx(0.2));
    CHECK(hud.note_fade_in == doctest::Approx(0.35));
    CHECK(hud.note_fade_out == doctest::Approx(0.65));
    CHECK(hud.audio_sample_time_ns == 0);
    CHECK(hud.note_count == 16u);
    CHECK(hud.notes[7].hold);
    CHECK(hud.notes[7].tail_sample > hud.notes[7].start_sample);
}

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

TEST_CASE("render pacing does not add an idle frame after a budget overrun") {
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'050) == 1'100);
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'100) == 1'100);
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'101) == 1'101);
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'299) == 1'299);
    CHECK(tenriff::render::advance_frame_deadline_ns(1'000, 100, 1'300) == 1'300);
}

TEST_CASE("render pacing keeps a dense burst proportional to work at common fps caps") {
    for (const int fps : {60, 144, 240, 300, 600, 1050, 1500}) {
        const int64_t interval = 1'000'000'000LL / fps;
        int64_t start = 1'000'000'000;
        for (int frame = 0; frame < 120; ++frame) {
            const int64_t work = interval + (frame % 2 == 0 ? -interval / 20 : interval / 20);
            const int64_t end = start + work;
            const auto next = tenriff::render::advance_frame_deadline_ns(start, interval, end);
            CHECK(next - start >= interval);
            CHECK(next - start <= interval + interval / 20);
            CHECK(next >= end);
            start = next;
        }
        // Recover from a stall without accumulated deadline debt or a catch-up burst.
        CHECK(tenriff::render::advance_frame_deadline_ns(start, interval, start + 20 * interval) == start + 20 * interval);
    }
}

TEST_CASE("render thread optional dense pacing benchmark") {
    if (!std::getenv("TENRIFF_BENCH_RENDER_PACING")) return;
    using tenriff::timing::HighResClock;
    for (const int work_us : {700, 980, 1100}) {
        tenriff::render::RenderThread thread;
        std::vector<int64_t> starts;
        starts.reserve(1100);
        std::atomic<bool> done{false};
        REQUIRE(thread.initialize({false, 1050}, [&] {
            if (done.load()) return;
            const auto start = HighResClock::now_ns();
            starts.push_back(start);
            while (HighResClock::now_ns() < start + work_us * 1000LL)
                std::atomic_signal_fence(std::memory_order_seq_cst);
            if (starts.size() == 1060) done.store(true);
        }));
        REQUIRE(thread.start());
        const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        while (!done.load() && std::chrono::steady_clock::now() < timeout)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        thread.stop();
        REQUIRE(done.load());
        std::vector<double> intervals;
        for (std::size_t i = 60; i < starts.size(); ++i)
            intervals.push_back((starts[i] - starts[i - 1]) / 1e6);
        std::sort(intervals.begin(), intervals.end());
        const auto q = [&](double p) { return intervals[static_cast<std::size_t>(std::ceil(p * intervals.size())) - 1]; };
        std::cout << "[render-pacing-bench] fps=1050 work_us=" << work_us
                  << " samples=" << intervals.size() << " p50_ms=" << q(.50)
                  << " p95_ms=" << q(.95) << " p99_ms=" << q(.99)
                  << " max_ms=" << intervals.back() << '\n';
    }
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

TEST_CASE("long-note body overlaps cap centers to bridge transparent and sloped edges") {
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
    CHECK(body.top == doctest::Approx(300.0f));
    CHECK(body.bottom == doctest::Approx(920.0f));

    const auto active = tenriff::render::compute_gameplay_hold_body_geometry(
        100.0f, 70.0f, 130.0f, 900.0f, 920.0f, 310.0f, 300.0f, false, false, 1.0);
    CHECK(active.top == doctest::Approx(300.0f));
    CHECK(active.bottom == doctest::Approx(920.0f));
}

TEST_CASE("short long notes retain a continuous body even when tall caps overlap") {
    // At 400% height the old edge-only join inverted this interval (140..120).
    for (const bool head_visible : {false, true}) {
        for (const bool tail_visible : {false, true}) {
            const auto body = tenriff::render::compute_gameplay_hold_body_geometry(
                100.0f, 70.0f, 130.0f, 120.0f, 160.0f, 140.0f, 110.0f,
                head_visible, tail_visible, 0.75);
            CHECK(body.top == doctest::Approx(110.0f));
            CHECK(body.bottom == doctest::Approx(160.0f));
            CHECK(body.left == doctest::Approx(77.5f));
            CHECK(body.right == doctest::Approx(122.5f));
        }
    }
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
TEST_CASE("solo gameplay header column stops before the playfield") {
    using tenriff::render::gameplay_solo_header_right;

    // Default 8K field at x=470: the column ends 32px short of the field.
    CHECK(gameplay_solo_header_right(84.0f, 1836.0f, 470.0f) == doctest::Approx(438.0f));
    // A field far to the right never widens the column past the legacy span.
    CHECK(gameplay_solo_header_right(84.0f, 1836.0f, 1400.0f) == doctest::Approx(1101.6f).epsilon(1e-3));
    // No usable column left of a field dragged hard left: keep the legacy span.
    CHECK(gameplay_solo_header_right(84.0f, 1836.0f, 300.0f) == doctest::Approx(1101.6f).epsilon(1e-3));
    CHECK(gameplay_solo_header_right(84.0f, 1836.0f, 356.0f) == doctest::Approx(324.0f));
}
TEST_CASE("ghost battle summaries retain every row in narrow and wide skin fields") {
    using tenriff::render::compute_gameplay_battle_summary_layout;
    for (const float width : {280.0f, 560.0f, 784.0f}) {
        const auto layout = compute_gameplay_battle_summary_layout(
            530.0f - width * 0.5f, 530.0f + width * 0.5f, 0.0f, 1080.0f,
            155.0f, 350.0f, 275.0f, 390.0f);
        CHECK(layout.right - layout.left >= 380.0f);
        CHECK(layout.bottom < 155.0f);
        CHECK(layout.row_edges.front() > layout.top);
        CHECK(layout.row_edges.back() < layout.bottom);
        for (std::size_t row = 1; row < layout.row_edges.size(); ++row) {
            CHECK(layout.row_edges[row] - layout.row_edges[row - 1] >= 22.0f);
        }
        const auto ghost = compute_gameplay_battle_summary_layout(
            1390.0f - width * 0.5f, 1390.0f + width * 0.5f, 0.0f, 1080.0f,
            155.0f, 350.0f, 275.0f, 390.0f, true);
        CHECK(ghost.top == doctest::Approx(layout.top));
        CHECK(ghost.right - ghost.left == doctest::Approx(layout.right - layout.left));
        CHECK(layout.right < ghost.left);
        CHECK(layout.left >= 530.0f - width * 0.5f);
        CHECK(ghost.right <= 1390.0f + width * 0.5f);
    }
}
TEST_CASE("ghost battle summaries avoid separately positioned judgement and combo text") {
    using tenriff::render::compute_gameplay_battle_summary_layout;
    // A large judgement near the top must not be covered by a fixed top card.
    const auto large = compute_gameplay_battle_summary_layout(
        250.0f, 810.0f, 0.0f, 1080.0f, 40.0f, 270.0f, 320.0f, 470.0f);
    CHECK(large.top > 470.0f);
    CHECK(large.bottom < 1080.0f);
    // Reversing the configured HUD positions preserves the same free interval.
    const auto reversed = compute_gameplay_battle_summary_layout(
        250.0f, 810.0f, 0.0f, 1080.0f, 320.0f, 470.0f, 40.0f, 270.0f);
    CHECK(reversed.top == doctest::Approx(large.top));
    const auto low_hud = compute_gameplay_battle_summary_layout(
        250.0f, 810.0f, 0.0f, 1080.0f, 760.0f, 920.0f, 900.0f, 1020.0f);
    CHECK(low_hud.bottom < 760.0f);
}

TEST_CASE("wide ghost fields retain readable metadata columns and narrow progress clearance") {
    using namespace tenriff::render;
    const auto wide = compute_gameplay_battle_summary_layout(138.0f, 922.0f,
        0.0f, 1080.0f, 155.0f, 350.0f, 275.0f, 390.0f);
    const auto columns = compute_gameplay_battle_summary_columns(wide, true);
    CHECK(columns.metadata_right - columns.metadata_left >= 340.0f);
    CHECK(columns.stats_right - columns.stats_left >= 370.0f);
    CHECK(columns.metadata_right < columns.stats_left);
    const auto player = compute_gameplay_battle_summary_layout(390.0f, 670.0f,
        0.0f, 1080.0f, 155.0f, 350.0f, 275.0f, 390.0f);
    const auto ghost = compute_gameplay_battle_summary_layout(1250.0f, 1530.0f,
        0.0f, 1080.0f, 155.0f, 350.0f, 275.0f, 390.0f, true);
    const auto progress = compute_gameplay_progress_track_layout(24.0f, 1836.0f,
        328.0f, player.right + 84.0f, true, ghost.left, 1612.0f, 16.0f);
    CHECK((progress.right < player.left || progress.left > player.right));
    CHECK((progress.right < ghost.left || progress.left > ghost.right));
    CHECK(progress.right - progress.left >= 250.0f);
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

TEST_CASE("native key travel responds immediately and releases consistently across render caps") {
    using tenriff::render::advance_native_key_travel;
    CHECK(advance_native_key_travel(0.0f, true, 1.0 / 60.0) > 0.80f);
    CHECK(advance_native_key_travel(1.0f, true, 1.0 / 144.0) == doctest::Approx(1.0f));
    float reference = 0.0f;
    for (int fps : {60, 144, 300, 1000}) {
        float travel = 1.0f;
        for (int frame = 0; frame < fps; ++frame)
            travel = advance_native_key_travel(travel, false, 0.1 / fps);
        if (fps == 60) reference = travel;
        CHECK(travel == doctest::Approx(reference).epsilon(0.0001));
        CHECK(travel < 0.10f);
        CHECK(travel > 0.0f);
    }
    CHECK(advance_native_key_travel(1.0f, false, 0.5) == 0.0f);
    CHECK(advance_native_key_travel(0.3f, true, -1.0) == doctest::Approx(0.3f));
}

TEST_CASE("native key bounds fit every 4K to 16K lane including extreme judge positions") {
    using tenriff::render::native_key_bounds;
    for (int keys = 4; keys <= 16; ++keys) {
        for (float field_width : {280.0f, 560.0f, 980.0f, 1372.0f}) {
            const float lane_width = field_width / keys;
            for (int lane = 0; lane < keys; ++lane) {
                const float left = lane_width * lane;
                for (float gear_top : {0.0f, 540.0f, 884.0f, 1080.0f}) {
                    const auto key = native_key_bounds(left, left + lane_width, 0.0f, 1080.0f, gear_top);
                    CHECK(key.left >= left);
                    CHECK(key.right <= left + lane_width);
                    CHECK(key.right > key.left);
                    CHECK(key.top >= 0.0f);
                    CHECK(key.bottom <= 1080.0f);
                    CHECK(key.bottom > key.top);
                    CHECK(key.bottom - key.top <= 180.0f);
                }
            }
        }
    }
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

TEST_CASE("disabled render metrics drop history and re-enable without an idle-time spike") {
    tenriff::render::RenderThread thread;
    thread.record_presented_frame_ns(100'000'000);
    thread.record_presented_frame_ns(104'000'000);
    REQUIRE(thread.performance_snapshot().valid);
    // This is the production path with the overlay hidden, including an
    // occluded frame (no successful Present timestamp).
    thread.record_presented_frame_ns(0, false);
    for (int i = 1; i <= 2000; ++i)
        thread.record_presented_frame_ns(104'000'000 + i * 1'000'000LL, false);
    CHECK_FALSE(thread.performance_snapshot().valid);
    CHECK(thread.performance_snapshot().sample_count == 0u);
    thread.record_presented_frame_ns(3'000'000'000);
    CHECK_FALSE(thread.performance_snapshot().valid);
    thread.record_presented_frame_ns(3'004'000'000);
    CHECK(thread.performance_snapshot().sample_count == 1u);
    CHECK(thread.performance_snapshot().average_frame_ms == doctest::Approx(4.0));
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

TEST_CASE("Native gameplay edited key geometry and response remain bounded") {
    tenriff::app::NativeGameplaySkinStyle style;
    style.metrics["key_min_height"]=200;
    style.metrics["key_max_height"]=10;
    style.metrics["key_inset"]=32;
    style.metrics["key_inset_ratio"]=.45f;
    const auto bounds=tenriff::render::native_key_bounds(20,40,0,100,90,style);
    CHECK(bounds.left<bounds.right);
    CHECK(bounds.top>=0);
    CHECK(bounds.bottom<=100);
    CHECK(tenriff::render::advance_native_key_travel(0,true,.016,1,1)<.1f);
    CHECK(tenriff::render::advance_native_key_travel(0,true,.016,500,500)>.99f);
    style.rects["score"]={100,-50,-1000,-1000};
    const auto rect=tenriff::render::native_gameplay_rect(style,"score",{0,0,50,30});
    CHECK(rect[0]==100);
    CHECK(rect[1]==-50);
    CHECK(rect[2]>rect[0]);
    CHECK(rect[3]>rect[1]);
}

TEST_CASE("group headings preserve selected row visibility and do not become input rows") {
    std::vector<tenriff::render::MenuRowData> rows(18);
    for (int i = 0; i < 18; ++i) rows[i].category = std::to_string(i / 3);
    for (int selected = 0; selected < 18; ++selected) {
        const auto window = tenriff::render::settings_list_window(rows, selected, 300, 66, 28);
        CHECK(window.start <= selected);
        CHECK(window.start + window.count > selected);
        float height = 0;
        for (int i = window.start; i < window.start + window.count; ++i)
            height += 66 + (tenriff::render::settings_category_heading(rows, i, window.start) ? 28 : 0);
        CHECK(height <= 300);
    }
}

TEST_CASE("skin preview refreshes cached HUD text for style settings and language without per-frame churn") {
    using tenriff::render::make_skin_gameplay_preview;
    using tenriff::ui::Language;
    tenriff::render::SkinPreviewData preview;
    preview.lane_count = 8;
    preview.mode_label = "8K";
    preview.selected_lane = 1;
    preview.hud_layout = "classic";
    const auto classic = make_skin_gameplay_preview(preview, 900'000'000LL, Language::English);
    preview.hud_layout = "studio";
    const auto studio = make_skin_gameplay_preview(preview, 900'000'000LL, Language::English);
    CHECK(studio.hud_layout == "studio");
    CHECK(studio.text_revision != classic.text_revision);
    CHECK(studio.text_revision != 0);
    preview.judgement_line_position = 0.64;
    const auto line = make_skin_gameplay_preview(preview, 900'000'000LL, Language::English);
    CHECK(line.judgement_line_position == 0.64);
    CHECK(line.text_revision != studio.text_revision);
    const auto korean = make_skin_gameplay_preview(preview, 900'000'000LL, Language::Korean);
    CHECK(korean.text_revision != line.text_revision);
    preview.mode_label = "7+1";
    const auto relabeled = make_skin_gameplay_preview(preview, 900'000'000LL, Language::Korean);
    CHECK(relabeled.text_revision != korean.text_revision);

    preview.gameplay_field_offset_x = -240;
    preview.combo_font_scale = 1.5;
    preview.show_timing_feedback = false;
    const auto animated = make_skin_gameplay_preview(preview, 1'400'000'000LL, Language::Korean);
    CHECK(animated.text_revision == relabeled.text_revision);
    CHECK(animated.lane_pressed[0] != relabeled.lane_pressed[0]);
}

TEST_CASE("skin preview carries the LR2 Studio HUD and riff map visibility independently") {
    tenriff::render::SkinPreviewData preview;
    preview.hud_layout = "studio";
    preview.skin_source = "lr2";
    preview.hud_riff_map_visible = true;
    const auto visible = tenriff::render::make_skin_gameplay_preview(preview);
    CHECK(visible.skin_source == "lr2");
    CHECK(visible.hud_layout == "studio");
    CHECK(visible.hud_riff_map_visible);
    preview.hud_riff_map_visible = false;
    const auto hidden = tenriff::render::make_skin_gameplay_preview(preview);
    CHECK_FALSE(hidden.hud_riff_map_visible);
    // Graph visibility invalidates static geometry, not unchanged score strings.
    CHECK(hidden.text_revision == visible.text_revision);
    CHECK(hidden.lane_count == visible.lane_count);
    CHECK(hidden.note_width_scale == visible.note_width_scale);
}

TEST_CASE("skin preview carries independent timing switches and positions") {
    tenriff::render::SkinPreviewData preview;
    preview.show_timing_feedback = false;
    preview.show_timing_bar = true;
    preview.timing_bar_always_visible = true;
    preview.timing_text_offset_x = 120;
    preview.timing_text_offset_y = -70;
    preview.timing_bar_offset_x = -80;
    preview.timing_bar_offset_y = 140;
    auto hud = tenriff::render::make_skin_gameplay_preview(preview);
    CHECK_FALSE(hud.show_timing_feedback);
    CHECK(hud.show_timing_bar);
    CHECK(hud.timing_bar_always_visible);
    CHECK(hud.has_timing_feedback);
    CHECK(hud.timing_feedback_delta_ms == -18.0);
    CHECK(hud.feedback == "GR");
    CHECK(hud.feedback_delta_ms != 0);
    CHECK(hud.timing_text_offset_x == 120);
    CHECK(hud.timing_text_offset_y == -70);
    CHECK(hud.timing_bar_offset_x == -80);
    CHECK(hud.timing_bar_offset_y == 140);
}

TEST_CASE("skin preview drives real lane activity for each hit burst style") {
    tenriff::render::SkinPreviewData preview;
    preview.lane_count = 8;
    preview.selected_lane = 8;
    for (const auto* style : {"prism", "ring", "spark"}) {
        preview.hit_burst_style = style;
        auto hit = tenriff::render::make_skin_gameplay_preview(preview, 900'000'000LL);
        auto decay = tenriff::render::make_skin_gameplay_preview(preview, 1'060'000'000LL);
        auto idle = tenriff::render::make_skin_gameplay_preview(preview, 1'400'000'000LL);
        CHECK(hit.hit_burst_style == style);
        CHECK(hit.lane_activity_count == 8);
        CHECK(hit.lane_activity[7] == 1.0f);
        CHECK(decay.lane_activity[7] == doctest::Approx(0.5));
        CHECK(idle.lane_activity[7] == 0.0f);
        CHECK(hit.lane_activity[0] == 0.0f);
        CHECK(hit.activity_publish_time_ns == 900'000'000LL);
        CHECK(hit.lane_pressed[7] == 1);
        CHECK(idle.lane_pressed[7] == 0);
    }
}

TEST_CASE("ghost battle reserves solo physical player width before fitting the ghost") {
    for (const int keys : {5, 8, 16}) {
        for (const double scale : {0.50, 0.60, 1.0, 1.4}) {
            const auto widths = tenriff::render::compute_gameplay_battle_widths(scale);
            CHECK(widths.player / keys == doctest::Approx(
                tenriff::render::compute_gameplay_playfield_width(980.0f, scale) / keys));
            CHECK(widths.ghost <= widths.player);
            CHECK(widths.ghost >= 160);
            CHECK(widths.player + widths.ghost + 220 <= 1920.01f);
        }
    }
}

TEST_CASE("LN body alpha keeps the complete configured opacity range linear") {
    for (const double value : {1.0, 0.50, 0.45, 0.0}) {
        CHECK(tenriff::render::gameplay_hold_body_alpha(value, 1.0) == doctest::Approx(value));
        CHECK(tenriff::render::gameplay_hold_body_alpha(value, 0.8) == doctest::Approx(value * 0.8));
    }
    CHECK(tenriff::render::gameplay_hold_body_alpha(-1, 1) == 0);
    CHECK(tenriff::render::gameplay_hold_body_alpha(2, 1) == 1);
}
