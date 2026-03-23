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

std::optional<double> SpeedManager::scrollBps(double bpm) const noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0) {
        return std::nullopt;
    }
    double scroll = (bpm * hi_speed_) / rate_;
    if (!std::isfinite(scroll)) {
        return std::nullopt;
    }
    return scroll;
}

std::optional<double> SpeedManager::recommendHiSpeed(double bpm, double target_scroll_bps) const noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0) {
        return std::nullopt;
    }
    if (!std::isfinite(target_scroll_bps) || target_scroll_bps <= 0.0) {
        return std::nullopt;
    }
    double recommended = (target_scroll_bps * rate_) / bpm;
    if (!std::isfinite(recommended)) {
        return std::nullopt;
    }
    return recommended;
}

}  // namespace tenriff::game
