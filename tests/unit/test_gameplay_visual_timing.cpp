#include "doctest/doctest.h"

#include <cmath>
#include <memory>

#include "app/GameplayHudRevisions.h"
#include "app/GameSessionInputTiming.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
#include "gameplay/GameplayChart.h"
#include "render/GameplayMotion.h"

namespace {

constexpr int64_t kVisualTestLeadInMs = 3000;

int64_t visual_test_sample(double song_ms, int sample_rate) {
    return static_cast<int64_t>(std::llround(
        (song_ms + kVisualTestLeadInMs) * static_cast<double>(sample_rate) / 1000.0));
}

tenriff::gameplay::GameplayChart visual_test_chart(int sample_rate, bool reverse = false) {
    tenriff::chart::BmsChart bms;
    bms.base_bpm = 120.0;
    bms.bpm["01"] = 240.0;
    bms.bpm["02"] = 120.0;
    bms.stop["01"] = 48.0;
    bms.commands.push_back({1, "08", "01"});
    bms.commands.push_back({2, "09", "01"});
    bms.commands.push_back({3, "08", "02"});
    if (reverse) {
        bms.scroll["01"] = -1.0;
        bms.scroll["02"] = 1.0;
        bms.commands.push_back({3, "SC", "01"});
        bms.commands.push_back({4, "SC", "02"});
    }
    bms.commands.push_back({4, "11", "01"});
    const auto normalized = tenriff::chart::BmsChartNormalizer{}.normalize(bms);
    REQUIRE(normalized.success());
    const auto timeline = tenriff::chart::BmsTimelineBuilder{}.build(normalized.chart, sample_rate);
    REQUIRE(timeline.success());
    auto chart = tenriff::gameplay::from_bms_timeline(timeline.timeline, 1.0);
    tenriff::gameplay::offset_gameplay_chart_samples(chart, visual_test_sample(0, sample_rate));
    return chart;
}

double visual_test_position(const tenriff::gameplay::GameplayChart& chart,
                            int sample_rate,
                            double anchor_ms,
                            double elapsed_ms,
                            double visual_offset_ms = 0.0) {
    tenriff::render::GameplayMotionState state;
    state.current_sample = visual_test_sample(anchor_ms, sample_rate);
    state.duration_samples = chart.duration_samples;
    state.sample_rate = sample_rate;
    state.audio_sample_time_ns = 1'000'000'000;
    state.audio_buffer_frames = static_cast<uint32_t>(std::llround(sample_rate * 0.01));
    state.visual_offset_ms = visual_offset_ms;
    const int64_t now_ns = state.audio_sample_time_ns +
        static_cast<int64_t>(std::llround(elapsed_ms * 1'000'000.0));
    const auto motion = tenriff::render::compute_gameplay_motion_diagnostics(state, now_ns);
    return tenriff::render::compute_gameplay_display_visual_position(
        &chart.scroll_segments, motion.display_sample, state.current_sample,
        chart.visual_position_at(state.current_sample), chart.visual_velocity_at(state.current_sample));
}

}  // namespace

TEST_CASE("visual timing resolves BMS BPM and STOP boundaries at the final display sample") {
    for (const int sample_rate : {44100, 48000, 96000}) {
        const auto chart = visual_test_chart(sample_rate);
        // Authored positions: BPM doubles at 2000ms; STOP is 3000..3250ms;
        // BPM halves at 4250ms. All samples also carry the normal 3s lead-in.
        CHECK(chart.visual_position_at(visual_test_sample(2000, sample_rate)) == doctest::Approx(1.0));
        CHECK(chart.visual_position_at(visual_test_sample(3000, sample_rate)) == doctest::Approx(2.0));
        CHECK(chart.visual_position_at(visual_test_sample(3250, sample_rate)) == doctest::Approx(2.0));
        CHECK(chart.visual_position_at(visual_test_sample(4250, sample_rate)) == doctest::Approx(3.0));
        const double sample_tolerance = 2.0 / sample_rate;
        CHECK(std::abs(visual_test_position(chart, sample_rate, 1995, 10) - 1.005) < sample_tolerance);
        CHECK(visual_test_position(chart, sample_rate, 2995, 10) == doctest::Approx(2.0));
        CHECK(std::abs(visual_test_position(chart, sample_rate, 3245, 10) - 2.005) < sample_tolerance);
        CHECK(std::abs(visual_test_position(chart, sample_rate, 4245, 10) - 3.0025) < sample_tolerance);
    }
}

