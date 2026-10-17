**2026-10-17 · 1.9.5:** Menu/result music follows the selected sound output. Indicator ranges and markers use blue FAST / green maximum judgement / red LATE. FAST/SLOW updates on maximum hits too. Lobby order is OFF → Mirror → Random → S-Random → Random (Scratch Fixed) → OFF. [Scene audio routing and timing indicators](release-1.9.5.en.md).

**2026-10-17 · 1.9.3 local:** [Indicators, output devices and profile shortcuts](input-feedback-20261017.en.md).

# TenRiff Config Schema (current)

**Current 1.9.3 contract:** Safe indexing defaults, Empty Poor accuracy exclusion with R7/R8, general ONNX and supported Safetensors apply. Production Sites v19 accepts eligible fresh Online records; Guest/unmarked historical records remain permanently excluded. No BGA weights ship; OFF/empty-path defaults and baseline1.9.0 remain. The [1.9.3 scope](release-1.9.3.en.md) supersedes dated history below.

**2026-10-15 · 1.9.1:** The user-authorized 1.9.1 includes the earlier volume/input/outro fixes, BMS TOTAL/RANDOM compatibility, Result colors and the 10 mark. A Home language button cycles EN → KO → JA → ZH and saves the choice. New BMS gauge/branch records remain local until server verification supports them. The fixed baseline stays 1.9.0; earlier releases, installations and profiles are preserved. Dated local notes below retain their historical no-version-bump/unpublished status; this 1.9.1 scope supersedes that status. [Changes and compatibility](feedback-1.9.1.en.md) · [Release verification gate](release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL / RANDOM / visual follow-up — unreleased, local build and CTest regression passed:** This local follow-up builds on 1.9.0.2. Result levels use the song lobby’s red level text and key-mode symbol colors, and the logo adopts the supplied 10-shaped mark. New ordinary BMS plays use LR2-based recovery/damage with #TOTAL and note count; authored #RANDOM branches receive a seed independent of lane randomization. Historical replays retain their original behavior. The earlier unified-volume changes remain included. Published 1.9.0.2, the fixed 1.9.0 baseline and the version number are unchanged. [Formulas, branching, compatibility and verification scope](bms-total-random-followup.en.md).

**2026-10-15 · BMS volume follow-up — unreleased, local build and CTest regression passed:** This local follow-up to 1.9.0.2 replaces separate background/keysound controls with one BMS performance volume control (Korean: **BMS 연주 볼륨**). Its new default is 20%; saved values are retained. Master/TenRiff music defaults remain 10%/100%, the remaining volumes use 1 percentage-point steps, and lobby preview keeps ×0.75 attenuation. The version number and fixed 1.9.0 baseline are unchanged. Dated entries below retain their original context.

1.9.1 shows difficulty-table loading status and changes debounce UI steps to 1ms, with a 20ms maximum and 0ms default. The old automatic 5→8ms rewrite is retired, preserving saved 5ms and 8ms values. The Korean skin label becomes `LN 끝 줄임`; appearance and judgement behavior are unchanged. In ordinary single-player with Listen to End enabled, `Esc` skips the outro directly to results without opening Pause once the final note and LN tail have been processed.

**2026-10-15 · 1.9.0.2 release candidate — publication verification gates:** Based on published 1.9.0.1, this candidate makes Options green, shows green ⑤ only for the selected built-in 5K Aery table, colors registered 4/6/8/10-key table symbols, and uses red levels/white scores, adds `lv:` searches, changes the new debounce default to 0ms, and uses 1% audio-volume steps. Move Song Select sort/filter and table selectors beside Song Library and increase the album cover height. Master volume still defaults to 10%; the fixed baseline remains 1.9.0. [Changes and usage](feedback-1.9.0.2.en.md). The release page determines publication status and available downloads. [Release scope and verification](release-1.9.0.2-gate.md).

**2026-10-15 · 1.9.0.1:** Includes 10% default master volume and aligned lobby icon centers. Preview ×0.75, profile-assigned F5/F6 -10/+10, Xbox/XInput button input, single modal file pickers, and corrected BMS BPM/STOP order with historical replay compatibility. The fixed baseline remains 1.9.0. [Changes](feedback-20261015.en.md) · [Release verification scope](release-1.9.0.1-gate.md).

**1.8.8:** Includes result analysis, freely resizable windows and direct width/height entry, result `F9` PNG hints, long LN Mix, and menu, keysound and start-control improvements. See the [release guide (Korean)](release-1.8.8-gate.md) for current verification and delivery status. The user-pinned baseline remains 1.8.7; its existing tag and release files are preserved.

Reference: [Config.h](../src/config/Config.h), [Config.cpp](../src/config/Config.cpp), [default JSON](../config/config.json), and [keymaps](../src/config/Keymap.cpp). Missing values use code defaults; bounds below describe load/save normalization.

This document summarizes the configuration structure that is actually active today, based on `config/config.json`, `profiles/<name>/config.json`, and `profiles/<name>/keymap.json`.

## Load Order
1. code defaults
2. global config: `config/config.json`
3. profile config: `profiles/<name>/config.json`
4. CLI
5. menu/runtime save

If a profile does not exist, it is created automatically on first launch.

## `config.json`

### `audio`

**Earlier 1.9.0.2 guidance (history before volume unification):**

The 1.9.0.2 candidate adjusts all five audio UI volumes—master, BMS song, TenRiff music, BGM and keysounds—in **1 percentage-point steps (0.01 in saved values)**. The first three range from 0–100%; BGM and keysounds range from 0–200%. New master volume still defaults to 10% and the others to 100%; explicitly saved volumes are retained.

**1.9.1 audio:** The audio UI now has three volume controls: master, BMS performance and TenRiff music. All use **1 percentage-point steps (0.01 in saved values)** over 0–100%, with new defaults of 10%/20%/100%; explicitly saved values are retained. Existing `gameplay_volume` is the only BMS-specific volume multiplier; master gain still applies. `bgm_volume` and `keysound_volume` remain readable/writable for compatibility, but have no individual UI and do not affect gain in play, any preview or editor path. Lobby preview retains its additional ×0.75 attenuation. Listening, physical-device and long-play validation remain unverified.

- `play_to_end` (bool; default `true`): Listen to the remaining audio after the final note and LN tail have been processed. During this outro, ordinary single-player `Esc` goes directly to normal results without opening Pause, including autoplay and replay. Existing Session Mix/multiplayer Esc policies remain. When false, finish normally after `ui.result_tail_ms` without waiting for the outro. Judgement, score and replay rules are unchanged.

- `backend` (string)
  - `wasapi | asio`; default `wasapi`. Gameplay, previews, menu and result music use the selected WASAPI endpoint or ASIO driver.
- `device_id` (string)
  - Empty by default (system default). Sound options select the WASAPI endpoint for gameplay, BMS previews, menu and result music. Menus use shared mode; gameplay retains its exclusive preference. Missing selected endpoints never silently fall back. The short settings-adjustment click retains Windows-default output.
- `asio_driver` (string)
  - CLSID of an installed 64-bit ASIO driver. Empty means Auto, the first driver sorted by name. Select in Audio settings; F5 refreshes the list.
- `rate` (int)
  - default sample rate
- `frames` (int)
  - buffer frames
- `periods` (int)
  - number of periods
- `exclusive` (bool)
  - whether to try WASAPI exclusive mode
- `use_mmcss` (bool)
- `affinity` (int)
  - `-1` means default
- `preset` (string)
  - `basic | high`
- `bms_keysound_policy` (string)
  - `follow | autoplay | ignore`
- `background_sound_enabled` (bool)
  - controls menu, result, and song-preview music only; gameplay chart BGM remains audible
- `title_music` (string)
  - `none | default | random_bms | last_played`; default `default`. Selects no music, the bundled theme, a random loaded BMS, or the last played chart for Title and Quick Setup. Unavailable chart audio falls back to the default theme. BMS audio includes keysounds and is prepared asynchronously, capped at five minutes and looped.
- `volume` (double)
  - Master volume. 0–1, new default 0.10 (10%); explicitly saved values are retained.
- `gameplay_volume` (double)
  - BMS performance volume (Korean: BMS 연주 볼륨), jointly controlling background audio, keysounds and previews. 0–1, new default 0.20 (20%); explicitly saved values are retained. The field name and existing UI-effect gain are unchanged.
- `menu_music_volume` (double)
  - TenRiff menu/result music only. 0–1, default 1.0. The reference gain is 10% of source, 75% quieter than the 40% reference in 1.8.9.7. Independent of BMS songs; not a Windows volume-curve emulation.
- `bgm_volume` (double)
  - Legacy compatibility field. 0–2, default 1.0; existing values are still loaded/saved, but have no individual UI and are ignored by play, preview and editor gain calculations.
- `normalize_audio` (bool)
  - In-game RMS loudness control, default false. ON retains RMS → soft limiter → master; OFF applies linear master gain and only guards final output overflow. Menu music and song previews are unchanged.
- `keysound_volume` (double)
  - Legacy compatibility field. 0–2, default 1.0; existing values are still loaded/saved, but have no individual UI and are ignored by play, preview and editor gain calculations.

See [ASIO setup](asio-audio.md). ASIO holds the selected sample rate fixed and resamples chart audio. `frames` is a request negotiated to a legal driver size. Presets do not overwrite ASIO frames; `exclusive` and `periods` apply to WASAPI. ASIO failures do not fall back to WASAPI automatically.

### `input`

Version 1.8.9.7 requests MMCSS `Games`/High and disables execution-speed power throttling on the input thread only. API failures are logged at startup and retain the existing input path. Cleanup restores power policy and reverts MMCSS on the owning thread, including RawInput-to-Polling fallback. No global power plan, registry or audio priority changes. Physical latency improvement requires a separate play comparison.

- `backend` (string)
  - `polling | rawinput`
  - defaults to `rawinput` on the current `1.7.2` release line
  - selectable per profile under `Options -> Input Settings -> Backend` or `Options -> Profile Setup -> Input Backend`
  - runtime fallback never rewrites the saved value to `polling`
  - a confirmed RawInput startup failure, registration-target loss, or message-window exit latches Polling across menu and subsequent gameplay sessions for the current app run
  - restarting the app or explicitly changing Backend in Input Settings retries the selected backend
- `rawinput` (bool)
  - convenience boolean persisted alongside `backend`
  - when `true`, menu/gameplay prefer RawInput
  - gameplay continuously shadows bound note/control keys with polling in the same `InputThread`
- `use_qpc` (bool)
- `grab` (bool)
  - currently a Linux-preview-oriented setting
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - sampling cadence for the Polling backend and the gameplay polling shadow
  - default is `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - compatibility field kept in the input config
  - the current `1.7.2` runtime no longer drives a separate audio-thread judgement sub-step loop from this value
  - default is `4000` (`0.25ms`)
- `debounce_ms` (double)
  - `0ms` disables time-based repress suppression and preserves existing fast repeats. Duplicate same-state events are still removed.
  - Above `0ms`, a Press from the same key and physical input source is suppressed if it occurs less than the configured interval after Release. A Press exactly at the boundary is accepted.
  - Real Release events are neither delayed nor dropped by the time filter. Intentional fast represses can also be suppressed inside the interval.
  - RawInput/Polling mirror events cannot bypass suppression; physical keyboards identified by RawInput are tracked independently. Polling itself does not distinguish physical device IDs. XInput keeps its separate GamepadTracker path and does not use this keyboard time filter.
  - clamped to `0..20`; the UI adjusts in 1ms steps
  - default is `0ms`; saved values within the range are retained
  - retired legacy rule: the value-based `5ms → 8ms` rewrite. It is removed so an explicit 5ms choice in the new UI stays 5ms; saved 5ms and 8ms values are each retained

### `judge`

The 1.9.0 release selects Normal / Hard / EX under `Options > Key Mode Settings > Judge Window`. Easy is removed from new selections but retained for historical replays. The session-only choice is not profile-persisted and resets to Normal on restart. See [EX timing and compatibility](judge-ex-1.9.0.ko.md) (Korean).

[Current timing windows, RANK tables and LN rules (Korean)](judgement-windows.md)

- BMS `#RANK` selects chart timing: `3/EASY` PG21ms, `2/NORMAL` PG18ms, `1/HARD` PG15ms, `0/VERYHARD` PG8ms. EASY keeps GR/GD/BAD at 65/115/210ms; the other ranks scale these by 18/21, 15/21 and 8/21. Missing/unsupported headers use EASY. Judge Hard/EX mods apply afterward. RANK does not change `hold_grace`/`hold_break` settings or automatic-miss deadlines; tail PG/GR/GD boundaries do follow RANK.
- `pg`, `gr`, `gd`, `bd` (double, ms)
- default `pg / gr / gd` values are `21ms / 65ms / 115ms`
- default `bd` is `210ms`
- `Judge Easy` (historical replays only) scales RANK-adjusted PG/GR/GD and hold tolerances by `1.35x`. EASY PG/GR/GD=`28.35/87.75/155.25ms`; BAD is fixed at `210ms` for every RANK; mask is unchanged
- `Judge Hard` uses EASY PG/GR/GD=`17.5/55.714286/98.571429ms`: multiply PG by `17.5/21` and GR/GD by `18/21` after RANK. BAD is fixed at `225ms` for every RANK; hold tolerances are unchanged
- `Judge EX` uses EASY PG/GR/GD=`14/44.571429/78.857143ms` (80% of Hard), retaining BMS RANK scaling. BAD is fixed at 250ms; the separate automatic-miss deadline remains 340ms and the score factor remains Hard's 1.10.
- `indirect_miss` (double, ms)
  - current profiles save and normalize this value to `340ms`, independently of the BAD hit window
  - default Normal/Easy/Hard automatically miss an unplayed note once it is more than `340ms` late. Normal/Easy record BAD; Hard records a combo-breaking indirect `POOR` / OD8 `MISS`
  - a late press outside BAD but before automatic timeout misses the expired note, then checks the next note; it cannot score a BAD hit outside the hit window
- New Normal/Hard uses `tenriff-native-score-v2-ruleset-7`; EX uses `tenriff-native-score-v2-ruleset-8`. Only Empty Poor accuracy contribution changes, not numerical timing windows. Historical R1–R6/custom records retain their policy; new custom paths use the corresponding 7/8 IDs.
- R5 keeps the numerical timing windows, processes automatic events chronologically, and preserves R1–R4 processing order. [Timing fix](release-1.8.9.5-gate.md).
- `hold_grace` (double, ms; default `80ms`)
  - retained for configuration compatibility and as the lower bound of `hold_break`; not the current native tail PG/GR boundary
- `hold_break` (double, ms; default `200ms`)
  - timeout after a CN tail when it remains held, ending in BAD; `270ms` with Judge Easy, unchanged by RANK
  - internally always kept at or above `hold_grace`
- LN release immediately uses the current PG/GR/GD windows, with BAD outside GD. Standard LN completes automatically if held to the end. `No LN Release` does not ignore an early drop
- `mask` (double, ms; default `30ms`)
  - temporarily ignores additional presses on a masked lane; unchanged by RANK or Judge mods

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)
- Reference BPM is the tempo with the longest accumulated running time; repeated sections add together and STOP waits are excluded. This fixes the Hi-Speed reference while explicit `#SCROLL`, stops and reverse motion still apply. See [reference BPM](reference-bpm.md).

