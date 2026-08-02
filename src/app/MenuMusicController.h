#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace tenriff::app {

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
    void stop();

private:
    void close_locked();
    void apply_gain_locked();

    std::mutex mutex_;
    std::string requested_path_;
    std::string current_path_;
    double gain_ = 1.0;
    bool open_ = false;
    bool open_failed_ = false;
    std::chrono::steady_clock::time_point retry_allowed_at_{};
};

}  // namespace tenriff::app
