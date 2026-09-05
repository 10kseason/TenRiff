# TenRiff Development Roadmap (staged)

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## Current baseline
- Windows GUI/runtime is the primary supported path.
- Project version line is `1.7.0`; public packages bundle NK3 P64 and generalized pattern MLP inference models, but no BGA upscaler model. Selecting a compatible rights-cleared upscaler ONNX only stores its path, and BGA Upscaler remains off until the user enables it and accepts the high-spec warning; there is no automatic benchmark gate.
- The active menu/runtime is BMS-family only (`.bms/.bme/.bml/.pms`) and supports native, bundled/profile TenRiff `skin.json`, and LR2 skins.
- For current shipped behavior, read `docs/current-state.md` first; this roadmap is about direction and remaining work.

## 0) Fix the skeleton and master clock
- Treat **AudioThread as the master clock** for all timing-sensitive work.
- **InputThread** should timestamp events from RawInput/evdev and push them into an SPSC queue.
- Normalize chart timelines into **sample positions (int64)** so the audio thread can consume deterministic timestamps.
- **Render** only consumes snapshots to draw; judgements/scores are finalized on the audio side.

## 0.5) Harden the low-latency loop
- Implement one backend end-to-end (WASAPI/ALSA) with device padding exposure and explicit `buffer_start_samples` computation so the mixer works in the playback buffer domain.
- Fortify ClockSync with outlier rejection, sliding-window EMA updates, reset hooks on device changes/underruns, and monotonic clamping to prevent regressions during drift or spikes.
- In AudioThread, pop inputs, convert to sample time, and branch late/normal/future for keysound placement so late inputs still make sound and future ones are staged.
- Express judgement windows in samples, add HUD counters for callback budget/late inputs/xruns, and scale windows appropriately when rate is adjustable.
- ✅ Capture replays as sample-time input traces (lane/state/sample) and write JSON exports for deterministic reproduction.

## 1) Make a full song playable end-to-end
- Keep the audio backend running from the menu with silent callbacks so the master clock is stable before gameplay.
- ✅ Stand up the UI state machine (console): **Title → Song Select → Play → Result** with InputThread/SPSC ingestion.
- ✅ Windows D3D11 menu UI added (text + background + focus styling).
- ✅ Add an async SongIndexerThread plus cached index (mtime/hash) so Song Select stays responsive while scanning.
- Minimal BMS loader (essential channels only) → note scheduling → judgement → result screen.
- Use the audio engine to schedule preview audio (no UI-thread playback) and preload keysounds during Song Select.

## 1.5) Harden BMS-only chart support
- The former multi-format direction is superseded for the current release line; keep loading, indexing, replay, result, and difficulty-table behavior focused on the BMS family.
- Continue hardening real-pack encoding, keysound, BGA, long-note, and lane-layout compatibility without reintroducing archive or alternate-format import paths.
- Keep optional integrations user-supplied, disabled until explicitly enabled, and safely recoverable to native behavior.

## 2) Key remap plus 8K/10K modes
- ✅ Key remapping UI per the “리맵 UI 플로우” spec (including NKRO test).
- Once this lands, the project becomes a solid personal practice tool.

## 3) Add lane-transform and random modes
- ✅ **Mirror**, **Full Random (FR)**, and **Super Random (SR)** are implemented; defer **AR** until its behavior is specified.

## 4) Attach a launcher
- Handle folder checks, first-run config creation, and error code cataloging.
- Completing this makes the game self-contained on a local PC.

## 5) Build trusted records before public ranking
- Follow [`ranked-integrity-plan.md`](ranked-integrity-plan.md): shared ranked-eligibility reason codes, a dedicated Local Records screen, then read-only Online Records, shadow submission, and finally public verified rankings.
- The server must recompute approved BMS results from chart SHA-256 plus replay evidence; client score claims are never authoritative.
- `.osu` charts and osu-derived import, ruleset, scoring, or conversion paths are not eligible for ranked registration. Auxiliary OD8 statistics on an otherwise native BMS result remain local metadata only.
- Development slice complete: a separate headless server exposes a read-only schema-v1 BMS leaderboard snapshot and Song Select Records can switch between Local and Online without blocking local play.
- `1.5.1` fixes the self-hosted implementation as a release baseline under [`release-1.5.1-gate.md`](release-1.5.1-gate.md); operating a project-owned central official server remains separately gated.

## 6) Refactor the menu architecture as it grows
- Phases 0-6 were completed for `1.6.0`: explicit navigation, typed controllers for every settings family, and exhaustive screen descriptors are now the maintained architecture.
- Follow the staged design and per-phase gates in [`menu-refactor-plan.md`](menu-refactor-plan.md); implementation must not begin with a whole-menu rewrite.
- Trigger this work when adding a screen requires coordinated edits across the input dispatcher, render-data builder, and multiple shared cursor/return flags, or when screen behavior can no longer be covered by focused unit tests.
- First add characterization tests for navigation, settings persistence, and pointer/keyboard parity. Then move one screen family at a time out of `MenuApp` into a screen-local state/controller without changing visible behavior.
- Replace shared return-screen flags with an explicit navigation stack/back policy, and model adjustable rows (including sliders) with typed value ranges and persistence callbacks instead of row-index conditionals.
- Keep rendering DTO construction separate from input/state mutation. Finish each extraction with focused tests plus the normal client build before starting the next family; do not perform a whole-menu rewrite.
