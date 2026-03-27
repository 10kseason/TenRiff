#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace tenriff::app {

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
