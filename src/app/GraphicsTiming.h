#pragma once

#include <cstdint>

namespace tenriff::app {

inline constexpr int kGraphicsRefreshHzMin = 60;
inline constexpr int kGraphicsRefreshHzMax = 1050;
inline constexpr int kGraphicsMenuRefreshHzCap = 300;
inline constexpr int kGraphicsOffVsyncGameplayRefreshHzFloor = 300;
inline constexpr int kGraphicsOffVsyncGameplayMonitorMultiplier = 2;

[[nodiscard]] int clamp_graphics_refresh_hz(int value);
[[nodiscard]] int effective_configured_refresh_hz(int configured_refresh_hz, bool gameplay_active);
[[nodiscard]] int effective_present_refresh_hz(bool vsync_enabled, int configured_refresh_hz,
                                               int detected_monitor_refresh_hz,
                                               bool gameplay_active);
[[nodiscard]] int effective_render_fps_limit(bool vsync_enabled, int configured_refresh_hz,
                                             int detected_monitor_refresh_hz,
                                             bool gameplay_active);
[[nodiscard]] bool should_allow_tearing_present(bool vsync_enabled,
                                                bool fullscreen_exclusive,
                                                bool swap_chain_allows_tearing);
[[nodiscard]] bool should_treat_present_failure_as_transient(std::uint32_t present_hr,
                                                             bool fullscreen_requested,
                                                             bool window_in_foreground,
                                                             bool window_minimized);

} // namespace tenriff::app
