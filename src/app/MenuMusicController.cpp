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
#include <utility>

#include "util/Utf8Compat.h"

namespace tenriff::app {

bool menu_music_detail::supported_audio_file(std::string_view path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos) return false;
    std::string extension(path.substr(dot));
    for (char& value : extension) if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
    return extension == ".mp3" || extension == ".ogg" || extension == ".wav" || extension == ".flac";
}

std::string_view menu_music_detail::bundled_sound_filename(MenuSoundKind kind) {
    switch (kind) {
        case MenuSoundKind::Button: return "SFX/button click.mp3";
        case MenuSoundKind::Slider: return "SFX/Silder Moving.mp3";
        case MenuSoundKind::OptionsEdit: return "SFX/options edit.mp3";
        case MenuSoundKind::Start: return "SFX/Start button click.mp3";
        case MenuSoundKind::Done: return "SFX/done.mp3";
        case MenuSoundKind::Drop: return {};
    }
    return {};
}

std::string menu_music_detail::resolve_bundled_sound(MenuSoundKind kind) {
#ifdef _WIN32
    // Deliberately ignore the working directory. A shortcut or file dialog may
    // change it, but the portable installation remains rooted at its executable.
    static const auto paths = [] {
        std::array<std::string, kMenuSoundKindCount> result{};
        std::array<wchar_t, 32'768> filename{};
        const DWORD count = GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
        if (!count || count >= filename.size()) return result;
        const auto executable = std::filesystem::path(filename.data()).parent_path();
        for (std::size_t index = 0; index < result.size(); ++index) {
            const auto relative = bundled_sound_filename(static_cast<MenuSoundKind>(index));
            if (relative.empty()) continue;
            auto root = executable;
            // Parent fallbacks support the existing source/build layout only;
            // no external machine path is compiled or written into a profile.
            for (int depth = 0; depth < 3; ++depth, root = root.parent_path()) {
                const auto candidate = root / "Mainmusic" / std::filesystem::u8path(relative);
                std::error_code error;
                if (std::filesystem::is_regular_file(candidate, error)) {
                    result[index] = candidate.u8string();
                    break;
                }
            }
            if (result[index].empty()) result[index] =
                (executable / "Mainmusic" / std::filesystem::u8path(relative)).u8string();
        }
        return result;
    }();
    const auto index = static_cast<std::size_t>(kind);
    return index < paths.size() ? paths[index] : std::string{};
#else
    static_cast<void>(kind);
    return {};
#endif
}

