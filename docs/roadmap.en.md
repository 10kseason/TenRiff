# TenRiff Development Roadmap (staged)

**2026-10-14 · 1.8.9.7:** Input threads request MMCSS Games/High and disable execution-speed power throttling. Home action borders share their icon colors with subtle cached glow. Only TenRiff scene music is reduced to 40%; BMS songs, keysounds and previews retain their original volume scale. The fixed baseline remains 1.8.7; physical input latency improvement is not established. [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** Reuse high-resolution timers for short input and gameplay/HUD waits; RawInput wakes on messages. Ships the B comparison behavior while preserving judgement windows, R1–R5, replays and the fixed 1.8.7 baseline. Manual impressions varied between runs; this does not guarantee removal of every stutter. [Scope / 검증](release-1.8.9.6-gate.md).

**Fixed baseline: [1.8.7](baseline-1.8.7.en.md)** (2026-10-10). Use Git tag `1.8.7`, commit `6b060ad47b950ad9e0dbc516fbb40189e8e21347`; do not advance it automatically with `latest` or a new release.

**2026-10-13 · 1.8.9.4:** Rearranges the large result rank and score, shows detail accuracy and distinguishes four Gauge Shift CLEAR colors. BGM/keysound volume move to the top of Audio with 100% defaults for new settings. Includes Korean skin-label revisions, explicit obj credits, BPM! changes and correction of duplicate identical audio at the same chart time. Author uploads are optional and existing record identities are retained. [Changes, compatibility and validation (Korean)](release-1.8.9.4-gate.md). Verification and game/web publication are tracked separately; the fixed baseline remains 1.8.7.

**2026-10-13 · 1.8.9.3:** Refines result typography, colored lobby cards with subtle bloom, skin-colored key bindings and song-list level colors. Removes indicator backings and the target-gap/visual-preset controls, placing Target Lane directly above Note Color. [Changes and verification scope (Korean)](release-1.8.9.3-gate.md). Fixed baseline remains 1.8.7; 1.8.9.2 BMS correction and replay compatibility are retained.

**2026-10-13 · 1.8.9.2:** Coalesces a BMS tap and long-note head at the same lane/time into one playable hold before Native play or conversion. Preserves original files, hold spans and keysounds. Old replays retain their original layout. [Behavior and compatibility limits (Korean)](release-1.8.9.2-gate.md). The fixed baseline remains 1.8.7.

**2026-10-13 · 1.8.9.1:** Adds sequential key binding, converter keysound/hold preservation, FAST/SLOW/timing backing controls, larger gauges, preview gear dragging and text-only judgements. Existing replays retain their original conversion. [Behavior, verification and compatibility limits (Korean)](release-1.8.9.1-gate.md). The fixed baseline remains 1.8.7.

> Document scope: includes early designs, improvement proposals and dated analysis. Uncompleted proposals and historical measurements are not current behavior, defaults or performance guarantees. See [current state](current-state.en.md) and [configuration](config.en.md).

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## Current Baseline
- Windows GUI / runtime is the primary supported path.
- The fixed baseline is `1.8.7` and this source targets `1.8.9.7`; public packages bundle NK3 P64 and generalized pattern MLP inference models, but no BGA upscaler model. Selecting a compatible rights-cleared upscaler ONNX only stores its path, and BGA Upscaler remains off until the user enables it and accepts the high-spec warning; there is no automatic benchmark gate.
- The active menu / runtime is BMS-family only (`.bms/.bme/.bml/.pms`) and supports native, bundled/profile TenRiff `skin.json`, and LR2 skins.
- For current shipped behavior, read `docs/current-state.en.md` first; this roadmap is about direction and remaining work.

## 1.8.8–1.8.9 Analysis and 1.9.0 Stabilization Plan (2026-10-12)

As of 2026-10-12, the source was **the 1.8.9 release line**. It builds on 1.8.8 analysis, window sizing, LN Mix, keysounds and start controls with implemented lobby/HUD/result readability changes and explicit Judge Easy/Hard handoff into new sessions. Completed local checks and performance results/residual limits are separated from remaining package, CI and publication work. Confirmed behavior and new verification/publication status belong in the [1.8.9 release validation](release-1.8.9-gate.md). The plugin host, API and SDK are not implemented; `1.9.0` remains a future stabilization target. Preserve the fixed `1.8.7` baseline, existing tags/assets and 1.8.8 records. The earlier stage list below does not automatically authorize additional unfinished core features.

| Stage | Completion criteria |
| --- | --- |
| 1.8.8 | Validate lane FAST/SLOW, time accuracy, tap/LN events, distributions/sample counts, free window sizing, result PNGs, long LN Mix, readability, menu input, keysound preservation and solo start/retry against the [release gate](release-1.8.8-gate.md) |
| 1.8.9 | Lobby/Studio HUD/result UI with S+ preserved and Judge session handoff are implemented in source. Completed local checks, performance residuals and remaining package, CI and publication work are tracked under the [new release criteria](release-1.8.9-gate.md), including hidden upper information when extreme field/overlay geometry leaves no room |
| Before 1.9.0 | A limited read-only plugin API demonstrated by an alternative result analysis view and a personal song filter, both built without core source edits |
| After 1.9.0 stabilization | Maintain the core feature scope; focus on bug, security and compatibility fixes and plugin extensions |

- **Analysis rules:** Show only LN events meaningful under the active ruleset. Analysis must not change judgement, scoring, combo or gauge rules. Automatic completion of a normal LN is not a release-timing sample. Count unmeasurable misses separately, never as `0 ms`. Show distributions, sample counts and spread alongside the mean so alternating `−20 ms / +20 ms` does not appear precise. See the [analysis criteria and detailed plan (Korean)](result-analysis-1.8.8.ko.md).
- **Core/plugin boundary (planned):** The core owns input, the audio clock, judgement, scores, official record storage/verification, and multiplayer rules/session state. The first API exposes read-only data, separate personal settings/analysis storage, and bounded result-view, song sort/filter, HUD and menu extensions.
- **Execution isolation (planned):** Run external code in a separate execution environment. The game never waits for its computation and never calls it directly from input/audio paths. Apply permission restrictions and execution/time budgets separately; contain faults, infinite loops and excessive output. Choose the runtime and concrete budgets after prototype validation.
- **Multiplayer (planned):** The core blocks external plugin execution and result application, then rebuilds plugin-affected play state using default rules. Do not rely on plugins disabling themselves. Preserve personal keymaps and audio offsets. If future single-player extensions change judgement, charts or input, segregate their records from default records.
- **API freeze gate (planned):** If either sample repeatedly needs engine edits, the API is not ready to freeze. Ship a small SDK with API documentation, runnable samples, error logs and a launch mode that loads no plugins. Verify that users and AI can build mods using the documented API and samples.
- **Version policy (planned):** `1.9.0` stabilizes feature scope and compatibility; it does not freeze version numbers or files forever. Distinguish core patch, plugin and public-API compatibility versions, and verify existing mod/record compatibility. A future fixed-baseline change requires separate approval and synchronized documentation.

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

## 5) Build Trusted Records Before Public Ranking
- Follow [`ranked-integrity-plan.en.md`](ranked-integrity-plan.en.md): shared ranked-eligibility reasons, a dedicated Local Records screen, read-only Online Records, shadow submissions, then public verified rankings.
- The server recomputes approved BMS results from chart SHA-256 plus replay evidence and never treats a client score claim as authoritative.
- `.osu` charts and osu-derived import, ruleset, scoring, or conversion paths are ineligible for ranked registration; auxiliary OD8 statistics on a native BMS result remain local metadata only.

## 6) Refactor the Menu Architecture as It Grows
- Phases 0-6 were completed for `1.6.0`: explicit navigation, typed controllers for every settings family, and exhaustive screen descriptors are now the maintained architecture.
- Follow the staged design and per-phase gates in [`menu-refactor-plan.md`](menu-refactor-plan.md); implementation must not begin with a whole-menu rewrite.
- Trigger this work when adding a screen requires coordinated edits across the input dispatcher, render-data builder, and multiple shared cursor/return flags, or when screen behavior can no longer be covered by focused unit tests.
- First add characterization tests for navigation, settings persistence, and pointer/keyboard parity. Then move one screen family at a time out of `MenuApp` into a screen-local state/controller without changing visible behavior.
- Replace shared return-screen flags with an explicit navigation stack/back policy, and model adjustable rows (including sliders) with typed value ranges and persistence callbacks instead of row-index conditionals.
- Keep rendering DTO construction separate from input/state mutation. Finish each extraction with focused tests plus the normal client build before starting the next family; do not perform a whole-menu rewrite.
