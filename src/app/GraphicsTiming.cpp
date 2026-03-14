#include "app/GraphicsTiming.h"

#include <algorithm>
#include <cstdint>

namespace tenriff::app {

int clamp_graphics_refresh_hz(int value) {
    return std::clamp(value, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
}

int effective_configured_refresh_hz(int configured_refresh_hz, bool gameplay_active) {
    const int configured = clamp_graphics_refresh_hz(configured_refresh_hz);
    if (gameplay_active) {
        return configured;
    }
    return std::min(configured, kGraphicsMenuRefreshHzCap);
}

int effective_present_refresh_hz(bool vsync_enabled, int configured_refresh_hz,
                                 int detected_monitor_refresh_hz, bool gameplay_active) {
    if (!vsync_enabled) {
        return effective_configured_refresh_hz(configured_refresh_hz, gameplay_active);
    }
    return clamp_graphics_refresh_hz(detected_monitor_refresh_hz);
}

int effective_render_fps_limit(bool vsync_enabled, int configured_refresh_hz,
                               int detected_monitor_refresh_hz, bool gameplay_active) {
    if (!vsync_enabled) {
        return effective_configured_refresh_hz(configured_refresh_hz, gameplay_active);
    }

    const int present_refresh_hz = effective_present_refresh_hz(
        true, configured_refresh_hz, detected_monitor_refresh_hz, gameplay_active);
    const int64_t doubled_refresh = static_cast<int64_t>(present_refresh_hz) * 2LL;
    return static_cast<int>(std::min<int64_t>(doubled_refresh, kGraphicsRefreshHzMax));
}

} // namespace tenriff::app
