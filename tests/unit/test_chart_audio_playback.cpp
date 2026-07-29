#include "doctest/doctest.h"

#include <vector>

#include "app/ChartAudioPlayback.h"

TEST_CASE("chart audio playback duration scales with rate") {
    CHECK(tenriff::app::chart_audio_playback_duration_frames(8, 1.0) == 8);
    CHECK(tenriff::app::chart_audio_playback_duration_frames(8, 2.0) == 4);
    CHECK(tenriff::app::chart_audio_playback_duration_frames(8, 0.5) == 16);
}

TEST_CASE("chart audio mix reads ahead when playback rate is faster") {
    const std::vector<float> clip = {
        1.0f, 10.0f,
        2.0f, 20.0f,
        3.0f, 30.0f,
        4.0f, 40.0f,
    };
    std::vector<float> output(4, 0.0f);

    const int64_t mixed = tenriff::app::mix_chart_audio_clip_linear(
        clip, 0, 2.0, 1.0f, output.data(), 2, 0);

    CHECK(mixed == 2);
    CHECK(output[0] == doctest::Approx(1.0f));
    CHECK(output[1] == doctest::Approx(10.0f));
    CHECK(output[2] == doctest::Approx(3.0f));
    CHECK(output[3] == doctest::Approx(30.0f));
}

TEST_CASE("chart audio mix interpolates when playback rate is slower") {
    const std::vector<float> clip = {
        1.0f, 10.0f,
        3.0f, 30.0f,
        5.0f, 50.0f,
    };
    std::vector<float> output(8, 0.0f);

    const int64_t mixed = tenriff::app::mix_chart_audio_clip_linear(
        clip, 0, 0.5, 1.0f, output.data(), 4, 0);

    CHECK(mixed == 4);
    CHECK(output[0] == doctest::Approx(1.0f));
    CHECK(output[1] == doctest::Approx(10.0f));
    CHECK(output[2] == doctest::Approx(2.0f));
    CHECK(output[3] == doctest::Approx(20.0f));
    CHECK(output[4] == doctest::Approx(3.0f));
    CHECK(output[5] == doctest::Approx(30.0f));
    CHECK(output[6] == doctest::Approx(4.0f));
    CHECK(output[7] == doctest::Approx(40.0f));
}

TEST_CASE("late realtime keysounds start at the writable buffer boundary") {
    const std::vector<float> short_clip = {
        1.0f, 1.0f,
        0.5f, 0.5f,
    };
    std::vector<float> output(4, 0.0f);

    const int64_t buffer_start = 1'000;
    const int64_t original_input_sample = 900;
    CHECK(tenriff::app::mix_chart_audio_clip_linear(
              short_clip, original_input_sample, 1.0, 1.0f,
              output.data(), 2, buffer_start) == 0);

    const int64_t audible_sample = tenriff::app::pin_realtime_audio_start_sample(
        original_input_sample, buffer_start);
    CHECK(audible_sample == buffer_start);
    CHECK(tenriff::app::mix_chart_audio_clip_linear(
              short_clip, audible_sample, 1.0, 1.0f,
              output.data(), 2, buffer_start) == 2);
    CHECK(output[0] == doctest::Approx(1.0f));
    CHECK(output[1] == doctest::Approx(1.0f));
}
