#include "audio/MixNormalizer.h"
#include "doctest/doctest.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "app/ChartAudioPlayback.h"
#include "app/AudioFileDecoder.h"
#include "app/AudioMixPolicy.h"
#include "app/SongPreviewBuilder.h"
#include "app/SongPreviewPlayback.h"

namespace {

struct PreviewTempDir {
    std::filesystem::path path;

    ~PreviewTempDir() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

std::filesystem::path make_preview_temp_dir() {
    const auto base =
        std::filesystem::temp_directory_path() / "tenriff_song_preview_tests";
    std::filesystem::create_directories(base);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto candidate = base / ("case_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

void write_preview_test_wav(const std::filesystem::path& path, int sample_rate) {
    constexpr std::uint32_t kSampleCount = 256;
    constexpr std::uint32_t kDataBytes = kSampleCount * 2u;
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());

    const auto write_u16 = [&out](std::uint16_t value) {
        const char bytes[2] = {
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu),
        };
        out.write(bytes, 2);
    };
    const auto write_u32 = [&out](std::uint32_t value) {
        const char bytes[4] = {
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu),
            static_cast<char>((value >> 16u) & 0xffu),
            static_cast<char>((value >> 24u) & 0xffu),
        };
        out.write(bytes, 4);
    };

    out.write("RIFF", 4);
    write_u32(36u + kDataBytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(16u);
    write_u16(1u);
    write_u16(1u);
    write_u32(static_cast<std::uint32_t>(sample_rate));
    write_u32(static_cast<std::uint32_t>(sample_rate * 2));
    write_u16(2u);
    write_u16(16u);
    out.write("data", 4);
    write_u32(kDataBytes);
    for (std::uint32_t sample = 0; sample < kSampleCount; ++sample) {
        const std::int16_t value = sample % 2u == 0u ? 12'000 : -12'000;
        write_u16(static_cast<std::uint16_t>(value));
    }
}

}  // namespace

TEST_CASE("chart audio playback duration scales with rate") {
    CHECK(tenriff::app::chart_audio_playback_duration_frames(8, 1.0) == 8);
    CHECK(tenriff::app::chart_audio_playback_duration_frames(8, 2.0) == 4);
    CHECK(tenriff::app::chart_audio_playback_duration_frames(8, 0.5) == 16);
}

TEST_CASE("chart sound offset shifts scheduled audio without moving chart time") {
    CHECK(tenriff::app::chart_audio_start_sample_with_offset(1'000, 125.0, 1'000) == 1'125);
    CHECK(tenriff::app::chart_audio_start_sample_with_offset(1'000, -250.0, 1'000) == 750);
    CHECK(tenriff::app::chart_audio_start_sample_with_offset(1'000, 125.0, 0) == 1'000);

    const std::vector<float> clip = {
        1.0f, 10.0f,
        2.0f, 20.0f,
        3.0f, 30.0f,
        4.0f, 40.0f,
    };
    std::vector<float> delayed_output(8, 0.0f);
    const int64_t delayed_start =
        tenriff::app::chart_audio_start_sample_with_offset(0, 2.0, 1'000);
    CHECK(tenriff::app::mix_chart_audio_clip_linear(
              clip, delayed_start, 1.0, 1.0f, delayed_output.data(), 4, 0) == 2);
    CHECK(delayed_output[0] == doctest::Approx(0.0f));
    CHECK(delayed_output[4] == doctest::Approx(1.0f));

    std::vector<float> advanced_output(4, 0.0f);
    const int64_t advanced_start =
        tenriff::app::chart_audio_start_sample_with_offset(0, -2.0, 1'000);
    CHECK(tenriff::app::mix_chart_audio_clip_linear(
              clip, advanced_start, 1.0, 1.0f, advanced_output.data(), 2, 0) == 2);
    CHECK(advanced_output[0] == doctest::Approx(3.0f));
    CHECK(advanced_output[1] == doctest::Approx(30.0f));
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

TEST_CASE("master volume remains linear after the mix soft limiter") {
    const float limited = tenriff::app::apply_master_volume_to_sample(2.0f, 1.0);
    CHECK(tenriff::app::apply_master_volume_to_sample(2.0f, 0.5) ==
          doctest::Approx(limited * 0.5f));
    CHECK(tenriff::app::apply_master_volume_to_sample(2.0f, 0.0) ==
          doctest::Approx(0.0f));
}

TEST_CASE("background sound toggle only gates non-game background audio") {
    CHECK(tenriff::app::menu_background_gain(false, 1.0, 0.75) ==
          doctest::Approx(0.0));
    CHECK(tenriff::app::menu_background_gain(true, 0.5, 2.0) ==
          doctest::Approx(0.5));
    CHECK(tenriff::app::gameplay_bgm_gain(0.75) == doctest::Approx(0.75f));
}

TEST_CASE("song preview PCM mixer loops and follows gain") {
    const std::vector<float> clip = {
        1.0f, -1.0f,
        0.5f, -0.5f,
    };
    std::vector<float> output(6, 0.0f);
    std::size_t cursor = 0;

    tenriff::app::mix_looping_song_preview(
        clip, cursor, 0.5f, output.data(), 3);

    CHECK(output[0] == doctest::Approx(0.5f));
    CHECK(output[1] == doctest::Approx(-0.5f));
    CHECK(output[2] == doctest::Approx(0.25f));
    CHECK(output[3] == doctest::Approx(-0.25f));
    CHECK(output[4] == doctest::Approx(0.5f));
    CHECK(output[5] == doctest::Approx(-0.5f));
    CHECK(cursor == 1u);
}

TEST_CASE("song preview chart mixer places and trims event clips in its window") {
    const std::vector<float> clip = {
        1.0f, 10.0f,
        2.0f, 20.0f,
        3.0f, 30.0f,
    };
    std::vector<float> window(8, 0.0f);

    CHECK(tenriff::app::mix_song_preview_clip_into_window(
              clip, 8, 10, window) == 1u);
    CHECK(window[0] == doctest::Approx(3.0f));
    CHECK(window[1] == doctest::Approx(30.0f));

    CHECK(tenriff::app::mix_song_preview_clip_into_window(
              clip, 12, 10, window) == 2u);
    CHECK(window[4] == doctest::Approx(1.0f));
    CHECK(window[5] == doctest::Approx(10.0f));
    CHECK(window[6] == doctest::Approx(2.0f));
    CHECK(window[7] == doctest::Approx(20.0f));
}

#if defined(_WIN32)
TEST_CASE("song preview builder decodes and renders fragmented BMS audio events") {
    PreviewTempDir temp;
    temp.path = make_preview_temp_dir();
    REQUIRE_FALSE(temp.path.empty());

    const auto chart_path = temp.path / "preview_mix.bms";
    const auto wav_path = temp.path / "tone.wav";
    write_preview_test_wav(wav_path, 44'100);
    std::vector<float> limited_decode;
    std::string limited_decode_error;
    REQUIRE(tenriff::app::decode_audio_file_stereo_resampled(
        wav_path.u8string(), 44'100, limited_decode, &limited_decode_error, 64));
    CHECK(limited_decode_error.empty());
    CHECK(limited_decode.size() == 128u);
    {
        std::ofstream chart(chart_path, std::ios::binary);
        REQUIRE(chart.good());
        chart << "#PLAYER 1\n"
                 "#TITLE Preview Mix\n"
                 "#BPM 120\n"
                 "#WAV01 tone.wav\n"
                 "#00001:01\n";
    }

    std::vector<float> samples;
    std::string source;
    std::string error;
    REQUIRE(tenriff::app::build_song_preview_audio(
        chart_path.u8string(), {}, 44'100, 5, samples, source, &error));
    CHECK(error.empty());
    CHECK(source == chart_path.u8string() + "#autoplay-preview");
    CHECK_FALSE(samples.empty());
    CHECK(std::any_of(samples.begin(), samples.end(), [](float sample) {
        return sample != 0.0f;
    }));

    auto cancel_flag = std::make_shared<std::atomic<bool>>(true);
    samples.assign(2u, 1.0f);
    source = "stale";
    error.clear();
    CHECK_FALSE(tenriff::app::build_song_preview_audio(
        chart_path.u8string(), {}, 44'100, 5, samples, source, &error, cancel_flag));
    CHECK(samples.empty());
    CHECK(source.empty());
    CHECK(error == "cancelled");
}
#endif

TEST_CASE("mix normalization preserves stereo balance and does not depend on buffer boundaries") {
    std::vector<float> whole(48000 * 2 * 3);
    for (std::size_t i = 0; i < whole.size(); i += 2) { whole[i] = 0.8f; whole[i + 1] = 0.4f; }
    auto chunks = whole;
    tenriff::audio::MixNormalizer a, b;
    a.reset(48000); b.reset(48000);
    a.process(whole.data(), 48000 * 3);
    for (std::size_t i = 0; i < chunks.size(); i += 128) b.process(chunks.data() + i, 64);
    CHECK(whole == chunks);
    CHECK(whole.back() < 0.4f);
    CHECK(whole[whole.size() - 2] == doctest::Approx(whole.back() * 2));
    std::vector<float> silence(1024, 0.0f);
    a.process(silence.data(), 512);
    for (float sample : silence) CHECK(sample == 0.0f);
    a.reset(48000);
    float reset_frame[]{0.1f, 0.1f};
    a.process(reset_frame, 1);
    CHECK(reset_frame[0] == doctest::Approx(0.1f).epsilon(0.001));
}