### `gauge`

**1.9.1 BMS behavior:** New ordinary BMS plays use `TOTAL / N` recovery for Normal/Easy, and fixed recovery plus TOTAL/note-count damage correction for Hard/EX. `N` counts valid current note heads, with each LN counted once. Hard judgement damage is multiplied by 0.6 below 32%. TenRiff retains its 100% start, zero failure, gauge shifts, judgement classification, 0.5 LN head/tail weights and authored mine damage. Courses retain their course gauge; peer battle retains v1. Existing `gauge.delta` values remain for historical replay compatibility and do not tune the new BMS v2 calculation.

Gauge Shift is always active. `mode.gauge` selects the starting tier: `ex_hard / hard / normal / easy`, from EX down to Easy. The selected tier and every lower tier are simulated independently from 100%; a failed tier yields to the next surviving tier. Gauge failure occurs only when all eligible tiers fail. Legacy `shift` means an EX start.

Grade/Session Mix uses a separate LR2-reference gauge, carried between songs, with indirect misses and failure below 2% HP. Normal long notes apply gauge once on completion/release. Ordinary `gauge.delta` settings do not apply to courses. See [LR2 gauge comparison and limits](lr2-gauge-audit.ko.md).

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - the default is `borderless`; it is also the recommended mode for external overlays such as Discord, OBS, and Game Bar
  - `windowed` has a title bar and supports moving/resizing by dragging its borders. Ending a resize drag saves the actual client size to the profile; maximize/restore and programmatic resizing do not overwrite the selected value
  - `fullscreen` is DXGI exclusive fullscreen, where the current Discord Game Overlay is not displayed
