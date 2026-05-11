# Low-Latency Implementation Plan

TenRiff prioritizes minimal input-to-judgement-to-sound latency (<20 ms end-to-end). This note consolidates follow-up items from the four development manuals to tighten the pipeline while keeping the "Raw Input → SPSC queue → Audio thread judgement" philosophy intact.

## Input pipeline and timestamping
- Normalize BMS/osu! timelines into **sample positions (int64)** at load time so judgement, keysounds, and the mixer operate on
  the same deterministic clock.
- **Per-profile input offset**: Add `input_offset_ms` to `profiles/<name>/config.json` (±10 ms fine-tuning) so users can compensate for device/driver delay.
- **In-game loopback calibrator**: Provide an automatic mode that emits a brief beep on key press, measures the return peak via mic loopback, and proposes an `input_offset_ms` value.
- **HUD judgement windows**: Display current PG/GR/GD/BD windows (scaled by `window_base / rate`) so players can correlate perceived lag with the tighter window when speeding up.
- **Raw event hygiene**:
  - Windows: register `WM_INPUT` with `RIDEV_EXINPUTSINK | RIDEV_NOLEGACY`, fetch data via `GetRawInputData`, and cache `QueryPerformanceFrequency` using 64-bit math.
  - Linux: allow toggling `EVIOCGRAB` to exclusively claim devices, with a UI warning about potential compatibility trade-offs.
- **Noise/debounce filter**: Drop duplicate same-state edges while preserving real Press/Release transitions, so fast taps and releases cannot leave a key stuck down.
- **Multi-device ingest**: Process keyboards and gamepads on their own threads (RawInput/DirectInput/XInput or evdev with `poll`/`epoll`) and merge events into the shared SPSC queue with unified timestamps.
- **Stateful input tracking**: Track a per-key state machine (UP/DOWN) so duplicate DOWNs while DOWN and UPs while UP are discarded without swallowing valid down→up→down rhythm input.

## Audio and threading
- **Alternate audio backends**: Offer `--audio-backend=wasapi|asio` on Windows and `--audio-backend=alsa|jack` on Linux to reach lower buffers on pro interfaces.
- **Adaptive buffers**: Start with 48 kHz / 128 frames ×3 periods; if xruns occur, step up to 192 then 256 automatically while logging the change.
- **CPU affinity visibility**: Expose the current thread-to-core mapping in settings so users can keep AudioThread on a P-core and separated from render.
- **Isolate media keys**: Handle media-key scancodes on a lightweight side thread (SDL/OS API) to prevent them from stalling the main RawInput path.
- **Playback buffer positioning**: Treat the audio callback as working in the **playback buffer sample domain**. Capture `playhead_samples` from the device clock and compute `buffer_start_samples = playhead_samples + padding` (e.g., WASAPI `GetCurrentPadding`). All mixing happens relative to `buffer_start_samples`/`buffer_end_samples` so keysounds align with when the buffer will actually play.
- **Input consumption rules in the audio thread**: When popping input events inside the callback, convert timestamps via ClockSync to `press_sample` and branch:
  - `press_sample < buffer_start`: late input — still apply judgement, but pin any keysound to `buffer_start` so it plays as soon as possible.
  - `buffer_start ≤ press_sample < buffer_end`: normal — mix at the computed offset.
  - `press_sample ≥ buffer_end`: future — leave it queued or park it in an audio-thread staging buffer.
- **ClockSync robustness**: Keep linear regression but harden it with outlier rejection (MAD/Huber), a sliding window with EMA-updated slope/intercept, reset hooks on device reset/underrun/backend swap, and a monotonic clamp so converted audio times never go backward.
- **Sample-domain judgement**: Convert PG/GR/GD/BD windows to samples (`round(window_ms * sample_rate / 1000.0)`) and compare entirely in the sample domain. If variable playback rates are allowed, scale chart event samples or lock rate per song to keep windows stable.
- **Keysound determinism**: Pre-decode/preload all keysounds before gameplay, forbid allocations/file I/O/locks inside AudioThread, and keep mixer buffers SoA/SIMD-friendly with zero allocations during callbacks.
- **No allocations inside audio**: “No allocations, no file I/O, no locks inside AudioThread.” is a hard rule; treat violations as bugs.
- **Sample-domain rule of thumb**: Judgement and keysound scheduling are expressed in the playback buffer sample domain, not wall-clock time; the audio callback derives `buffer_start_samples` from device padding and places every event relative to it.

## Rendering and frame pacing
- **Driver pre-render floor**: Document a "pre-rendered frames = 0/1" option (e.g., `__GL_MaxFramesAllowed=1` on NVIDIA) alongside VSYNC OFF and no triple buffering.
- **Vendor guidance**: Call out NVIDIA "Maximum pre-rendered frames = 1" / "Low Latency Mode = Ultra", AMD Anti-Lag+ alternatives, and Intel frame queue toggles, with a matching in-game "Frame queue mode" setting so users can find and disable driver-side pre-rendering.
- **Menu timing parity**: Use the same monotonic clock and input→audio separation in menus/results; avoid letting render code directly timestamp inputs outside gameplay. Keep the audio backend open in menus with silent callbacks so `playhead_samples`/`buffer_start_samples` remain valid before gameplay starts.
- **Render is read-only**: Rendering consumes immutable gameplay snapshots; it must never timestamp inputs or mutate authoritative audio/judgement state.

## UI/UX for latency awareness
- **Latency HUD**: Small overlay showing input→audio delta histogram, queue depth, xruns, and highlighting 99.9th percentile >10 ms in red.
- **Audio callback budget**: Display `callback_time_ms / buffer_length_ms` per tick to flag overruns early; surface when processing exceeds the buffer budget.
- **Late input counter**: Track the percentage/count of inputs where `press_sample < buffer_start` to spot host/OS scheduling or device issues.
- **Lag toast**: If measured end-to-end delay exceeds a threshold (e.g., 25 ms), surface a toast with mitigation tips (ASIO/JACK, RawInput/evdev grab, VSYNC OFF).
- **Input backend toggles**: Settings checkboxes for RawInput/evdev grab default to on, with warnings that disabling them increases latency but may help compatibility.
- **NKRO guidance**: In the 10-key NKRO test UI, remind users to press many keys together to verify true NKRO and recommend mechanical keyboards if ghosting appears.

## QA and tooling
- **Performance log export**: Optional CSV logging of latency, buffer depth, xruns, and core usage during normal play for offline analysis.
- **Environment preflight**: Launcher or startup script should detect power plan, CPU boost, and USB polling rate, suggesting high-performance settings when suboptimal.
- **Privilege guidance**: When RT scheduling or evdev grab fails due to permissions, show remediation steps (admin rerun, `/etc/security/limits.conf` hints).
- **Built-in burn-in**: Add `--burnin` to auto-play charts for hours while monitoring input queue, xruns, and latency to surface regressions early.
- **Sample-time replays**: Record replays as `{keycode, down/up, press_sample (int64)}` so sessions can be deterministically re-simulated on the sample timeline to reproduce latency spikes.
