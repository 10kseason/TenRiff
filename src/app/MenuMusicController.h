#pragma once

#include <chrono>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <cstdint>
#include <string_view>

#include "audio/AudioConfig.h"
#ifdef _WIN32
#include "audio/AudioThread.h"
#endif

namespace tenriff::app {

enum class MenuSoundKind : std::uint8_t { Button, Slider, Drop, OptionsEdit, Start, Done };
inline constexpr std::size_t kMenuSoundKindCount = 6;

// Plays the same short click used by gameplay tuning with the supplied master
// gain. Its PCM storage outlives asynchronous Windows playback.
void play_settings_adjustment_click(double gain);

namespace menu_music_detail {

[[nodiscard]] bool supported_audio_file(std::string_view path);
[[nodiscard]] std::string_view bundled_sound_filename(MenuSoundKind kind);
[[nodiscard]] std::string resolve_bundled_sound(MenuSoundKind kind);
[[nodiscard]] constexpr std::size_t sound_max_frames(MenuSoundKind kind) {
    return 48'000 * (kind == MenuSoundKind::Done ? 4 : 2);
}
// Start intent is emitted by the validated manual-start boundary; suppress its
// generic pointer cue here so one activation never plays two different sounds.
[[nodiscard]] constexpr std::optional<MenuSoundKind> pointer_sound(bool activation, bool slider,
                                                                 bool options, bool start_intent) {
    if (start_intent) return std::nullopt;
    if (slider) return MenuSoundKind::Slider;
    if (activation) return options ? MenuSoundKind::OptionsEdit : MenuSoundKind::Button;
    return std::nullopt;
}
// Short UI clips are capped before encoding, including files selected by mistake.
[[nodiscard]] std::vector<unsigned char> sound_wave(const std::vector<float>& pcm, double gain,
                                                  std::size_t max_frames = 48'000 * 2);

// Scene music and BMS previews use the selected endpoint without monopolizing
// desktop audio. ASIO still uses its selected driver and hardware rate.
[[nodiscard]] inline audio::AudioConfig output_config(audio::AudioConfig config) {
    config.exclusive_mode = false;
    return config;
}

[[nodiscard]] inline bool same_output(const audio::AudioConfig& a, const audio::AudioConfig& b) {
    return a.backend == b.backend && a.device_id == b.device_id && a.asio_driver == b.asio_driver &&
        a.sample_rate == b.sample_rate && a.frames_per_buffer == b.frames_per_buffer &&
        a.periods == b.periods && a.exclusive_mode == b.exclusive_mode &&
        a.use_mmcss == b.use_mmcss && a.affinity_core == b.affinity_core;
}

enum class PlaybackAction {
    Close,
    Open,
    UpdateGain,
};

[[nodiscard]] constexpr PlaybackAction playback_action(bool open,
                                                       bool requested_path_matches,
                                                       double gain) noexcept {
    if (!(gain > 0.0)) {
        return PlaybackAction::Close;
    }
    return open && requested_path_matches ? PlaybackAction::UpdateGain : PlaybackAction::Open;
}

}  // namespace menu_music_detail

// Latest request wins. A single bounded decoder and six cached clips avoid
// spawning work for every slider tick; all playback storage stays owned here.
// Like the pre-existing settings click, these UI cues use Windows system output.
class MenuSoundController {
public:
    ~MenuSoundController();
    void request(MenuSoundKind kind, const std::string& token, double gain);
    void service(bool muted);
    void invalidate(MenuSoundKind kind);
    void preload(MenuSoundKind kind, const std::string& path);
    [[nodiscard]] std::uint64_t request_serial() const { return request_serial_; }
    [[nodiscard]] std::string take_error();
private:
    friend struct MenuSoundControllerTestAccess;
    struct Clip { std::string token; std::vector<float> pcm; std::string error; };
    struct Request { MenuSoundKind kind; std::string token; double gain; };
    struct Decoded { MenuSoundKind kind; Clip clip; std::uint64_t generation = 0; };
    void play(const Clip& clip, double gain, MenuSoundKind kind);
    void decode(MenuSoundKind kind, const std::string& token);
    std::array<Clip, kMenuSoundKindCount> clips_{};
    std::array<std::uint64_t, kMenuSoundKindCount> generations_{};
    std::array<std::uint64_t, kMenuSoundKindCount> request_counts_{};
    std::uint64_t request_serial_ = 0;
    std::optional<Request> pending_;
    std::future<Decoded> decoding_;
    std::vector<unsigned char> wave_;
    std::string error_;
    std::chrono::steady_clock::time_point last_request_{};
    MenuSoundKind last_kind_ = MenuSoundKind::Button;
    bool muted_ = false;
};

class MenuMusicController {
public:
    MenuMusicController() = default;
    ~MenuMusicController();

    MenuMusicController(const MenuMusicController&) = delete;
    MenuMusicController& operator=(const MenuMusicController&) = delete;

    void play_looping_file(const std::string& path, double gain, const audio::AudioConfig& config);
    // Focus mute keeps the current file and its playback cursor alive.
    void set_output_muted(bool muted);
    void stop();
    [[nodiscard]] bool decode_failed() const { return decode_failed_; }

private:
    friend struct MenuMusicControllerTestAccess;
    struct DecodedTrack {
        std::string path;
        uint32_t sample_rate = 0;
        std::shared_ptr<const std::vector<float>> samples;
        std::string error;
    };
    void close_locked();
    void apply_gain_locked();

    std::mutex mutex_;
    std::string requested_path_;
    std::string current_path_;
    audio::AudioConfig requested_config_{};
    std::future<DecodedTrack> decoding_;
    std::shared_ptr<const std::vector<float>> samples_;
    std::atomic<float> output_gain_{0.0f};
#ifdef _WIN32
    audio::AudioThread audio_;
#endif
    double gain_ = 1.0;
    bool output_muted_ = false;
    bool open_ = false;
    bool open_failed_ = false;
    bool decode_failed_ = false;
    std::chrono::steady_clock::time_point retry_allowed_at_{};
};

}  // namespace tenriff::app
