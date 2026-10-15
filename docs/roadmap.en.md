**2026-10-16 · Current release 1.9.2:** Adds the startup F10 Guest/Online choice, permanent Guest/historical-record upload exclusion, dynamic original-resolution AI with FFmpeg deblock, and removal of the skin-export row. Runtime/source packages contain no BGA model; the model path is empty and upscaling OFF by default. The fixed baseline stays 1.9.0. [Changes and compatibility](release-1.9.2.en.md) · [Verification gate](release-1.9.2-gate.md). Dated entries below retain their historical scope; this 1.9.2 note defines current behavior.

> 2026-10-16: [FFmpeg deblock and external ONNX x2](deblock-lunasr-20261016.en.md)

> 2026-10-16: [Restore BGA at its original resolution before fitting](native-upscale-20261016.en.md)

# TenRiff Development Roadmap (staged)

**2026-10-16 · 1.9.1.1 UI:** [Home, graphics, loading and fixed grades — 1.9.1.1 follow-up](ui-feedback-20261016.en.md).

**2026-10-15 · 1.9.1.1 online BMS follow-up:** Submit new BMS gauge/timing/RANDOM-branch records to the Sites community leaderboard, with rankings separated by policy and selected branch content. The updated client and corresponding web deployment are required; Sites records remain `client_submitted`, and the separate Ranked verifier gate stays in place. The 1.9.1 local-only note below describes that release; published files and the fixed 1.9.0 baseline are preserved. [Scope, compatibility and verification](feedback-1.9.1.1.en.md).

