#include "doctest/doctest.h"

#include "audio/AudioConfig.h"
#include "input/InputEvent.h"
#include "input/SPSCQueue.h"
#include "timing/ClockSync.h"

TEST_CASE("SPSCQueue preserves order and drops when full") {
    tenriff::input::SPSCQueue<int, 4> queue;
    CHECK(queue.capacity() == 3);

    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK_FALSE(queue.push(4));

    auto first = queue.pop();
    CHECK(first.has_value());
    if (!first.has_value()) {
        return;
    }
    CHECK(*first == 1);

    auto second = queue.pop();
    CHECK(second.has_value());
    if (!second.has_value()) {
        return;
    }
    CHECK(*second == 2);

    CHECK(queue.push(4));
    auto third = queue.pop();
    CHECK(third.has_value());
    if (!third.has_value()) {
        return;
    }
    CHECK(*third == 3);
}

TEST_CASE("ClockSync estimates slope and anchored input-to-audio mapping") {
    tenriff::timing::ClockSyncConfig config{};
    config.max_samples = 4;
    tenriff::timing::ClockSync sync(config);
    sync.add_sample(0, 0);
    sync.add_sample(1'000'000'000, 48000);

    auto estimate = sync.input_to_audio_samples(500'000'000);
    CHECK(estimate.has_value());
    if (!estimate.has_value()) {
        return;
    }
    CHECK(*estimate == 24000);
}

TEST_CASE("ClockSync drops oldest samples when exceeding capacity") {
    tenriff::timing::ClockSyncConfig config{};
    config.max_samples = 2;
    tenriff::timing::ClockSync sync(config);
    sync.add_sample(0, 0);
    sync.add_sample(1'000'000'000, 48000);
    sync.add_sample(2'000'000'000, 96050);

    auto mapped = sync.input_to_audio_samples(1'500'000'000);
    CHECK(mapped.has_value());
    if (!mapped.has_value()) {
        return;
    }
    CHECK(*mapped > 70000);
}

TEST_CASE("ClockSync clamps regressions that would move backwards") {
    tenriff::timing::ClockSync sync;
    sync.add_sample(0, 0);
    sync.add_sample(1'000'000'000, 48000);

    auto first = sync.input_to_audio_samples(1'000'000'000);
    CHECK(first.has_value());
    if (!first.has_value()) {
        return;
    }
    CHECK(*first == 48000);

    // Even if a caller requests an older timestamp, the output should remain monotonic.
    auto second = sync.input_to_audio_samples(500'000'000);
    CHECK(second.has_value());
    if (!second.has_value()) {
        return;
    }
    CHECK(*second == *first);
}

TEST_CASE("ClockSync rejects outliers while keeping a stable fit") {
    tenriff::timing::ClockSyncConfig config{};
    config.max_samples = 8;
    config.ema_alpha = 0.5;
    config.mad_floor_samples = 8.0;
    config.min_outlier_samples = 3;
    tenriff::timing::ClockSync sync(config);

    for (int i = 0; i < 4; ++i) {
        sync.add_sample(static_cast<int64_t>(i) * 1'000'000'000LL, i * 48000);
    }

    // Outlier: wildly incorrect audio sample value for the given input time.
    sync.add_sample(4'000'000'000LL, 1'000'000);

    auto mapped = sync.input_to_audio_samples(4'000'000'000LL);
    CHECK(mapped.has_value());
    if (!mapped.has_value()) {
        return;
    }

    // With the outlier ignored, slope should stay near 48k samples/sec.
    CHECK(*mapped > 180000);
    CHECK(*mapped < 210000);
}

TEST_CASE("ClockSync reset clears accumulated state") {
    tenriff::timing::ClockSync sync;
    sync.add_sample(0, 0);
    sync.add_sample(1'000'000'000, 48000);
    CHECK(sync.has_estimate());

    sync.reset();
    CHECK_FALSE(sync.has_estimate());
    CHECK_FALSE(sync.input_to_audio_samples(500'000'000).has_value());
}

TEST_CASE("audio playback sample tracks the device head instead of the write cursor") {
    CHECK(tenriff::audio::playback_sample_from_write_cursor(4096, 512) == 3584);
    CHECK(tenriff::audio::playback_sample_from_write_cursor(256, 256) == 0);
    CHECK(tenriff::audio::playback_sample_from_write_cursor(128, 512) == 0);
}

TEST_CASE("ClockSync mapping follows playback head when non-zero padding exists") {
    constexpr int64_t input_time_ns = 1'000'000'000LL;
    constexpr int64_t write_cursor_samples = 4096;
    constexpr uint32_t queued_padding_frames = 512;
    const int64_t playback_sample =
        tenriff::audio::playback_sample_from_write_cursor(write_cursor_samples, queued_padding_frames);
    REQUIRE(playback_sample == 3584);

    tenriff::timing::ClockSync sync;
    sync.add_sample(input_time_ns, playback_sample);
    sync.add_sample(input_time_ns + 1'000'000'000LL, playback_sample + 48'000);

    auto mapped = sync.input_to_audio_samples(input_time_ns);
    REQUIRE(mapped.has_value());
    CHECK(*mapped == playback_sample);
    CHECK(*mapped != write_cursor_samples);
}

TEST_CASE("ClockSync stays stable with multi-day QPC timestamps") {
    constexpr int64_t qpc_base_ns = 4LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
    constexpr int64_t callback_step_ns = 2'902'494LL;
    constexpr int64_t callback_step_samples = 128;
    constexpr int sample_count = 512;

    tenriff::timing::ClockSync sync;
    for (int i = 0; i < sample_count; ++i) {
        sync.add_sample(qpc_base_ns + static_cast<int64_t>(i) * callback_step_ns,
                        static_cast<int64_t>(i) * callback_step_samples);
    }

    auto mapped = sync.input_to_audio_samples(
        qpc_base_ns + static_cast<int64_t>(sample_count - 1) * callback_step_ns);
    REQUIRE(mapped.has_value());

    const int64_t expected = static_cast<int64_t>(sample_count - 1) * callback_step_samples;
    CHECK(*mapped >= expected - 8);
    CHECK(*mapped <= expected + 8);
}

TEST_CASE("ClockSync recovers after a sustained audio clock discontinuity") {
    constexpr int64_t qpc_base_ns = 1'000'000'000LL;
    constexpr int64_t callback_step_ns = 10'000'000LL;
    constexpr int64_t callback_step_samples = 480;
    constexpr int64_t audio_rebase_samples = 24'000;

    tenriff::timing::ClockSync sync;
    for (int i = 0; i < 16; ++i) {
        sync.add_sample(qpc_base_ns + static_cast<int64_t>(i) * callback_step_ns,
                        static_cast<int64_t>(i) * callback_step_samples);
    }

    for (int i = 16; i < 48; ++i) {
        sync.add_sample(qpc_base_ns + static_cast<int64_t>(i) * callback_step_ns,
                        static_cast<int64_t>(i) * callback_step_samples + audio_rebase_samples);
    }

    auto mapped = sync.input_to_audio_samples(qpc_base_ns + 47LL * callback_step_ns);
    REQUIRE(mapped.has_value());

    const int64_t expected = 47LL * callback_step_samples + audio_rebase_samples;
    CHECK(*mapped >= expected - 8);
    CHECK(*mapped <= expected + 8);
}

TEST_CASE("ClockSync keeps its fit across separated one-shot outliers") {
    tenriff::timing::ClockSyncConfig config;
    config.rebase_after_consecutive_outliers = 2;
    tenriff::timing::ClockSync sync(config);

    constexpr int64_t qpc_base_ns = 4LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
    constexpr int64_t callback_step_ns = 10'000'000LL;
    constexpr int64_t callback_step_samples = 480;

    for (int i = 0; i < 8; ++i) {
        sync.add_sample(qpc_base_ns + static_cast<int64_t>(i) * callback_step_ns,
                        static_cast<int64_t>(i) * callback_step_samples);
    }

    for (int i = 8; i < 24; i += 2) {
        sync.add_sample(qpc_base_ns + static_cast<int64_t>(i) * callback_step_ns,
                        1'000'000 + static_cast<int64_t>(i) * callback_step_samples);
        sync.add_sample(qpc_base_ns + static_cast<int64_t>(i + 1) * callback_step_ns,
                        static_cast<int64_t>(i + 1) * callback_step_samples);
    }

    auto mapped = sync.input_to_audio_samples(qpc_base_ns + 23LL * callback_step_ns);
    REQUIRE(mapped.has_value());
    CHECK(*mapped >= 23LL * callback_step_samples - 8);
    CHECK(*mapped <= 23LL * callback_step_samples + 8);
}
