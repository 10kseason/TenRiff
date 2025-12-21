# Low-Latency Implementation Plan

TenRiff prioritizes minimal input-to-judgement-to-sound latency (<20 ms end-to-end). This note consolidates follow-up items from the four development manuals to tighten the pipeline while keeping the "Raw Input → SPSC queue → Audio thread judgement" philosophy intact.

## Input pipeline and timestamping
- **Per-profile input offset**: Add `input_offset_ms` to `profiles/<name>/config.json` (±10 ms fine-tuning) so users can compensate for device/driver delay.
- **In-game loopback calibrator**: Provide an automatic mode that emits a brief beep on key press, measures the return peak via mic loopback, and proposes an `input_offset_ms` value.
- **HUD judgement windows**: Display current PG/GR/GD/BD windows (scaled by `window_base / rate`) so players can correlate perceived lag with the tighter window when speeding up.
- **Raw event hygiene**:
  - Windows: register `WM_INPUT` with `RIDEV_EXINPUTSINK | RIDEV_NOLEGACY`, fetch data via `GetRawInputData`, and cache `QueryPerformanceFrequency` using 64-bit math.
  - Linux: allow toggling `EVIOCGRAB` to exclusively claim devices, with a UI warning about potential compatibility trade-offs.
- **Noise/debounce filter**: Drop repeated up/down pairs that arrive within 5 ms on the same key to reject hardware chatter.
- **Multi-device ingest**: Process keyboards and gamepads on their own threads (RawInput/DirectInput/XInput or evdev with `poll`/`epoll`) and merge events into the shared SPSC queue with unified timestamps.

## Audio and threading
- **Alternate audio backends**: Offer `--audio-backend=wasapi|asio` on Windows and `--audio-backend=alsa|jack` on Linux to reach lower buffers on pro interfaces.
- **Adaptive buffers**: Start with 48 kHz / 128 frames ×3 periods; if xruns occur, step up to 192 then 256 automatically while logging the change.
- **CPU affinity visibility**: Expose the current thread-to-core mapping in settings so users can keep AudioThread on a P-core and separated from render.
- **Isolate media keys**: Handle media-key scancodes on a lightweight side thread (SDL/OS API) to prevent them from stalling the main RawInput path.

## Rendering and frame pacing
- **Driver pre-render floor**: Document a "pre-rendered frames = 0/1" option (e.g., `__GL_MaxFramesAllowed=1` on NVIDIA) alongside VSYNC OFF and no triple buffering.
- **Menu timing parity**: Use the same monotonic clock and input→audio separation in menus/results; avoid letting render code directly timestamp inputs outside gameplay.

## UI/UX for latency awareness
- **Latency HUD**: Small overlay showing input→audio delta histogram, queue depth, xruns, and highlighting 99.9th percentile >10 ms in red.
- **Lag toast**: If measured end-to-end delay exceeds a threshold (e.g., 25 ms), surface a toast with mitigation tips (ASIO/JACK, RawInput/evdev grab, VSYNC OFF).
- **Input backend toggles**: Settings checkboxes for RawInput/evdev grab default to on, with warnings that disabling them increases latency but may help compatibility.
- **NKRO guidance**: In the 10-key NKRO test UI, remind users to press many keys together to verify true NKRO and recommend mechanical keyboards if ghosting appears.

## QA and tooling
- **Performance log export**: Optional CSV logging of latency, buffer depth, xruns, and core usage during normal play for offline analysis.
- **Environment preflight**: Launcher or startup script should detect power plan, CPU boost, and USB polling rate, suggesting high-performance settings when suboptimal.
- **Privilege guidance**: When RT scheduling or evdev grab fails due to permissions, show remediation steps (admin rerun, `/etc/security/limits.conf` hints).
- **Built-in burn-in**: Add `--burnin` to auto-play charts for hours while monitoring input queue, xruns, and latency to surface regressions early.
