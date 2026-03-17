#pragma once

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
};

}  // namespace tenriff::app
