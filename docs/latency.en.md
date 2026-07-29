# Low-Latency Implementation Plan

TenRiff prioritizes minimal input-to-judgement-to-sound latency (<20 ms end-to-end). This note consolidates the follow-up items from the four development manuals to tighten the pipeline while keeping the "Raw Input -> SPSC queue -> Audio thread judgement" philosophy intact.

## Input Pipeline and Timestamping
- Normalize BMS timelines into **sample positions (int64)** at load time so judgement, keysounds, and the mixer all operate on the same deterministic clock.
- **Per-profile input offset**: add `input_offset_ms` to `profiles/<name>/config.json` (±10 ms fine tuning) so users can compensate for device / driver delay.
- **In-game loopback calibrator**: provide an automatic mode that emits a short beep on key press, measures the return peak through mic loopback, and proposes an `input_offset_ms` value.
- **HUD judgement windows**: display the current PG / GR / GD / BD windows (scaled by `window_base / rate`) so players can correlate perceived lag with the tighter window when speeding up.
- **Raw event hygiene**:
  - Windows: register `WM_INPUT` with `RIDEV_EXINPUTSINK | RIDEV_NOLEGACY`, fetch data via `GetRawInputData`, and cache `QueryPerformanceFrequency` using 64-bit math.
  - Linux: allow toggling `EVIOCGRAB` to claim devices exclusively, with a UI warning about compatibility trade-offs.
- **Noise / debounce filter**: drop duplicate same-state edges while preserving real Press/Release transitions, so fast taps and releases cannot leave a key stuck down.
- **Multi-device ingest**: process keyboards and gamepads on their own threads (RawInput / DirectInput / XInput or evdev with `poll` / `epoll`) and merge events into the shared SPSC queue with unified timestamps.
- **Stateful input tracking**: track a per-key state machine (UP / DOWN) so duplicate DOWNs while DOWN and UPs while UP are discarded without swallowing valid down -> up -> down rhythm input.

## Audio and Threading
- **Alternate audio backends**: offer `--audio-backend=wasapi|asio` on Windows and `--audio-backend=alsa|jack` on Linux to reach lower buffers on pro interfaces.
- **Adaptive buffers**: start at 48 kHz / 128 frames x 3 periods; if xruns occur, step up to 192 then 256 automatically while logging the change.
- **CPU affinity visibility**: expose the current thread-to-core mapping in settings so users can keep AudioThread on a P-core and separated from render.
- **Isolate media keys**: handle media-key scancodes on a lightweight side thread (SDL / OS API) to prevent them from stalling the main RawInput path.
- **Playback buffer positioning**: treat the audio callback as operating in the **playback buffer sample domain**. Capture `playhead_samples` from the device clock and compute `buffer_start_samples = playhead_samples + padding` (for example via WASAPI `GetCurrentPadding`). All mixing happens relative to `buffer_start_samples` / `buffer_end_samples` so keysounds align with when the buffer will actually play.
- **Input consumption rules in the audio thread**: when popping input events inside the callback, convert timestamps through ClockSync to `press_sample` and branch:
  - `press_sample < buffer_start`: late input - still apply judgement, but pin any keysound to `buffer_start` so it plays as soon as possible.
  - `buffer_start ≤ press_sample < buffer_end`: normal - mix at the computed offset.
  - `press_sample ≥ buffer_end`: future - leave it queued or park it in an audio-thread staging buffer.
- **ClockSync robustness**: keep linear regression, but harden it with outlier rejection (MAD / Huber), a sliding window with EMA-updated slope / intercept, reset hooks on device reset / underrun / backend swap, and a monotonic clamp so converted audio times never go backward.
- **Sample-domain judgement**: convert PG / GR / GD / BD windows to samples (`round(window_ms * sample_rate / 1000.0)`) and compare entirely in the sample domain. If variable playback rates are allowed, scale chart event samples or lock rate per song to keep windows stable.
- **Keysound determinism**: pre-decode / preload all keysounds before gameplay, forbid allocations / file I/O / locks inside AudioThread, and keep mixer buffers SoA / SIMD-friendly with zero allocations during callbacks.
- **No allocations inside audio**: "No allocations, no file I/O, no locks inside AudioThread." is a hard rule; treat violations as bugs.
- **Sample-domain rule of thumb**: judgement and keysound scheduling are expressed in the playback buffer sample domain, not wall-clock time; the audio callback derives `buffer_start_samples` from device padding and places every event relative to it.

## Rendering and Frame Pacing
- **Driver pre-render floor**: document a "pre-rendered frames = 0/1" option (for example `__GL_MaxFramesAllowed=1` on NVIDIA) alongside VSYNC OFF and no triple buffering.
- **Vendor guidance**: call out NVIDIA "Maximum pre-rendered frames = 1" / "Low Latency Mode = Ultra", AMD Anti-Lag+ alternatives, and Intel frame queue toggles, with a matching in-game "Frame queue mode" setting so users can find and disable driver-side pre-rendering.
- **Menu timing parity**: use the same monotonic clock and input -> audio separation in menus / results; do not let render code timestamp inputs directly outside gameplay. Keep the audio backend open in menus with silent callbacks so `playhead_samples` / `buffer_start_samples` remain valid before gameplay starts.
- **Render is read-only**: rendering consumes immutable gameplay snapshots; it must never timestamp inputs or mutate authoritative audio / judgement state.

## UI / UX for Latency Awareness
- **Latency HUD**: small overlay showing the input -> audio delta histogram, queue depth, xruns, and highlighting the 99.9th percentile above 10 ms in red.
- **Audio callback budget**: display `callback_time_ms / buffer_length_ms` per tick to flag overruns early; surface when processing exceeds the buffer budget.
- **Late input counter**: track the percentage / count of inputs where `press_sample < buffer_start` to spot host / OS scheduling or device issues.
- **Lag toast**: if measured end-to-end delay exceeds a threshold (for example 25 ms), show a toast with mitigation tips (ASIO / JACK, RawInput / evdev grab, VSYNC OFF).
- **Input backend toggles**: settings checkboxes for RawInput / evdev grab should default to on, with warnings that disabling them increases latency but may help compatibility.
- **NKRO guidance**: in the 10-key NKRO test UI, remind users to press many keys together to verify real NKRO and recommend mechanical keyboards if ghosting appears.

## QA and Tooling
- **Performance log export**: optionally log latency, buffer depth, xruns, and core usage to CSV during normal play for offline analysis.
- **Environment preflight**: the launcher or startup script should detect power plan, CPU boost, and USB polling rate, and suggest high-performance settings when the environment is suboptimal.
- **Privilege guidance**: when RT scheduling or evdev grab fails because of permissions, show remediation steps (rerun as administrator, `/etc/security/limits.conf` hints).
- **Built-in burn-in**: add `--burnin` to auto-play charts for hours while monitoring input queue, xruns, and latency so regressions surface early.
- **Sample-time replays**: record replays as `{keycode, down/up, press_sample (int64)}` so sessions can be deterministically re-simulated on the sample timeline to reproduce latency spikes.