TEST_CASE("visual timing applies both offset signs across multiple chart boundaries") {
    for (const int sample_rate : {44100, 48000, 96000}) {
        const auto chart = visual_test_chart(sample_rate);
        // Legal offsets cross both STOP entry and exit; the anchor may be on
        // either side or inside STOP, where a velocity-only offset would be 0.
        CHECK(visual_test_position(chart, sample_rate, 2990, 0, 300) == doctest::Approx(2.04));
        CHECK(visual_test_position(chart, sample_rate, 3260, 0, -300) == doctest::Approx(1.96));
        CHECK(visual_test_position(chart, sample_rate, 3100, 0, -200) == doctest::Approx(1.9));
        CHECK(visual_test_position(chart, sample_rate, 3100, 0, 200) == doctest::Approx(2.05));
        CHECK(visual_test_position(chart, sample_rate, 4260, 0, -100) == doctest::Approx(2.91));
        CHECK(visual_test_position(chart, sample_rate, -2990, 0, -500) == doctest::Approx(0.0));
    }
}

TEST_CASE("visual timing does not step backwards when an anchor crosses STOP or a slower BPM") {
    const int sample_rate = 48000;
    const auto chart = visual_test_chart(sample_rate);
    for (const double visual_offset_ms : {-200.0, 0.0, 200.0}) {
        for (const int start_ms : {2780, 2980, 3180, 4020, 4220, 4420}) {
            double previous_position = -1.0;
            double previous_note_y = -1.0;
            for (int frame = 0; frame < 80; ++frame) {
                const double now_ms = start_ms + frame;
                // Exact sample/QPC anchors published every 8ms. The frame time
                // remains monotonic while the selected anchor crosses a boundary.
                const double anchor_ms = std::floor(now_ms / 8.0) * 8.0;
                const double position = visual_test_position(
                    chart, sample_rate, anchor_ms, now_ms - anchor_ms, visual_offset_ms);
                const double expected = chart.visual_position_at(
                    visual_test_sample(now_ms + visual_offset_ms, sample_rate));
                const double note_y = tenriff::render::compute_gameplay_visual_y_normalized(
                    4.0, position, 4.0, 0.2, 0.82);
                CHECK(position == doctest::Approx(expected));
                CHECK(position + 1e-12 >= previous_position);
                CHECK(note_y + 1e-12 >= previous_note_y);
                previous_position = position;
                previous_note_y = note_y;
            }
        }
    }
}

TEST_CASE("visual timing preserves authored reverse scrolling instead of clamping visual position") {
    const auto chart = visual_test_chart(48000, true);
    CHECK(visual_test_position(chart, 48000, 4245, 10) == doctest::Approx(2.9975));
    const double first = visual_test_position(chart, 48000, 4344, 6);
    const double second = visual_test_position(chart, 48000, 4352, 8);
    CHECK(first == doctest::Approx(2.95));
    CHECK(second == doctest::Approx(2.945));
    CHECK(second < first);
    CHECK(visual_test_position(chart, 48000, 6260, 0, -100) == doctest::Approx(2.045));
}

TEST_CASE("visual timing keeps sample-only charts and synthetic previews distinct") {
    const std::vector<tenriff::gameplay::ScrollSegment> sample_map;
    CHECK(tenriff::render::compute_gameplay_display_visual_position(
              &sample_map, 1500, 1400, 123.0, 0.0) == doctest::Approx(1500.0));
    CHECK(tenriff::render::compute_gameplay_display_visual_position(
              nullptr, 1500, 1400, 1.0, 0.001) == doctest::Approx(1.1));
}

