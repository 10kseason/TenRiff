#pragma once

namespace tenriff::app {

inline constexpr int kGraphicsRefreshHzMin = 60;
inline constexpr int kGraphicsRefreshHzMax = 1050;
inline constexpr int kGraphicsMenuRefreshHzCap = 300;

[[nodiscard]] int clamp_graphics_refresh_hz(int value);
[[nodiscard]] int effective_configured_refresh_hz(int configured_refresh_hz, bool gameplay_active);
[[nodiscard]] int effective_present_refresh_hz(bool vsync_enabled, int configured_refresh_hz,
                                               int detected_monitor_refresh_hz,
                                               bool gameplay_active);
[[nodiscard]] int effective_render_fps_limit(bool vsync_enabled, int configured_refresh_hz,
                                             int detected_monitor_refresh_hz,
                                             bool gameplay_active);

} // namespace tenriff::app
