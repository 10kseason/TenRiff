#include "game/SpeedManager.h"

#include <algorithm>
#include <cmath>

namespace tenriff::game {

namespace {
constexpr double kEpsilon = 1e-9;
}

bool SpeedManager::setRate(double rate) {
    if (!std::isfinite(rate) || rate <= 0.0) {
        return false;
    }
    rate_ = rate;
    return true;
}

bool SpeedManager::setHiSpeed(double hi_speed) {
    if (!std::isfinite(hi_speed) || hi_speed <= 0.0) {
        return false;
    }
    hi_speed_ = hi_speed;
    return true;
}

double SpeedManager::scaleJudgeWindow(double base_window_ms) const noexcept {
    static_cast<void>(rate_);
    return base_window_ms;
}

std::optional<double> SpeedManager::visualScrollScale(double rate, double hi_speed) noexcept {
    if (!std::isfinite(rate) || rate <= 0.0 || !std::isfinite(hi_speed) || hi_speed <= 0.0) {
        return std::nullopt;
    }

    // Chart and audio timestamps are already divided by Rate. Applying Rate again here made
    // slower practice playback scroll faster, so the visual scale intentionally remains HS-only.
    return hi_speed;
}

std::optional<double> SpeedManager::scrollBps(double bpm, double rate, double hi_speed) noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0) {
        return std::nullopt;
    }
    const auto scale = visualScrollScale(rate, hi_speed);
    if (!scale.has_value()) {
        return std::nullopt;
    }
    const double scroll = bpm * scale.value();
    if (!std::isfinite(scroll)) {
        return std::nullopt;
    }
    return scroll;
}

std::optional<double> SpeedManager::scrollBps(double bpm) const noexcept {
    return scrollBps(bpm, rate_, hi_speed_);
}

std::optional<double> SpeedManager::recommendHiSpeed(double bpm, double target_scroll_bps) const noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0) {
        return std::nullopt;
    }
    if (!std::isfinite(target_scroll_bps) || target_scroll_bps <= 0.0) {
        return std::nullopt;
    }
    const double recommended = target_scroll_bps / bpm;
    if (!std::isfinite(recommended)) {
        return std::nullopt;
    }
    return recommended;
}

}  // namespace tenriff::game