- `resolution` (string)
  - `native`, legacy aliases `720p | 1080p | qhd`, or `widthxheight` (e.g. `1600x900`, `1366x768`, `1280x800`, `3440x1440`). Each axis accepts 320–8192px.
  - Graphics Settings combines common sizes with the current monitor's display modes. Press `F5` to refresh the list. Explicit custom sizes survive saving and restarting.
  - Resolution `+/-` keeps cycling presets. `Enter` or clicking the value opens Width/Height drafts; type with the keyboard's top-row digits or numpad. `Tab`/arrows switch fields, `Enter` applies, `Esc` cancels, `Backspace` erases and `Delete` clears the field. Each axis must be an integer from 320 to 8192; empty/out-of-range values show an error and preserve the setting. Other characters, paste and IME input are not used.
  - Content and click mapping retain the 1920×1080 aspect, with margins for other window shapes. Oversized requests retain the selected dimensions while the actual window fits the work area. Fitting is normally proportional, but the minimum actual client size of 320px per axis can alter extreme window aspect ratios.
  - An unsupported exclusive-fullscreen size falls back to windowed mode and saves the actual size/mode. Existing profile tokens and defaults stay unchanged. See [new-candidate window/transition/save verification (Korean)](release-conditions-1.8.8.ko.md#창-크기와-직접-해상도-입력).
  - Skin Settings uses a uniformly scaled view of the actual gameplay renderer, including field movement, lane/note/gear sizes, judgement/combo positions and skin fonts.
- `vsync` (bool)
- `refresh_hz` (int)
  - `-1` selects `Match Display`; `0` is the profile-compatible `Unlimited` selector with an actual 1500 FPS maximum
  - legacy fixed numeric limits migrate to `-1`
  - with `vsync=false`, Unlimited paces gameplay at no more than 1500 FPS while menu rendering keeps a 300 FPS cap
  - when `vsync=true`, the present refresh follows the active monitor Hz and render pacing targets `monitor_hz * 2` (`1050` clamp)
- `performance_overlay` (bool)
  - defaults to `false`; it occupies the top-right corner and can overlap a Discord Voice widget placed there
  - in-game frame pacing measures intervals between successful DXGI `Present()` completions; HUD update cadence is not used as an FPS sample
- `bga_enabled` (bool)
  - defaults to `true`; `false` disables gameplay image/video BGA plus its decoder and upscaler work
  - Song Select background previews remain visible because they are a separate feature
- `background_upscale_mode` (string)
  - `onnx | off`; legacy `lunasr` maps to `onnx`; default `off`. Enabling requires the high-spec warning, without an automatic performance benchmark
  - original → FFmpeg RGB deblock → model-native x1–x4 inference → residual composition/complete image → display fit/crop; no pre-resizing to 960×540
  - dynamic models use the whole original; fixed models use padding/context tiles. Source/output axis bounds4096/8192; cache3 images/96MiB per instance. Video AI remains disabled
- `background_upscale_model_path` (string)
  - default empty; preserve explicitly saved paths. Select `ONNX / Safetensors` or drop `.onnx`/`.safetensors`; selection resets output Auto without enabling inference
  - no BGA weights in runtime/source delivery. Absolute or executable/working-directory-relative paths are accepted
  - one RGB NCHW FP32/FP16 input/output, arbitrary names, batch1/channels3, both spatial axes fixed or dynamic, uniform integer x1–x4. Input0..1; internal INT8 QDQ with float boundaries is supported. Reject partial dynamics, NHWC and multiple inputs/outputs
  - Safetensors needs a supported LunaSR2 or exportable installed Spandrel architecture and external Python+torch/safetensors/onnx/onnxruntime. No auto-install/download. Failure retains native rendering and logs the cause
  - [Contract, setup and limitations](../tools/onnx_upscaler/README.md). Model-specific size/operator/memory/quality constraints remain
- `background_upscale_output_mode` (string)
  - `auto | residual | image`, default `auto`. Explicit choices override detection; model changes reset Auto and replace sessions/caches
  - Auto uses `tenriff.output_mode` metadata, otherwise the legacy `rgb_lr`/`rgb_residual_x2` pair means residual and other names mean complete image. Select unmarked residual models explicitly; Auto does not infer semantics
  - residual adds output to bilinear enlargement of the deblocked original; image uses output RGB directly. Both clamp0..1 and preserve original alpha
- `background_upscale_prefer_npu` (bool)
  - defaults to `false`; the default path requests a high-performance DirectX GPU
  - experimental `Low-Power DirectX` in Graphics Settings requests `DirectXMinPower`
  - the legacy WinML path neither explicitly selects nor verifies an NPU, so this option is not evidence of NPU execution
  - failure to create the low-power session falls back to the existing high-performance DirectX path

### `mode`
The chart loader and indexer are limited to BMS-family files (`.bms/.bme/.bml/.pms`). Legacy `enable_osu_charts` and `format` values are ignored when read and are not saved again.

- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
  - `none` means using the chart's original key count as-is
- `key_conversion_algorithm` (string)
  - `krrcream | nk2 | nk3`
  - select `Krrcream`, `KeyWeaver nK2`, or `KeyWeaver NK3 ONNX` from in-game `Mode Settings > Key Converter`
  - defaults to `krrcream`; NK3 also remasters unchanged key counts and its default `AUTO` backend prefers ncnn Vulkan
  - Krrcream only remaps source notes into target lanes
  - when expanding the key count, nK2 creates safe support notes directly in the target layout during conversion instead of pre-adding notes to the source
  - NK3 always uses P64 and the host beam safety solver, adding the generalized MLP only for non-10K sources converted to 10K; select `TENRIFF_NK3_BACKEND=AUTO|VULKAN|NCNN_CPU|OPENVINO`, where `AUTO` tries ncnn Vulkan first, and use `TENRIFF_NK3_VULKAN_DEVICE=<index>` when selecting among multiple Vulkan GPUs
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`; defaults to `native`
  - selects nK2 `Native (12%)`, `Transform (35%)` or `Remaster (65%)`; the setting row is locked for Krrcream
  - `Remaster` raises the budget while keeping the source placement, and fills LN sections with holds of the same length
  - all three are caps; how much actually lands depends on the source density and the safety windows
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
  - Default `ex_hard` (EX). Explicit saved starting tiers are preserved.
- `random` (string)
  - `off | mirror | rr | frns | sr` (`fr` = `frns`)
- `random_seed` (int)
  - fixed seed for RR/SR, forced key-mode conversion, and LN Mix selection; ordinary Random creates a fresh session seed per play and records the actual value in the replay
- `mods` (string array)
  - Note Structure accepts one of `full_long_notes`, `ln_mix_10` through `ln_mix_90`, or `full_short_notes`
  - new LN Mix v2 targets 60% long alternating 2/4-beat holds, 20% medium 1-beat holds and 20% short alternating 1/4-/1/2-beat holds, using base BPM × Rate; other lanes keep taps/short holds and same-lane repeated taps are not merged
  - candidates must fit at least half a beat and end 50ms before the next same-lane head; a target that does not fit steps down through 4, 2, 1, 1/2 and 1/4 beats, so dense charts can realize fewer long holds than the target ratio
  - existing LN/CN, original heads and keysounds are preserved; heads overlapping an existing same-lane span are excluded. The same chart, `random_seed`, Rate and transform version reproduce the same result
  - legacy replay metadata without a transform version uses v1 and its original 1/8-, 1/16-, 1/24- and 1/32-note policy. New v2 allows local history/replay only and is excluded from legacy chart-wide BEST and Sites upload. See [length policy, compatibility and verification (Korean)](release-conditions-1.8.8.ko.md#ln-mix의-요구-의미)
- `ghost_battle_enabled` (bool)
  - defaults to `false`
  - when `true`, TenRiff auto-loads the selected chart's best compatible replay for ghost comparison
  - when `false`, normal gameplay stays single-field
- `autoplay_enabled` (bool)
  - non-competitive automatic play mode for QA
  - when `true`, playable note input is handled automatically and the result is saved as `AUTOPLAY`
  - never awards an official clear, best score, clear lamp, or default ghost; local result/replay history is retained
- `practice_no_fail_enabled` (bool)
  - QA assist mode
  - when `true`, gauge-based early failure is disabled while judgement and result export still run to chart end
  - the result is tagged with `ASSIST`
- `one_miss_fail_enabled` (bool)
  - when `true`, the first OD8-converted object `MISS` forces the gauge to zero and ends the run immediately
  - native `BAD` timing alone and empty-key `POOR` judgements do not trigger it
  - enabling it in Mode Settings automatically disables `practice_no_fail_enabled`
- `pacemaker_mode` (string)
  - `off | accuracy | score` (default `off`)
  - Shows the live gap from the accuracy or score target. Normal gauge failure, clear and scoring rules remain active; missing the target alone does not fail a clear. Best records and ranking remain eligible when all other conditions are met
  - enabling Pacemaker disables Practice and Sudden Death; replay playback and multiplayer force it off
- `pacemaker_target_accuracy` (double)
  - 0..100, default 90.0; displays current standard Accuracy minus the target in percentage points
- `pacemaker_target_score` (int)
  - 0..10000, default 8000; displays current score minus the target pace proportional to judged note weights
- `auto_scratch_hide_lanes` (bool)
  - Default false. With auto_scratch in mods, hides actual BMS scratch columns while preserving other key widths and logical input IDs
  - `auto_scratch`: automatically plays actual BMS scratches only. Score multiplier 0%, ASSIST record, excluded from official best records and rankings
- `song_index_profile` (string)
  - `safe | fast`
  - Default `safe`: prepares hashes and preview metadata, and supports difficulty tables. Native LV/CR calculation follows the separate setting below.
  - Saved choices are preserved. Optional `fast` (minimal) skips hashes, previews and difficulty calculation.
- `calculate_song_index_difficulty` (bool)
  - defaults to `false`
  - `false` keeps BMS `#PLAYLEVEL` as the menu LV and skips the CPU-heavy native LV/CR calculation
  - `true` calculates Revive LV/Circus Rating during a full `safe` index; `fast` always skips it
  - changing the setting separates cache modes and triggers a full reindex of the current song source

### `ui`

The Home language button cycles **EN → KO → JA → ZH → EN**. It saves through the existing `ui.language` setting and persists across restarts. Strings without a translation can fall back to English.

`ui.skip_start_countdown` and `ui.instant_restart_key` are built-in controls saved in the profile and `.trprofile` exports. Only the initial `3 / 2 / 1` display after loading is skipped; audio preparation, the existing three-second chart lead-in and judgement timing remain unchanged. Resuming from pause always retains its three-second countdown. Multiplayer forces the initial countdown and blocks instant restart; Session Mix also blocks instant restart. Instant restart does not fire while paused or during the initial countdown, and the key must be released before it can trigger again. See [start/restart controls (Korean)](gameplay-start-controls.ko.md) for lane/reserved-key conflicts and editor-practice start positions.

These controls do not change Sites submission policy. Long LN Mix v2 exclusion from legacy BEST and Sites is a separate conversion policy; the [plugin runtime, API and SDK remain future plans (Korean)](result-analysis-1.8.8.ko.md).

`last_played_chart_path` is the local path of the most recent chart whose gameplay started; empty by default. Replay and editor practice do not update it. Used by `title_music=last_played` across restarts.

| Field | Type, range, default | Behavior |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | Display name; empty uses profile ID. Whitespace/control characters are normalized. |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | Local PNG/JPG avatar path. |
| `language` | `en`, `ko`, `ja`, `zh-CN`; `en` | UI language; invalid values normalize to en. |
| `menu_font_size` | `normal`, `large`, `extra_large`; `normal` | Profile and first-run menu text at 100%, 115%, or 130%. Gameplay fonts remain skin-controlled. |
| `skip_start_countdown` | bool; `false` | Skip the initial `3 / 2 / 1` display in solo play, including normal starts, menu restarts and result retries. |
| `instant_restart_key` | string; `""` (unassigned) | Key name for restarting an active solo chart from the beginning. The new attempt always skips the initial countdown; the interrupted attempt saves no result or replay. |
| `hispeed_decrease_key` | string; `"F5"` | Note Speed ×0.5 key, assigned in Profile; empty disables. |
| `hispeed_increase_key` | string; `"F6"` | Note Speed ×2 key, assigned in Profile; empty disables. |
| `function_keys` | object; F1–F11 | Profile mappings for F1–F11 actions and displayed hints. Empty unassigns. |
| `all_song_sources` | bool; `false` | Restore ALL SONG, combining registered folder caches and deduplicating identical chart paths. |
| `result_tail_ms` | double; `500` ms | Extra result-transition delay after judgement completion; the chart audio end is also considered when `audio.play_to_end=true`. |
| `require_enter_to_exit` | bool; `true` | Retained for read/write compatibility; the current Windows result-input path does not use it to auto-exit. |
| `show_cursor_in_gameplay` | bool; `true` | Show the mouse pointer during gameplay. |
| `active_song_source`, `recent_song_sources` | string / string[] | Current and recent song folders. |
| `song_sources_initialized` | bool; `false` | Explicit folder addition/removal sets this to true so an intentionally empty source list stays empty on relaunch. See [source management](library-management.md). |
| `session_mix_lr2_course_path` | string | Selected LR2 course file path. |
| `favorite_chart_keys` | string[] | Internal chart keys for favorites. |
| `collections` | object: name → string[] | Chart keys grouped by collection name. |
| `song_collection_filter` | string; `all` | All/favorites/collection filter; saved immediately. |
| `song_key_filter` | int: `0..16`; `0` | Key-count filter; zero means all. UI choices are 4K–10K, 12K, 14K and 16K. |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | Level bounds; zero disables the corresponding bound. With an external table selected, use its displayed numeric level and exclude nonnumeric labels while bounds are active. Native LV keeps the existing built-in level. See [number/prefix handling (Korean)](release-feedback-1.8.8.ko.md#선택한-난이도표-기준의-최소최대-필터). |
| `difficulty_table_path` | string | Local header JSON or downloaded profile-cache header. Selecting a table switches to safe indexing; Native LV clears it and removes table metadata from a valid cache. |
| `difficulty_table_url` | string | Original HTTP(S) BMSTable page/header URL. Resolves bmstable metadata and caches header/data; local JSON selection clears it. |
| `online_records_server_url` | string | Records/ranking/chat API URL; failures do not block local play or records. |
| `tenriff_main_server_url` | string | F10 main-server API URL; default is `kTenRiffMainApiUrl` in Config.h. |
| `private_server_url` | string | F10 private API URL; remote HTTPS, HTTP allowed only for localhost. |
| `account_server_mode` | `main`, `private`; `main` | Last account-server selection. |

Difficulty-table headers use `name`, `symbol` and a local relative `data_url`; data entries match `md5` or `sha256` plus `level`. Remote imports are stored in the profile's `difficulty_tables` cache.

Native LV clears `difficulty_table_path` and `difficulty_table_url`; a valid cache removes external table metadata without a full rescan. The default is BMS `#PLAYLEVEL`; with difficulty calculation enabled, the cached calculated LV is retained.

### `skin`

Current `skin` settings and active skin assets can be exported/imported as a [portable `.trskin` preset](skin-presets.md). Audio devices, keymaps, accounts, song sources and timing calibration are excluded.

These are profile `config.json` skin settings. For a skin package's `skin.json` contract, see [Skin format](skin-format.md).

| Field | Type, range, default | Behavior |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | Skin source. |
| `tenriff_skin_name`, `lr2_skin_name` | string | Imported skin folder names. |
| `scratch_position` | `left`, `right`; `left` | Changes only 7+1 scratch display order; input/judgement lanes stay unchanged. |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 coordinate resolution; auto uses `#DST_NOTE` coordinates, not filenames. |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | Selecting a preset in the menu resets its visual option bundle. |
| `note_shape` | `rect`, `simple_rect`, `circle`, `triangle`, `pentagon`, `hexagon`, `square`, `diamond`, `arrow`; `rect` | Procedural note shape. `simple_rect` uses sharp corners and a flat face; the existing `rect` design is preserved. Imported images and explicit vector sprites take precedence. `hex` is a legacy alias for `hexagon`. |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | Fill the rectangle / fit inside with aspect preserved / keep width and derive height from aspect. |
| `preserve_note_image_aspect_ratio` | bool; `false` | Legacy field; explicit `note_image_aspect` wins. Saved true for non-stretch modes. |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | Show note borders, lane dividers, and the judgement line respectively. |
| `note_divider_gap_px` | double: `0..40`; `12` px | Gap from each note edge to the divider; zero expands notes to the dividers. |
| `show_gear_boundary_line` | bool; `false` | Show the gear boundary line. |
| `show_timing_feedback` | bool; `true` | Show FAST/SLOW text independently of the bar. |
| `show_timing_bar` | bool; `true` | Show the timing bar; older profiles/skins without this field inherit the text switch. |
| `timing_bar_always_visible` | bool; `false` | `true` keeps ranges and latest FAST/SLOW text. Maximum hits refresh text; 0ms clears it. Markers expire at 0.7s; conditional bars show while markers exist. `show_timing_bar` switches the bar off. |
| `timing_marker_style` | `dot` / `barcode`; `dot` | Judgement markers: round dots or straight vertical strokes. |
| `timing_feedback_override` | bool; `false` | After the user edits a switch, profile visibility choices take priority over the skin manifest. |
| `timing_text_offset_x`, `timing_bar_offset_x` | double: `-600..600`; `0` | Independent text/bar X offsets added to the existing judgement-area layout, in 1920x1080 base pixels. |
| `timing_text_offset_y`, `timing_bar_offset_y` | double: `-400..400`; `0` | Independent text/bar Y offsets in base pixels; positive moves down. |
| `timing_text_scale`, `timing_bar_scale` | double: `0.50..2`; `1` | Independent FAST/SLOW text and bar scales, adjustable from 50–200% in 5% steps or with sliders. Apply to Studio, Classic, imported skins and previews without changing judgement/combo size or XY offsets. |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | LN tail-cap visibility and taper respectively. Only the Korean taper label changes to `LN 끝 줄임`; appearance and judgement rules stay unchanged. |
| `judgement_line_glow_enabled` | bool; `true` | Judgement-line glow. |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst brightness; zero disables it. |
| `key_pulse_enabled` | bool; `true` | Legacy ON/OFF mirror; false or zero brightness disables the burst. |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | Built-in Hit Burst material. |
| `hud_layout` | `classic`, `studio`; `studio` | HUD style for Native or imported LR2 solo play; LR2 note/key/gear assets and geometry are preserved. Missing/unknown values use `studio`; select Classic in Options › Skins. In 1.8.9, information is upper right, score lower left and the horizontal map lower right; narrow margins scale groups or move them to the opposite side. Extreme fields plus Performance HUD can hide only the information card when neither upper side has room. Other image skins, ghost and multiplayer retain Classic. [Placement and verification scope](release-1.8.9-gate.md). |
| `hud_riff_map_visible` | bool; `true` | Show Studio Deck's horizontal 64-bin density map and elapsed/total time. The default is lower right; narrow margins move/scale it, and very narrow maps omit time labels. Options › Skins › Studio Riff Map saves this in profiles and skin presets. Hiding it preserves other card positions and Classic HUD. |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | Menu font; default is Segoe UI. Logo, rank and combo keep their dedicated fonts. |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | Lane key-label position. |
| `judgement_line_position` | double: `0..1`; `0.82` | Vertical judgement-line position ratio. |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | Field X offset in 1920×1080 base pixels; additionally constrained to keep the field and ↔ handle visible. |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | Independent combo/judgement Y anchors; missing judgement position inherits `combo_position` in older profiles. |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | Independent combo/judgement X offsets in 1920×1080 base pixels. |
| `combo_font_scale`, `judgement_font_scale` | double: `0.50..2`; `1` | Independent combo/judgement text multipliers. Skin Settings adjusts 50–200% in 5% steps or with mouse sliders; profile values also apply to imported skins. |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | Lane-background opacity. |
| `black_playfield_enabled` | bool; `true` | Black field including lane gaps. |
| `visual_opacity` | double: `0.20..1`; `0.96` | Shared opacity multiplier for notes, receptors and key labels. |
| `note_outline_opacity` | double: `0..1`; `0.78` | Note-outline opacity. |
| `note_fade_in` | double: `0..1`; `0` | Top black fog depth; notes gradually appear. Zero is off. |
| `note_fade_out` | double: `0..1`; `0` | Black fog depth above the judgement line; notes gradually disappear. Zero is off. Receptors and HUD remain visible. |
| `hold_body_opacity` | double: `0..1`; `1` | LN-body opacity. |
| `lane_width_scales` | object: mode → number[]; `0.50..1.75` | Per-lane widths; array length equals lane count. |
| `note_width_scale` | double: `0.50..1.40`; `1` | Note & Field Size: scales field, lanes, notes and adjacent gauge around the center. |
| `lane_spacing_scales` | object: mode → number[]; `0..2` | Lane-gap array with lane_count - 1 entries. |
| `note_height_scale` | double: `0.50..4`; `1.8` | Note head/tail height scale. |
| `lane_divider_width_scale` | double: `0..2`; `1` | Shared divider-width scale for all modes, including imported LR2 dividers. |
| `lane_center_gap_scale` | double: `0..2`; `0` | Center gap between the two 16K blocks. |
| `hold_body_width_scale` | double: `0.50..1.20`; `1` | LN-body width scale. |
| `note_width_scales`, `note_height_scales`, `lane_center_gap_scales` | object: mode → number | Per-mode overrides of the corresponding scalar, with the same bounds. |
| `lane_divider_width_scales` | object: mode → number | Legacy field; runtime uses the shared `lane_divider_width_scale`. |
| `lane_colors` | object: mode → string[] | Color-token array with one entry per lane. |
| `single_color` | string; `off` | A color token overrides all lanes while preserving `lane_colors`. |

Per-mode arrays/overrides support `4k`, `5k`, `6k`, `7k`, `7+1`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`. Color tokens: `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`. `7+1` is a skin palette, not a separate keymap mode. Legacy `expand_notes_to_dividers=true` seeds a zero gap; explicit `note_divider_gap_px` takes precedence.

No full-screen BGA dark filter is applied; configured black playfields, lane backgrounds and gear remain intact. Visual Latency is the fifth Skin Settings item.

Text readability applies across menus, options, the song library, results, help/chat/account overlays, the editor and gameplay. Light text gets a dark contour and dark text a light contour, preserving skin text colors and transparency.

### `offsets`

- `input` (double)
- `visual` (double)
  - clamped to the `-500..500` range
  - the storage key and behavior are unchanged; the UI exposes it as `Skins > Visual Latency`
- `sound` (double, ms)
  - clamped to `-500..500` and exposed in `Audio Settings > Sound Offset` and the `Calibration Wizard`
  - positive values delay chart BGM/autoplay keysounds and negative values advance them; judgement, note/BGA timing, and hit-triggered `follow` keysounds do not move

## `keymap.json`

Xbox/XInput buttons use existing key-token fields: `PadUp`, `PadDown`, `PadLeft`, `PadRight`, `PadStart`, `PadBack`, `PadLS`, `PadRS`, `PadLB`, `PadRB`, `PadA`, `PadB`, `PadX`, `PadY`, `PadLT`, `PadRT`. Press the physical button during Keymap capture; the same names work for Profile restart/Hi-Speed controls. LT/RT press above 30/255, and LS/RS are stick clicks. Matching buttons across four controllers share one logical input; analog axes and vibration are outside this release. [Controls, connection and compatibility](feedback-20261015.en.md).

### Shape
- `layout` (string)
- `bindings`
  - legacy 10K compatibility
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`
  - lane id -> key token under each mode

### Notes
- Old single-layout keymaps are migrated into the 10K map at runtime.
- Runtime selects the relevant mode binding based on the final chart lane count.
- Key rebinding is saved immediately to `keymap.json` after a successful capture; there is no separate final save step.
- When opening keymap editing from Song Select, the editor should default to the selected chart's lane count first, then fall back to `mode.key_mode`, then `10k`.

## Runtime Migration Notes
- Stale profiles are automatically corrected for some values.
- In particular, BMS defaults and keysound-policy values are migration targets; legacy osu chart/skin fields are no longer saved.
- If the config file does not exist, the app starts with defaults and immediately saves the profile.

## Settings usability and audio synchronization

New profiles default `audio.volume` to `0.1`; explicitly saved volumes remain intact. `audio.mute_when_inactive` defaults to `false` and gates output while another window is active, preserving music position and saved gain. ASIO Buffer Size is displayed in samples per channel; the internal `frames_per_buffer` API name stays compatible.

Key Settings presents horizontal Key 1… keys with separate primary and secondary bindings. Optional `keymap.json` `secondary_modes` follows the existing mode/lane structure. Either held input keeps the logical key held; × or Delete during capture clears only the secondary slot. The initial Options editor mode is 4K; opening from a chart uses its actual key count.

`skin.key_backdrop_enabled` and `skin.key_backdrop_opacity` (0–1) control held-key lane tint independently of hit-burst brightness. Skin defaults apply until the first user change, which saves `key_backdrop_override=true`. Judgement-line thickness follows note height. ALL SONG shows only table members while a difficulty table is active; Native LV restores all charts. Visuals/BGA follow the audible playback head while judgement/audio scheduling retain the write timeline. Actual speed changes play a short click.

Held-key backdrop RGB brightness uses `skin.key_backdrop_brightness` (0–2, default 1); maximum height uses `skin.key_backdrop_height` (0–1, default 1), anchored at the field bottom. Both are independent of opacity and match in gameplay, ghost play and Skin Settings previews. Hold Left/Right to repeat an enabled minus/plus setting.

## BMS replay/result compatibility (1.9.1)

Three user-approved optional evidence fields are separate from profile settings: `bms_gauge_version`, `bms_control_flow_version`, and `bms_random_seed`. Missing versions mean 1; a missing branch seed falls back to historical `random_seed`, then 0. New ordinary BMS plays record gauge v2; charts with authored branching also record control-flow v2 and a uint32 branch seed. The lane RR/SR, conversion and LN Mix `random_seed` is independent from the authored branch seed. Audio-device reloads retain the same branch, and replay loads use the saved seed and policies. Courses keep their existing gauge and use the new branch parser. Protocol v6 peer battle retains gauge/control-flow v1 and branch seed 0. Sites has accepted supported gauge/branch evidence since 1.9.1.1; production v19 also accepts new R7/R8 in separate boards. Permanent Guest exclusion and separate Ranked restrictions remain. New plays use R7/R8 and old records retain their original IDs. Local BEST prefers the policy-version tuple; result comparison groups only matching policies and branch seeds.

[Formulas, branching, compatibility and verification scope](bms-total-random-followup.en.md).