TEST_CASE("visual timing invalidates motion when a new immutable chart map replaces the old one") {
    tenriff::app::GameplayHudRevisionInput previous;
    previous.visual_scroll_segments =
        std::make_shared<const std::vector<tenriff::gameplay::ScrollSegment>>();
    auto next = previous;
    CHECK_FALSE(tenriff::app::diff_gameplay_hud_revisions(previous, next).motion_changed);
    next.visual_scroll_segments =
        std::make_shared<const std::vector<tenriff::gameplay::ScrollSegment>>();
    const auto flags = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(flags.motion_changed);
    CHECK_FALSE(flags.text_changed);
}

TEST_CASE("visual timing uses the shared clock rate while keeping visual offset in chart time") {
    for (const int sample_rate : {44100, 48000, 96000}) {
        for (const double clock_rate : {0.98, 1.0, 1.02}) {
            tenriff::render::GameplayMotionState state;
            state.current_sample = 5 * sample_rate;
            state.duration_samples = 10 * sample_rate;
            state.sample_rate = sample_rate;
            state.audio_sample_time_ns = 1'000'000'000;
            state.audio_buffer_frames = static_cast<uint32_t>(sample_rate / 100);
            state.audio_clock_rate = clock_rate;
            for (const double offset_ms : {-20.0, 0.0, 20.0}) {
                state.visual_offset_ms = offset_ms;
                const auto motion = tenriff::render::compute_gameplay_motion_diagnostics(
                    state, state.audio_sample_time_ns + 10'000'000);
                const int64_t expected_elapsed = static_cast<int64_t>(
                    std::llround(0.01 * sample_rate * clock_rate));
                const int64_t fixed_offset = static_cast<int64_t>(
                    std::llround(offset_ms * sample_rate / 1000.0));
                CHECK(motion.extrapolated_samples == expected_elapsed);
                CHECK(motion.display_sample == state.current_sample + expected_elapsed + fixed_offset);

                state.finished = true;  // The renderer also uses this flag while paused.
                const auto frozen = tenriff::render::compute_gameplay_motion_diagnostics(
                    state, state.audio_sample_time_ns + 10'000'000);
                CHECK(frozen.extrapolated_samples == 0);
                CHECK(frozen.display_sample == state.current_sample + fixed_offset);
                state.finished = false;
            }
        }
    }
}

TEST_CASE("visual timing invalidates only motion when the shared clock rate changes") {
    tenriff::app::GameplayHudRevisionInput previous;
    auto next = previous;
    next.audio_clock_rate = 1.02;
    const auto flags = tenriff::app::diff_gameplay_hud_revisions(previous, next);
    CHECK(flags.motion_changed);
    CHECK_FALSE(flags.text_changed);
}

