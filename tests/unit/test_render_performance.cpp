#include "doctest/doctest.h"

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

}  // namespace
