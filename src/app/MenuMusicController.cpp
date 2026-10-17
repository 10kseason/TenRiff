#include "app/MenuMusicController.h"
#include "audio/SettingsAdjustmentClick.h"
#include "app/AudioFileDecoder.h"
#include "app/SongPreviewPlayback.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "util/Utf8Compat.h"

namespace tenriff::app {

void play_settings_adjustment_click(double gain) {
#ifdef _WIN32
    const double safe_gain = std::clamp(gain, 0.0, 1.0);
    if (!(safe_gain > 0.0)) return;
    constexpr int sample_rate = 48000;
    constexpr std::size_t frames = sample_rate * audio::SettingsAdjustmentClick::kDurationMs / 1000;
    constexpr std::size_t data_bytes = frames * 2 * sizeof(std::int16_t);
    static std::array<unsigned char, 44 + data_bytes> wav{};
    static std::mutex click_mutex;
    static double rendered_gain = -1.0;
    std::lock_guard<std::mutex> lock(click_mutex);
    if (rendered_gain != safe_gain) {
        // Stop the previous asynchronous read before replacing its PCM storage.
        PlaySoundW(nullptr, nullptr, 0);
        const auto put = [&](std::size_t offset, std::uint32_t value, int bytes) {
            for (int byte = 0; byte < bytes; ++byte)
                wav[offset + byte] = static_cast<unsigned char>(value >> (byte * 8));
        };
        const auto tag = [&](std::size_t offset, const char* value) {
            for (std::size_t byte = 0; byte < 4; ++byte) wav[offset + byte] = value[byte];
        };
        tag(0, "RIFF"); put(4, 36 + data_bytes, 4); tag(8, "WAVE");
        tag(12, "fmt "); put(16, 16, 4); put(20, 1, 2); put(22, 2, 2);
        put(24, sample_rate, 4); put(28, sample_rate * 4, 4); put(32, 4, 2); put(34, 16, 2);
        tag(36, "data"); put(40, data_bytes, 4);
        std::array<float, frames * 2> samples{};
        audio::SettingsAdjustmentClick click;
        click.trigger(sample_rate);
        click.mix(samples.data(), static_cast<std::uint32_t>(frames));
        for (std::size_t sample = 0; sample < samples.size(); ++sample) {
            const auto pcm = static_cast<std::int16_t>(std::lround(samples[sample] * safe_gain * 32767.0));
            put(44 + sample * 2, static_cast<std::uint16_t>(pcm), 2);
        }
        rendered_gain = safe_gain;
    }
    PlaySoundW(reinterpret_cast<const wchar_t*>(wav.data()), nullptr,
               SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    static_cast<void>(gain);
#endif
}

MenuMusicController::~MenuMusicController() {
    stop();
}

void MenuMusicController::play_looping_file(const std::string& path, double gain,
                                              const audio::AudioConfig& config) {
#ifdef _WIN32
    const double clamped_gain = std::isfinite(gain) ? std::clamp(gain, 0.0, 1.0) : 0.0;
    const auto desired = menu_music_detail::output_config(config);
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    const bool changed = requested_path_ != path ||
        !menu_music_detail::same_output(requested_config_, desired);
    if (changed) {
        close_locked();
        requested_path_ = path;
        requested_config_ = desired;
        open_failed_ = false;
        retry_allowed_at_ = {};
    }
    gain_ = clamped_gain;
    if (path.empty() || !(gain_ > 0.0)) {
        close_locked();
        open_failed_ = false;
        retry_allowed_at_ = {};
        return;
    }
    apply_gain_locked();
    if (open_ && audio_.is_running() && !audio_.has_runtime_error()) return;
    if (open_) close_locked();
    if (open_failed_ && now < retry_allowed_at_) return;

    // Decode off the menu and audio threads. An obsolete decode may finish,
    // but only the current path/rate can be opened, and only from a menu tick.
    // stop() releases the device immediately without joining an in-flight decode.
    if (decoding_.valid()) {
        if (decoding_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        DecodedTrack decoded;
        try { decoded = decoding_.get(); }
        catch (const std::exception& error) { decoded.error = error.what(); }
        if (decoded.path == path && decoded.sample_rate == desired.sample_rate) {
            samples_ = std::move(decoded.samples);
            if (!samples_) {
                std::cerr << "[warn] Failed to decode scene music: " << decoded.error << std::endl;
                open_failed_ = true;
                retry_allowed_at_ = now + std::chrono::seconds(1);
                return;
            }
        }
    }
    if (!samples_) {
        decoding_ = std::async(std::launch::async, [path, rate = desired.sample_rate] {
            DecodedTrack result;
            result.path = path;
            result.sample_rate = rate;
            auto samples = std::make_shared<std::vector<float>>();
            try {
                if (decode_audio_file_stereo_resampled(path, static_cast<int>(rate), *samples,
                                                       &result.error) && !samples->empty())
                    result.samples = std::move(samples);
            } catch (const std::exception& error) { result.error = error.what(); }
            return result;
        });
        return;
    }
    const auto pcm = samples_;
    auto result = audio_.initialize(desired,
        [this, pcm, cursor = std::size_t{0}](float* output, uint32_t frames, int64_t, int64_t) mutable {
            mix_looping_song_preview(*pcm, cursor, output_gain_.load(std::memory_order_acquire),
                                     output, frames);
        });
    if (result == audio::AudioResult::Success) result = audio_.start();
    if (result != audio::AudioResult::Success) {
        // A missing selected endpoint must not redirect music to another device.
        std::cerr << "[warn] Failed to open scene music output (result=" << static_cast<int>(result)
                  << "): " << audio_.error_message() << std::endl;
        close_locked();
        open_failed_ = true;
        retry_allowed_at_ = now + std::chrono::seconds(1);
        return;
    }
    current_path_ = path;
    open_ = true;
    open_failed_ = false;
#else
    static_cast<void>(path); static_cast<void>(gain); static_cast<void>(config);
#endif
}

void MenuMusicController::set_output_muted(bool muted) {
    std::lock_guard<std::mutex> lock(mutex_);
    output_muted_ = muted;
    apply_gain_locked();
}

void MenuMusicController::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    close_locked();
    requested_path_.clear();
    open_failed_ = false;
    retry_allowed_at_ = {};
}

void MenuMusicController::close_locked() {
#ifdef _WIN32
    // Stop callbacks before releasing their PCM or letting another owner open ASIO.
    audio_.shutdown();
#endif
    samples_.reset();
    open_ = false;
    current_path_.clear();
}

void MenuMusicController::apply_gain_locked() {
    output_gain_.store(output_muted_ ? 0.0f : static_cast<float>(gain_), std::memory_order_release);
}

}  // namespace tenriff::app
