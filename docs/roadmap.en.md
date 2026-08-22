# TenRiff Development Roadmap (staged)

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## Current Baseline
- Windows GUI / runtime is the primary supported path.
- Project version line is `1.4.5.1`; public packages bundle NK3 P64 and generalized pattern MLP inference models, but no BGA upscaler model. Selecting a compatible rights-cleared upscaler ONNX only stores its path, and BGA Upscaler remains off until the user enables it and accepts the high-spec warning; there is no automatic benchmark gate.
- The active menu / runtime is BMS-family only (`.bms/.bme/.bml/.pms`) and supports native/LR2 skins.
- For current shipped behavior, read `docs/current-state.en.md` first; this roadmap is about direction and remaining work.

## 0) Fix the Skeleton and Master Clock
- Treat **AudioThread as the master clock** for all timing-sensitive work.
- **InputThread** should timestamp events from RawInput / evdev and push them into an SPSC queue.
- Normalize chart timelines into **sample positions (int64)** so the audio thread can consume deterministic timestamps.
- **Render** only consumes snapshots to draw; judgements / scores are finalized on the audio side.

## 0.5) Harden the Low-Latency Loop
- Implement one backend end-to-end (WASAPI / ALSA) with device padding exposure and explicit `buffer_start_samples` computation so the mixer works in the playback-buffer domain.
- Fortify ClockSync with outlier rejection, sliding-window EMA updates, reset hooks on device changes / underruns, and monotonic clamping to prevent regressions during drift or spikes.
- In AudioThread, pop inputs, convert them to sample time, and branch late / normal / future for keysound placement so late inputs still make sound and future ones are staged.
- Express judgement windows in samples, add HUD counters for callback budget / late inputs / xruns, and scale windows appropriately when rate is adjustable.
- ✅ Capture replays as sample-time input traces (lane / state / sample) and write JSON exports for deterministic reproduction.

## 1) Make a Full Song Playable End-to-End
- Keep the audio backend running from the menu with silent callbacks so the master clock is stable before gameplay starts.
- ✅ Stand up the UI state machine (console): **Title -> Song Select -> Play -> Result** with InputThread / SPSC ingestion.
- ✅ Windows D3D11 menu UI added (text + background + focus styling).
- ✅ Add an async SongIndexerThread plus cached index (mtime / hash) so Song Select stays responsive while scanning.
- Minimal BMS loader (essential channels only) -> note scheduling -> judgement -> result screen.
- Use the audio engine to schedule preview audio (no UI-thread playback) and preload keysounds during Song Select.

## 1.5) Harden BMS-Only Chart Support
- The former multi-format direction is superseded for the current release line; keep loading, indexing, replay, result, and difficulty-table behavior focused on the BMS family.
- Continue hardening real-pack encoding, keysound, BGA, long-note, and lane-layout compatibility without reintroducing archive or alternate-format import paths.
- Keep optional integrations user-supplied, disabled until explicitly enabled, and safely recoverable to native behavior.

## 2) Key Remap Plus 8K / 10K Modes
- ✅ Key remapping UI per the "리맵 UI 플로우" spec (including NKRO test).
- Once this lands, the project becomes a solid personal practice tool.

## 3) Add Lane-Transform and Random Modes
- ✅ **Mirror**, **Full Random (FR)**, and **Super Random (SR)** are implemented; defer **AR** until its behavior is specified.

## 4) Attach a Launcher
- Handle folder checks, first-run config creation, and error-code cataloging.
- Completing this makes the game self-contained on a local PC.
