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

    // Judge windows stay in real playback milliseconds; chart/audio scheduling already encodes the rate change.
    [[nodiscard]] double scaleJudgeWindow(double base_window_ms) const noexcept;

    // Visual scrolling is owned by Hi-Speed. Rate already changes chart/audio scheduling.
    [[nodiscard]] static std::optional<double> visualScrollScale(double rate, double hi_speed) noexcept;
    [[nodiscard]] static std::optional<double> scrollBps(double bpm,
                                                         double rate,
                                                         double hi_speed) noexcept;

    // Effective scroll speed used for UI hints (BPM * HS).
    [[nodiscard]] std::optional<double> scrollBps(double bpm) const noexcept;

    // Recommend a Hi-Speed multiplier to reach the desired scroll speed at the given BPM.
    // The recommendation stays stable when playback Rate changes.
    [[nodiscard]] std::optional<double> recommendHiSpeed(double bpm, double target_scroll_bps) const noexcept;

private:
    double rate_ = 1.0;
    double hi_speed_ = 1.0;
};

}  // namespace tenriff::game
