> **2026-10-22 · 1.9.9 · NK2 E / 16K submissions:** [1.9.9](release-1.9.9.en.md) · [Verification](release-1.9.9-gate.md). Baseline **1.9.6**. 4/5/6/7/8/9/10/12/14/16K. Earlier dated entries below are historical.

> **2026-10-21 · 1.9.8.5 release scope:** Adds BGA playback and black-key transparency fixes, wrapping song selection, Library/clear labels, decimal timing, scratch-free 7K and secondary-key ownership to the user-selected 1.9.8. The fixed general baseline remains **1.9.6**. Earlier notices retain their historical scope; see [1.9.8.5](release-1.9.8.5.en.md) for behavior and compatibility and [release gates](release-1.9.8.5-gate.md) for verification. Publication and downloads follow the [release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5).

> **2026-10-19 · 1.9.8:** Based on 1.9.7, exposes X immediately before Y for Judgement, Combo, the FAST/SLOW indicator and its bar, and adds lane-divider brightness from 0–100% in 1% steps. Includes 1.9.7 Common Keys, larger colored tabs, fresh per-play seeds and scene-music reference gain; the fixed general baseline remains 1.9.6. [1.9.8](release-1.9.8.en.md) · [X/Y](skin-xy-20261019.en.md) · [Baseline 1.9.6](baseline-1.9.6.en.md) · [Verification](release-1.9.8-gate.md).

Earlier versioned and dated notices below retain their historical context. The 1.9.8.5 documents define the current scope; publication and downloads follow the version-specific release page.

> **2026-10-19 · 1.9.7:** Adds Common Keys, larger mode tabs with lobby colors, colored FAST/SLOW labels, 1% adjustments, Game Mode Settings/Normal labels and a fresh seed for every solo play. The version-specific release page determines publication and downloads. Earlier dated notices are historical. [1.9.7](release-1.9.7.en.md) · [Baseline 1.9.6](baseline-1.9.6.en.md) · [Verification](release-1.9.7-gate.md).

**2026-10-18 · 1.9.6:** [Fixed judgement, skin tabs/search and windowed drops](release-1.9.6.en.md) · [Verification scope](release-1.9.6-gate.md). Sites accepts only new standard Online R9/R10 submissions; old boards and records remain legacy read-only. Unsupported separate Ranked rules and Guest/custom exclusions remain.

[Pinned timing table](judgement-baseline-20261018.en.md): Normal PG22/19/15/10ms and exact existing GR/GD/BAD remain. Hard uses the complete Normal NM row and EX the complete Normal VHD row for every chart RANK, without integer-ms rounding. The general source baseline remains **1.9.0**.

[Skin UI behavior and shortcuts](skin-ui-followup-20261018.en.md). Earlier version/local-candidate entries below are historical; 1.9.6 documents define the current scope.

**2026-10-18 · 1.9.5.3:** [Skin settings cleanup and search/input fixes](release-1.9.5.3.en.md). Select the edit lane with numbered preview buttons. UI font, font file and Gameplay Cursor live in Profile. Scratch Position appears only in Original runtime key mode and is adjustable for a 7+1 layout.