**2026-10-15 · 1.9.1:** The user-authorized 1.9.1 includes the earlier volume/input/outro fixes, BMS TOTAL/RANDOM compatibility, Result colors and the 10 mark. A Home language button cycles EN → KO → JA → ZH and saves the choice. New BMS gauge/branch records remain local until server verification supports them. The fixed baseline stays 1.9.0; earlier releases, installations and profiles are preserved. Dated local notes below retain their historical no-version-bump/unpublished status; this 1.9.1 scope supersedes that status. [Changes and compatibility](feedback-1.9.1.en.md) · [Release verification gate](release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL / RANDOM / visual follow-up — unreleased, local build and CTest regression passed:** This local follow-up builds on 1.9.0.2. Result levels use the song lobby’s red level text and key-mode symbol colors, and the logo adopts the supplied 10-shaped mark. New ordinary BMS plays use LR2-based recovery/damage with #TOTAL and note count; authored #RANDOM branches receive a seed independent of lane randomization. Historical replays retain their original behavior. The earlier unified-volume changes remain included. Published 1.9.0.2, the fixed 1.9.0 baseline and the version number are unchanged. [Formulas, branching, compatibility and verification scope](bms-total-random-followup.en.md).

**2026-10-15 · BMS volume follow-up — unreleased, local build and CTest regression passed:** This local follow-up to 1.9.0.2 replaces separate background/keysound controls with one BMS performance volume control (Korean: **BMS 연주 볼륨**). Its new default is 20%; saved values are retained. Master/TenRiff music defaults remain 10%/100%, the remaining volumes use 1 percentage-point steps, and lobby preview keeps ×0.75 attenuation. The version number and fixed 1.9.0 baseline are unchanged. Dated entries below retain their original context. [Settings and compatibility](config.en.md#audio).

This unreleased follow-up also shows difficulty-table loading status and changes debounce UI steps to 1ms, with a 20ms maximum and 0ms default. The old automatic 5→8ms rewrite is retired, preserving saved 5ms and 8ms values. The Korean skin label becomes `LN 끝 줄임`; appearance and judgement behavior are unchanged. In ordinary single-player with Listen to End enabled, `Esc` skips the outro directly to results without opening Pause once the final note and LN tail have been processed.

**2026-10-15 · 1.9.0.2 release candidate — publication verification gates:** Based on published 1.9.0.1, this candidate makes Options green, shows green ⑤ only for the selected built-in 5K Aery table, colors registered 4/6/8/10-key table symbols, and uses red levels/white scores, adds `lv:` searches, changes the new debounce default to 0ms, and uses 1% audio-volume steps. Move Song Select sort/filter and table selectors beside Song Library and increase the album cover height. Master volume still defaults to 10%; the fixed baseline remains 1.9.0. [Changes and usage](feedback-1.9.0.2.en.md). The release page determines publication status and available downloads. [Release scope and verification](release-1.9.0.2-gate.md).

**2026-10-15 · 1.9.0.1:** Includes 10% default master volume and aligned lobby icon centers. Preview ×0.75, profile-assigned F5/F6 -10/+10, Xbox/XInput button input, single modal file pickers, and corrected BMS BPM/STOP order with historical replay compatibility. The fixed baseline remains 1.9.0. [Changes](feedback-20261015.en.md) · [Release verification scope](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / new fixed baseline:** Promotes the reviewed local candidate to the requested release line: independent BMS/TenRiff music volume, lobby/filter/skin controls, grades and SUBARTIST credits, localized mods, EX timing and upload status. Normal/Hard use R5, EX uses R6; historical R1–R5 records and saved settings are preserved. [Baseline](baseline-1.9.0.en.md) · [Release verification scope](release-1.9.0-gate.md). Dated entries below retain their historical release context.

**2026-10-14 · 1.8.9.7:** Input threads request MMCSS Games/High and disable execution-speed power throttling. Home action borders share their icon colors with subtle cached glow. Only TenRiff scene music is reduced to 40%; BMS songs, keysounds and previews retain their original volume scale. The fixed baseline remains 1.8.7; physical input latency improvement is not established. [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** Reuse high-resolution timers for short input and gameplay/HUD waits; RawInput wakes on messages. Ships the B comparison behavior while preserving judgement windows, R1–R5, replays and the fixed 1.8.7 baseline. Manual impressions varied between runs; this does not guarantee removal of every stutter. [Scope / 검증](release-1.8.9.6-gate.md).

**Fixed baseline: [1.9.0](baseline-1.9.0.en.md)** (explicit user request, 2026-10-14). Use Git tag `1.9.0`; do not advance automatically with `main` or `latest`. Preserve the 1.8.7 baseline and previous releases as history.

**2026-10-13 · 1.8.9.4:** Rearranges the large result rank and score, shows detail accuracy and distinguishes four Gauge Shift CLEAR colors. BGM/keysound volume move to the top of Audio with 100% defaults for new settings. Includes Korean skin-label revisions, explicit obj credits, BPM! changes and correction of duplicate identical audio at the same chart time. Author uploads are optional and existing record identities are retained. [Changes, compatibility and validation (Korean)](release-1.8.9.4-gate.md). Verification and game/web publication are tracked separately; the fixed baseline remains 1.8.7.

**2026-10-13 · 1.8.9.3:** Refines result typography, colored lobby cards with subtle bloom, skin-colored key bindings and song-list level colors. Removes indicator backings and the target-gap/visual-preset controls, placing Target Lane directly above Note Color. [Changes and verification scope (Korean)](release-1.8.9.3-gate.md). Fixed baseline remains 1.8.7; 1.8.9.2 BMS correction and replay compatibility are retained.

**2026-10-13 · 1.8.9.2:** Coalesces a BMS tap and long-note head at the same lane/time into one playable hold before Native play or conversion. Preserves original files, hold spans and keysounds. Old replays retain their original layout. [Behavior and compatibility limits (Korean)](release-1.8.9.2-gate.md). The fixed baseline remains 1.8.7.

**2026-10-13 · 1.8.9.1:** Adds sequential key binding, converter keysound/hold preservation, FAST/SLOW/timing backing controls, larger gauges, preview gear dragging and text-only judgements. Existing replays retain their original conversion. [Behavior, verification and compatibility limits (Korean)](release-1.8.9.1-gate.md). The fixed baseline remains 1.8.7.

> Document scope: includes early designs, improvement proposals and dated analysis. Uncompleted proposals and historical measurements are not current behavior, defaults or performance guarantees. See [current state](current-state.en.md) and [configuration](config.en.md).

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## Current Baseline
- Windows GUI / runtime is the primary supported path.
- The fixed baseline is `1.9.0` and this source targets release `1.9.2`; public packages bundle NK3 P64 and generalized pattern MLP inference models, but no BGA upscaler model. Selecting a compatible rights-cleared upscaler ONNX only stores its path, and BGA Upscaler remains off until the user enables it and accepts the high-spec warning; there is no automatic benchmark gate.
- The active menu / runtime is BMS-family only (`.bms/.bme/.bml/.pms`) and supports native, bundled/profile TenRiff `skin.json`, and LR2 skins.
- For current shipped behavior, read `docs/current-state.en.md` first; this roadmap is about direction and remaining work.

## 1.9.0.2 release candidate validation

These results describe the candidate before the final Aery ⑤, registered-table symbol-color and Song Select layout follow-ups. The development build and independently extracted source build each passed the Release build, CTest 3/3 (1,224 cases each, 0 integration skips), editor 35/35 and native MenuApp/GameSession 307/307. Native smoke includes 22 search key-handling checks. The earlier render passed six fixtures/360 Presents/86 geometry hit checks. Final build and display checks for Aery ⑤ and 4UE/6UE/8UE/Ⓡ colors remain release gates, including Aery FHD/small-window captures, red numeric levels, default Lv/other/custom-table exclusions, and non-overlapping header selectors, taller cover, best-record area and song count.

The first packaging gate failed an existing skin lane-number shortcut test once. A focused retry with the unchanged test executable, the full CTest retry and independent build checks passed, but **the root cause remains unconfirmed**. Preserve the original failure; passing retries do not establish a fix for the intermittent cause.

Local package/independent-build evidence and initial failures remain in separate evidence. Final package correspondence, PR/main CI and public asset checks follow the [release gate](release-1.9.0.2-gate.md); this document does not mark them passed in advance. The release page determines actual publication status. Internal key dispatch and geometry checks do not establish physical-device input coverage; hardware, latency, long play and other PCs remain unverified. The fixed baseline remains 1.9.0.

## Follow-up after 1.9.0.1

This release is limited to preview gain, Hi-Speed bindings, native dialogs, BMS timing and XInput buttons. Physical-controller and long-play checks remain separate; online timing-version-2 support is a future task. Keep affected new records local until the web service distinguishes the schedules. Preserve the fixed 1.9.0 baseline and the existing plugin/restructuring boundaries.

## 1.9.0 stabilization scope and follow-up

1.9.0 promotes the reviewed candidate and explicitly updates the fixed baseline. Its scope is volume/settings/UI/credits/EX-R6/upload diagnostics, not universal latency resolution. The former plugin prerequisite in the 1.8.8–1.8.9 plan is superseded by this requested release scope. Plugin runtime/API/SDK and major GameSession extraction remain unimplemented follow-up work. Future plugins should start with read-only extensions without taking ownership of input, audio, judgement or official records. Continue focused bug, compatibility and security fixes; future baseline changes require another explicit request and synchronized evidence.


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