std::vector<unsigned char> menu_music_detail::sound_wave(const std::vector<float>& pcm, double gain,
                                                        std::size_t max_frames) {
    const std::size_t max_samples = std::min<std::size_t>(max_frames, 48'000 * 4) * 2;
    const std::size_t samples = std::min(pcm.size() / 2 * 2, max_samples);
    const std::size_t bytes = samples * sizeof(std::int16_t);
    std::vector<unsigned char> wave(44 + bytes);
    const auto put = [&](std::size_t offset, std::uint32_t value, int count) {
        for (int byte = 0; byte < count; ++byte)
            wave[offset + byte] = static_cast<unsigned char>(value >> (byte * 8));
    };
    const auto tag = [&](std::size_t offset, const char* value) {
        std::copy_n(value, 4, wave.begin() + offset);
    };
    tag(0, "RIFF"); put(4, static_cast<std::uint32_t>(36 + bytes), 4); tag(8, "WAVE");
    tag(12, "fmt "); put(16, 16, 4); put(20, 1, 2); put(22, 2, 2);
    put(24, 48'000, 4); put(28, 48'000 * 4, 4); put(32, 4, 2); put(34, 16, 2);
    tag(36, "data"); put(40, static_cast<std::uint32_t>(bytes), 4);
    const double safe_gain = std::isfinite(gain) ? std::clamp(gain, 0.0, 1.0) : 0.0;
    for (std::size_t index = 0; index < samples; ++index) {
        const double value = std::isfinite(pcm[index]) ? std::clamp(pcm[index] * safe_gain, -1.0, 1.0) : 0.0;
        const auto sample = static_cast<std::int16_t>(std::lround(value * 32767.0));
        put(44 + index * 2, static_cast<std::uint16_t>(sample), 2);
    }
    return wave;
}

MenuSoundController::~MenuSoundController() {
#ifdef _WIN32
    if (!wave_.empty()) PlaySoundW(nullptr, nullptr, 0);
#endif
}

void MenuSoundController::request(MenuSoundKind kind, const std::string& token, double gain) {
    ++request_counts_[static_cast<std::size_t>(kind)];
    ++request_serial_;
    const auto now = std::chrono::steady_clock::now();
    if (muted_ || token == "none" || !(gain > 0.0)) {
        if (pending_ && pending_->kind == kind) pending_.reset();
        return;
    }
    const auto priority = [](MenuSoundKind value) {
        return value == MenuSoundKind::Start ? 3 : value == MenuSoundKind::Done ? 2 :
            value == MenuSoundKind::Drop ? 1 : 0;
    };
    // A cold completion/start decode must not disappear merely because the
    // user moves a slider while it finishes; equal-priority requests stay latest-only.
    if (pending_ && priority(pending_->kind) > priority(kind)) return;
    // Slider events can arrive at mouse-poll frequency. One audible cue per
    // 60 ms is enough; this limit also bounds expensive malformed-file requests.
    if (kind == last_kind_ && now - last_request_ <
        std::chrono::milliseconds(kind == MenuSoundKind::Slider ? 60 : 30)) return;
    last_request_ = now;
    last_kind_ = kind;
    if (token.empty()) {
        pending_.reset();
        play_settings_adjustment_click(gain);
        return;
    }
    pending_ = Request{kind, token, gain};
    service(false);
}

void MenuSoundController::service(bool muted) {
    if (muted && !muted_) {
#ifdef _WIN32
        PlaySoundW(nullptr, nullptr, 0);
#endif
        pending_.reset();
    }
    muted_ = muted;
    if (decoding_.valid()) {
        if (decoding_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            auto decoded = decoding_.get();
            const auto index = static_cast<std::size_t>(decoded.kind);
            if (decoded.generation == generations_[index]) clips_[index] = std::move(decoded.clip);
        } catch (const std::exception&) {
            error_ = "Selected UI sound could not be decoded.";
            pending_.reset();
        }
    }
    if (!pending_ || muted_) return;
    const auto request = *pending_;
    const auto& cached = clips_[static_cast<std::size_t>(request.kind)];
    if (cached.token == request.token) {
        if (cached.error.empty()) play(cached, request.gain, request.kind);
        else error_ = "Selected UI sound could not be decoded.";
        pending_.reset();
        return;
    }
    decode(request.kind, request.token);
}

void MenuSoundController::preload(MenuSoundKind kind, const std::string& path) {
    if (path.empty() || path == "none" || decoding_.valid() || pending_ ||
        clips_[static_cast<std::size_t>(kind)].token == path) return;
    decode(kind, path);
}

void MenuSoundController::decode(MenuSoundKind kind, const std::string& token) {
    decoding_ = std::async(std::launch::async, [kind, token,
                                              generation = generations_[static_cast<std::size_t>(kind)]] {
        Decoded decoded{kind, Clip{token}, generation};
        try {
            if (!decode_audio_file_stereo_resampled(token, 48'000, decoded.clip.pcm,
                                                     &decoded.clip.error, menu_music_detail::sound_max_frames(kind)) || decoded.clip.pcm.empty()) {
                decoded.clip.pcm.clear();
                if (decoded.clip.error.empty()) decoded.clip.error = "No audio samples";
            }
        } catch (const std::exception& error) { decoded.clip.error = error.what(); }
        return decoded;
    });
}

void MenuSoundController::invalidate(MenuSoundKind kind) {
    const auto index = static_cast<std::size_t>(kind);
    ++generations_[index];
    clips_[index] = {};
    if (pending_ && pending_->kind == kind) pending_.reset();
#ifdef _WIN32
    if (!wave_.empty()) PlaySoundW(nullptr, nullptr, 0);
#endif
}

void MenuSoundController::play(const Clip& clip, double gain, MenuSoundKind kind) {
#ifdef _WIN32
    PlaySoundW(nullptr, nullptr, 0);
    wave_ = menu_music_detail::sound_wave(clip.pcm, gain, menu_music_detail::sound_max_frames(kind));
    if (wave_.size() > 44)
        PlaySoundW(reinterpret_cast<const wchar_t*>(wave_.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    static_cast<void>(clip); static_cast<void>(gain); static_cast<void>(kind);
#endif
}

std::string MenuSoundController::take_error() { return std::exchange(error_, {}); }

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
        decode_failed_ = false;
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
    if (decode_failed_) return;
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
                decode_failed_ = true;
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