**2026-10-17 · 1.9.5.2:** [Note and field size down to10%](release-1.9.5.2.en.md). Includes GOMazk’s contribution (PR #102). Note & Field Size expands from50–140% to10–140%. The100% default, saved values and per-key-mode settings are preserved; menus, gameplay, ghost and preview use the same lower limit. Judgement, score, input and audio calculations and the1.9.5.1 350ms FAST/SLOW/file-picker behavior remain unchanged.

**2026-10-17 · 1.9.5.1:** [Judgement line, FAST/SLOW and file pickers](release-1.9.5.1.en.md). FAST/SLOW remains for 0.35 seconds from the last hit outside the maximum window. A maximum judgement neither clears it immediately nor extends its lifetime. Another outside-window hit replaces it immediately. The Always scale and 0.7-second dot/barcode lifetime remain separate.

**2026-10-17 · 1.9.5:** Menu/result music follows the selected sound output. Indicator ranges and markers use blue FAST / green maximum judgement / red LATE. FAST/SLOW updates on maximum hits too. Lobby order is OFF → Mirror → Random → S-Random → Random (Scratch Fixed) → OFF. [Scene audio routing and timing indicators](release-1.9.5.en.md).

**2026-10-16 · 1.9.3 release scope:** Adds Empty Poor accuracy exclusion (R7/R8), the BMS rank badge, in-game levels and measure lines, retained FAST/SLOW, Safe indexing defaults, general residual/image ONNX and supported external Safetensors conversion. Runtime/source packages contain no BGA weights; OFF/empty-path defaults remain. Guest upload exclusion, fixed grades and baseline 1.9.0 are preserved. [Changes and compatibility](release-1.9.3.en.md) · [Verification gate](release-1.9.3-gate.md). The [1.9.3 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.3) determines publication and downloads; dated entries below retain their historical scope.

> Previous 1.9.2: [Guest/Online, original-resolution upscaling and UI changes](release-1.9.2.en.md).

> 2026-10-16: [FFmpeg deblock and external ONNX x2](deblock-lunasr-20261016.en.md)

> 2026-10-16: [Restore BGA at its original resolution before fitting](native-upscale-20261016.en.md)

# TenRiff Docs Map

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

**2026-10-13 · 1.8.9.5:** R5 processes LN tails and automatic judgements in time order, separates judgement from audio write-ahead, and aligns input/display timing with exact BPM/STOP scroll positions. Restores dark lobby cards with colored icons and moves Master Volume first. R1–R4 and the fixed 1.8.7 baseline are preserved. [Scope and verification](release-1.8.9.5-gate.md).

**Fixed baseline: [1.9.0](baseline-1.9.0.en.md)** (explicit user request, 2026-10-14). Use Git tag `1.9.0`; do not advance automatically with `main` or `latest`. Preserve the 1.8.7 baseline and previous releases as history.

**2026-10-13 · 1.8.9.4:** Rearranges the large result rank and score, shows detail accuracy and distinguishes four Gauge Shift CLEAR colors. BGM/keysound volume move to the top of Audio with 100% defaults for new settings. Includes Korean skin-label revisions, explicit obj credits, BPM! changes and correction of duplicate identical audio at the same chart time. Author uploads are optional and existing record identities are retained. [Changes, compatibility and validation (Korean)](release-1.8.9.4-gate.md). Verification and game/web publication are tracked separately; the fixed baseline remains 1.8.7.

**2026-10-13 · 1.8.9.3:** Refines result typography, colored lobby cards with subtle bloom, skin-colored key bindings and song-list level colors. Removes indicator backings and the target-gap/visual-preset controls, placing Target Lane directly above Note Color. [Changes and verification scope (Korean)](release-1.8.9.3-gate.md). Fixed baseline remains 1.8.7; 1.8.9.2 BMS correction and replay compatibility are retained.

**2026-10-13 · 1.8.9.2:** Coalesces a BMS tap and long-note head at the same lane/time into one playable hold before Native play or conversion. Preserves original files, hold spans and keysounds. Old replays retain their original layout. [Behavior and compatibility limits (Korean)](release-1.8.9.2-gate.md). The fixed baseline remains 1.8.7.

**2026-10-13 · 1.8.9.1:** Adds sequential key binding, converter keysound/hold preservation, FAST/SLOW/timing backing controls, larger gauges, preview gear dragging and text-only judgements. Existing replays retain their original conversion. [Behavior, verification and compatibility limits (Korean)](release-1.8.9.1-gate.md). The fixed baseline remains 1.8.7.

**2026-10-12 · 1.8.9:** Adds larger lobby BEST numbers, key-mode colors and a white shared frame; Studio information at the upper right, score at the lower left and a horizontal riff map at the lower right; and larger result text, analysis tabs and centered actions. S+ color/font are preserved. Judge Easy/Hard selections now reach new sessions. See [behavior, verification and remaining scope](release-1.8.9-gate.md) and [Judge cause and compatibility](judge-session-handoff-1.8.9.ko.md) (Korean).

**2026-10-12 · 1.8.8:** Combines result analysis, free window sizing and F9 PNG hints, long LN Mix, readability updates, table URL/filter fixes, skin lane shortcuts, preserved keysounds during key-count reduction, and solo start/instant-retry controls. See the [1.8.8 release gate](release-1.8.8-gate.md) for current validation, packages and publication status. The fixed baseline remains 1.8.7.

**Analysis and extension plans:** Separate lane distributions, time-based accuracy and tap/LN-head/release/hold-failure events; unmeasurable misses are never zero-ms samples. Long LN Mix v2 keeps local history/replay only and is excluded from legacy BEST and Sites uploads. Plugins remain future work, outside 1.9.0. [Analysis](result-analysis-1.8.8.ko.md), [display/menu/keysound changes](release-feedback-1.8.8.ko.md), [start/retry controls](gameplay-start-controls.ko.md) (Korean).

**2026-10-09 · TenRiff 1.8.7:** Improves input calibration and chart-loading audio, adds LR2 solo Studio Deck, 750ms FAST/SLOW retention, key reassignment, consolidated Options and complete song-title fitting. [1.8.7 release](release-1.8.7-gate.md).

**2026-10-08 · TenRiff 1.8.6:** Adds the Studio Deck HUD for Native solo play. Left and right Shift work as play keys; F5/F6 halve/double Hi-Speed and F11 opens chat. Classic HUD remains selectable. [1.8.6 release](release-1.8.6-gate.md).

**2026-10-06 · TenRiff 1.8.5:** Batches supported note images in drawing order during VSync-OFF gameplay to reduce CPU submission work. Unsupported skins/long notes and VSync-ON retain the existing path. Lobby/menu behavior and R1–R4 judgement/replay rules are preserved. [1.8.5 release](release-1.8.5-gate.md).

**2026-10-05 · TenRiff 1.8.42:** Fixes extra render waiting when a dense frame slightly exceeds its budget, and skips performance-history collection/sorting while the overlay is off. Note visuals/count and R1–R4 judgement/replay rules are preserved. [1.8.42 release](release-1.8.42-gate.md).

**2026-10-05 · TenRiff 1.8.41:** Retunes Judge Hard to EASY-baseline PG/GR/GD ±17.5/55.714286/98.571429ms. BAD is fixed across every BMS RANK at Hard ±225ms / Easy ±210ms. New records use R4; R1/R2/R3 replays and existing web records are preserved. [1.8.41 release](release-1.8.41-gate.md).

**2026-10-05 · TenRiff 1.8.4:** Improves pause mouse controls and settings clicks; adds live pacemaker feedback, practice Auto Scratch and portable full-profile settings. Fixes player field sizing in ghost battle, LN opacity, HUD scanning and the frame-queue limit. [1.8.4 release](release-1.8.4-gate.md).

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

If you have already read the root [`README.en.md`](../README.en.md), this document is the next-step index into the detailed documentation. It includes both design documents and the current-state document, so when you need to gather context quickly, the following order is the most efficient.

This codebase should also be read as a `vibe coding` work that grew through rapid iteration and experimentation.

## Documentation authority and recent changes

When current-behavior descriptions conflict, check **implementation and regression tests at the same commit** first, then correct `current-state`, `config` and their translations together. `baseline-*` and `release-*` describe their named versions; early designs and roadmap proposals are not implementation or performance guarantees.

- [Current timing windows, RANK tables and LN rules (Korean)](judgement-windows.md)
- [1.8.3 release](release-1.8.3-gate.md)
- [1.8.3 audio / LN / RANK](audio-ln-rank.ko.md)
- [Skin timing controls](timing-feedback-controls.ko.md)
- [Note fog / song ending / pause resume](skin-fade-audio-resume.ko.md)
- [1.8.2 release](release-1.8.2-gate.md)
- [1.8.1 release](release-1.8.1-gate.md)
- [1.8.1 Studio](menu-studio-1.8.1.ko.md)
- [1.8.0](release-1.8.0-gate.md)
- [1.7.10](release-1.7.10-gate.md)
- [1.7.9](release-1.7.9-gate.md)
- [1.7.8](release-1.7.8-gate.md)
- [1.7.2 history](release-1.7.2-gate.md)
- [1.7.1 history](release-1.7.1-gate.md)
- [UI](menu-visual-polish.md)
- [Gameplay / Multiplayer](gameplay-polish-followup.md)

## Recommended Reading Order
1. `docs/current-state.en.md`
   - Current product state, core subsystems, validated commands, and remaining manual verification items
2. [`docs/baseline-1.9.0.en.md`](baseline-1.9.0.en.md)
   - The `1.9.0 fixed stable baseline` baseline document that defines what the current work should be stacked on top of
3. `docs/gameplay-guide.en.md`
   - How to start playing, choose songs, handle controls, and understand the HUD, judgements, and result screen from a practical player perspective
4. `docs/config.en.md`
   - Actual config / profile / keymap structure
5. `docs/localization.en.md`
   - Current English/Korean/Japanese UI structure and the file boundaries to touch when adding more languages
6. `docs/menu.en.md`
   - Menu / state machine / song-select flow
7. `docs/core-loop.en.md`
   - Play loop and data flow
8. `docs/roadmap.en.md`
   - Long-term direction for future work
9. `docs/developer-extension-guide.en.md`
   - Developer guide explaining where to extend modes/mods, runtime migration, replay/result handling, and maintenance workflows

## Which Docs Are Source Of Truth
- `docs/current-state.en.md`
  - Summary of the current implementation state
- [`docs/baseline-1.9.0.en.md`](baseline-1.9.0.en.md)
  - The `1.9.0 fixed stable baseline` baseline document that follow-up work should preserve
- `docs/config.en.md`
  - Based on the actual `config/config.json`, `profiles/<name>/config.json`, and `keymap.json`

## Historical / Design Docs
- `docs/menu.en.md`
  - Menu / state machine / low-latency direction design document
- `docs/core-loop.en.md`
  - Early design and data-flow explanation for the play loop
- `docs/localization.en.md`
  - Reference for the current UI localization structure and future language expansion
- `docs/latency.en.md`, `docs/modes.en.md`, `docs/gap-analysis.en.md`, `docs/roadmap.en.md`
  - Feature-specific design, analysis, and long-term direction documents
- `docs/developer-extension-guide.en.md`
  - Maintenance guide for mode/mod extension work in the current codebase

## Translation Coverage
- The root `README.md` also has [`README.en.md`](../README.en.md), [`README.zh-CN.md`](../README.zh-CN.md), and [`README.ja.md`](../README.ja.md).
- Major `docs/` documents are translated side-by-side as `.en.md`, `.zh-CN.md`, and `.ja.md` files.
- If a translation and the Korean original disagree, use the current code first, then `docs/current-state.md`, then `docs/config.md`.

## Acknowledgements
- Thanks to OpenAI Codex, ChatGPT, Claude Code, Gemini, and the guest testers who helped validate the project.

## Practical Rule
- When checking current behavior, start with `docs/current-state.en.md`.
- When deciding what baseline to build on, read [`docs/baseline-1.9.0.en.md`](baseline-1.9.0.en.md) together with it.
- Older design documents and current code can differ, so if there is a conflict, interpret things in the order of current code, then `docs/current-state.en.md`, then `docs/config.en.md`.

## Audio and Play Settings

See [native ASIO](asio-audio.md), [reference BPM](reference-bpm.md), [portable skin presets](skin-presets.md), [source and table management](library-management.md), and [LR2 gauge comparison](lr2-gauge-audit.ko.md).
