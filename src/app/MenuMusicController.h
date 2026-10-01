#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace tenriff::app {

// Plays the same short click used by gameplay tuning with the supplied master
// gain. Its PCM storage outlives asynchronous Windows playback.
void play_settings_adjustment_click(double gain);

namespace menu_music_detail {

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

    void play_looping_file(const std::string& path, double gain);
    // Focus mute keeps the current file and its playback cursor alive.
    void set_output_muted(bool muted);
    void stop();

private:
    void close_locked();
    void apply_gain_locked();

    std::mutex mutex_;
    std::string requested_path_;
    std::string current_path_;
    double gain_ = 1.0;
    bool output_muted_ = false;
    bool open_ = false;
    bool open_failed_ = false;
    std::chrono::steady_clock::time_point retry_allowed_at_{};
};

}  // namespace tenriff::app
