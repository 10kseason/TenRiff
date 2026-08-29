#include "app/GraphicsTiming.h"

#include <algorithm>
#include <cstdint>

namespace tenriff::app {

namespace {

constexpr std::uint32_t kDxgiStatusOccluded = 0x087A0001u;
constexpr std::uint32_t kDxgiStatusModeChanged = 0x087A0007u;
constexpr std::uint32_t kDxgiErrorInvalidCall = 0x887A0001u;

}  // namespace

int clamp_graphics_refresh_hz(int value) {
    return std::clamp(value, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
}

int normalize_graphics_refresh_hz(int value) {
    if (value == kGraphicsRefreshHzUnlimited) {
        return kGraphicsRefreshHzUnlimited;
    }
    // Positive values are legacy fixed caps. The current UI intentionally has
    // only Match Display and Unlimited, so migrate every non-zero value.
    return kGraphicsRefreshHzMatchDisplay;
}

int cycle_graphics_refresh_hz(int value, int direction) {
    static_cast<void>(direction);
    return normalize_graphics_refresh_hz(value) == kGraphicsRefreshHzUnlimited
               ? kGraphicsRefreshHzMatchDisplay
               : kGraphicsRefreshHzUnlimited;
}

int effective_configured_refresh_hz(int configured_refresh_hz,
                                    int detected_monitor_refresh_hz,
                                    bool gameplay_active) {
    if (configured_refresh_hz == kGraphicsRefreshHzUnlimited) {
        return gameplay_active ? kGraphicsUnlimitedFpsCap : kGraphicsMenuRefreshHzCap;
    }
    return clamp_graphics_refresh_hz(detected_monitor_refresh_hz);
}

int effective_present_refresh_hz(bool vsync_enabled, int configured_refresh_hz,
                                 int detected_monitor_refresh_hz, bool gameplay_active) {
    if (!vsync_enabled) {
        return clamp_graphics_refresh_hz(detected_monitor_refresh_hz);
    }
    return clamp_graphics_refresh_hz(detected_monitor_refresh_hz);
}

int effective_render_fps_limit(bool vsync_enabled, int configured_refresh_hz,
                               int detected_monitor_refresh_hz, bool gameplay_active) {
    if (!vsync_enabled) {
        if (configured_refresh_hz == kGraphicsRefreshHzUnlimited) {
            return gameplay_active ? kGraphicsUnlimitedFpsCap : kGraphicsMenuRefreshHzCap;
        }
        return effective_configured_refresh_hz(
            configured_refresh_hz, detected_monitor_refresh_hz, gameplay_active);
    }

    const int present_refresh_hz = effective_present_refresh_hz(
        true, configured_refresh_hz, detected_monitor_refresh_hz, gameplay_active);
    const int64_t doubled_refresh = static_cast<int64_t>(present_refresh_hz) * 2LL;
    return static_cast<int>(std::min<int64_t>(doubled_refresh, kGraphicsRefreshHzMax));
}

bool should_allow_tearing_present(bool vsync_enabled,
                                  bool fullscreen_exclusive,
                                  bool swap_chain_allows_tearing) {
    return !vsync_enabled && !fullscreen_exclusive && swap_chain_allows_tearing;
}

bool should_record_presented_frame(std::uint32_t present_hr) {
    return present_hr == 0u;
}

bool should_treat_present_failure_as_transient(std::uint32_t present_hr,
                                               bool fullscreen_requested,
                                               bool window_in_foreground,
                                               bool window_minimized) {
    if (window_minimized) {
        return true;
    }
    if (present_hr == kDxgiStatusOccluded) {
        return true;
    }
    if (present_hr == kDxgiStatusModeChanged) {
        return true;
    }
    return present_hr == kDxgiErrorInvalidCall &&
           fullscreen_requested &&
           !window_in_foreground;
}

} // namespace tenriff::app
