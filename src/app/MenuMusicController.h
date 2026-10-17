#pragma once

#include <chrono>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/AudioConfig.h"
#ifdef _WIN32
#include "audio/AudioThread.h"
#endif

namespace tenriff::app {

// Plays the same short click used by gameplay tuning with the supplied master
// gain. Its PCM storage outlives asynchronous Windows playback.
void play_settings_adjustment_click(double gain);

namespace menu_music_detail {

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
    std::chrono::steady_clock::time_point retry_allowed_at_{};
};

}  // namespace tenriff::app
