#pragma once

#include <optional>

namespace tenriff::game {

class SpeedManager {
public:
    SpeedManager() = default;

    [[nodiscard]] double rate() const noexcept { return rate_; }
    [[nodiscard]] double hiSpeed() const noexcept { return hi_speed_; }

    // Returns true if the value was accepted and applied.
    bool setRate(double rate);
    bool setHiSpeed(double hi_speed);

    // Judge windows shrink as playback rate increases.
    [[nodiscard]] double scaleJudgeWindow(double base_window_ms) const noexcept;

    // Effective scroll speed used for UI hints (BPM * HS / rate).
    [[nodiscard]] std::optional<double> scrollBps(double bpm) const noexcept;

    // Recommend a Hi-Speed multiplier to reach the desired scroll speed at the given BPM.
    [[nodiscard]] std::optional<double> recommendHiSpeed(double bpm, double target_scroll_bps) const noexcept;

private:
    double rate_ = 1.0;
    double hi_speed_ = 1.0;
};

}  // namespace tenriff::game