TEST_CASE("visual timing holds the last rendered sample across delayed slower clock publication") {
    constexpr int64_t epoch = 1'000'000'000;
    for (const int sample_rate : {44100, 48000, 96000}) {
        for (const int delay_ms : {0, 2, 8, 20}) {
            tenriff::app::GameplayInputClock clock;
            clock.observe(epoch, sample_rate, sample_rate);
            tenriff::render::GameplayMotionState old_state;
            old_state.current_sample = clock.observe(epoch + 12'000'000,
                static_cast<int64_t>(std::llround(sample_rate * 1.062)), sample_rate);
            old_state.duration_samples = 10 * sample_rate;
            old_state.sample_rate = sample_rate;
            old_state.audio_sample_time_ns = epoch + 12'000'000;
            old_state.audio_buffer_frames = static_cast<uint32_t>(std::llround(sample_rate * 0.012));
            old_state.audio_clock_rate = clock.rate();
            auto next_state = old_state;
            next_state.current_sample = clock.observe(epoch + 24'000'000,
                static_cast<int64_t>(std::llround(sample_rate * 1.004)), sample_rate);
            next_state.audio_sample_time_ns = epoch + 24'000'000;
            next_state.audio_clock_rate = clock.rate();
            const int64_t publication_ns = next_state.audio_sample_time_ns + delay_ms * 1'000'000LL;
            for (const double offset_ms : {-200.0, 0.0, 200.0}) {
                old_state.visual_offset_ms = offset_ms;
                next_state.visual_offset_ms = offset_ms;
                tenriff::render::GameplayDisplaySampleCache cache;
                const auto before = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
                    old_state, publication_ns - 100'000, cache);
                const auto raw = tenriff::render::compute_gameplay_motion_diagnostics(next_state, publication_ns);
                const auto after = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
                    next_state, publication_ns, cache);
                CHECK(after.display_sample >= before.display_sample);
                if (delay_ms == 8) {
                    CHECK(raw.display_sample < before.display_sample);
                    CHECK(after.display_hold_samples > 0);
                    CHECK(after.display_hold_samples <= static_cast<int64_t>(std::ceil(sample_rate * 0.00025)));
                    const auto recovered = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
                        next_state, publication_ns + 400'000, cache);
                    CHECK(recovered.display_hold_samples == 0);
                } else {
                    CHECK(after.display_hold_samples == 0);
                }
            }
        }
    }
}

TEST_CASE("visual timing display cache resets on pause seek and clock identity changes") {
    tenriff::render::GameplayDisplaySampleCache cache;
    tenriff::render::GameplayMotionState state;
    state.current_sample = 10000;
    state.duration_samples = 100000;
    state.sample_rate = 48000;
    state.audio_sample_time_ns = 1'000'000'000;
    state.audio_buffer_frames = 576;
    auto before = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 8'000'000, cache);
    state.finished = true; // The draw path also sets this for pause.
    auto frozen = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 9'000'000, cache);
    CHECK(frozen.display_sample == state.current_sample);
    CHECK(frozen.display_sample < before.display_sample);
    state.finished = false;
    state.audio_sample_time_ns += 10'000'000;
    CHECK(tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns, cache).display_sample == state.current_sample);

    state.current_sample = 100; // A backwards anchor is an explicit discontinuity.
    state.audio_sample_time_ns += 10'000'000;
    CHECK(tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns, cache).display_sample == 100);
    before = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 8'000'000, cache);
    cache.reset(); // Called by the owner on a new chart or an explicit seek.
    state.audio_sample_time_ns += 10'000'000;
    CHECK(tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns, cache).display_sample == 100);
    before = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 8'000'000, cache);
    state.sample_rate = 44100;
    state.duration_samples = 90000;
    state.audio_sample_time_ns += 10'000'000;
    CHECK(tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns, cache).display_sample == 100);
}

TEST_CASE("visual timing display cache preserves visual offset edits and authored reverse scroll") {
    const auto chart = visual_test_chart(48000, true);
    tenriff::render::GameplayDisplaySampleCache cache;
    tenriff::render::GameplayMotionState state;
    state.current_sample = visual_test_sample(4300, 48000);
    state.duration_samples = chart.duration_samples;
    state.sample_rate = 48000;
    state.audio_sample_time_ns = 1'000'000'000;
    state.audio_buffer_frames = 576;
    const auto first = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 1'000'000, cache);
    const auto next = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 2'000'000, cache);
    CHECK(next.display_sample > first.display_sample);
    CHECK(chart.visual_position_at(next.display_sample) < chart.visual_position_at(first.display_sample));
    state.visual_offset_ms = -200.0;
    const auto offset = tenriff::render::compute_continuous_gameplay_motion_diagnostics(
        state, state.audio_sample_time_ns + 2'000'000, cache);
    CHECK(offset.display_sample == next.display_sample - 9600);
    CHECK(offset.display_hold_samples == 0);
}
