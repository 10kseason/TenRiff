# TenRiff 1.9.1 release scope and verification

Requested release: **1.9.1**, 2026-10-15. This combines the 1.9.0.2-based audio/controls and BMS/visual follow-ups with the saved Home language button. The user-pinned baseline remains **1.9.0**; publishing 1.9.1 does not promote the baseline or replace previous release assets, installations, profiles or original chart files.

[한국어](feedback-1.9.1.ko.md) · [English](feedback-1.9.1.en.md) · [日本語](feedback-1.9.1.ja.md) · [简体中文](feedback-1.9.1.zh-CN.md) · [Baseline](baseline-1.9.0.en.md).

**Publication status and available downloads are determined by the [1.9.1 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.1).** These source documents describe the release scope and distinguish recorded development evidence from final release checks; they do not assert that publication or remote asset readback has already completed.

## Included changes

- **Home language:** cycle EN → KO → JA → ZH → EN, save the selected language through `ui.language`, and retain it across restarts. The canonical Chinese token is `zh-CN`; aliases normalize to it. Missing catalog translations retain the established English fallback. The button does not change chart, profile or filesystem names.
- **Audio:** use only Master, BMS Performance (`BMS 연주 볼륨`) and TenRiff Music controls, with new defaults 10%, 20% and 100%, all in 1 percentage-point steps. Existing `gameplay_volume` stays the one BMS-specific multiplier. Legacy background/keysound fields remain compatible with old files but no longer multiply runtime/editor/preview audio. Preserve explicitly saved volumes and preview ×0.75.
- **Input and skin label:** apply selected debounce in the actual input filter, with 1ms steps over 0–20ms and default 0ms. Retire the old 5→8ms rewrite and retain saved values. Rename the Korean LN setting to `LN 끝 줄임` without changing its visual or judgement behavior.
- **Loading and outro:** publish difficulty-table loading status before blocking import/refresh work and clear it afterward. With Listen to End enabled, ordinary solo Esc skips the post-note outro directly to normal results, preserving score/replay and avoiding pause/abort flags. All remaining taps and LN tails must be resolved. Preserve course and multiplayer controls.
- **BMS TOTAL:** use LR2-derived Normal/Easy recovery and Hard/EX damage corrections for new ordinary BMS play. Preserve TenRiff 100% start, zero failure, parallel gauge shifts, judgement classification, LN head/tail weights, mines and course gauge. This is a delta policy, not a full LR2 rules replacement.
- **Authored branches:** give RANDOM/SWITCH branches an independent session seed, preserve it across device-rate reloads and in replays, accept annotated control numbers and perform bounded missing-ENDIF recovery in parser v2. Keep historical parser v1 for old evidence. Index cache epoch 19 refreshes derived metadata from older RANDOM parsing; original charts are unchanged.
- **Compatibility:** add optional `bms_gauge_version`, `bms_control_flow_version` and `bms_random_seed` to replay/result evidence, as approved by the user. Missing versions retain v1; absent branch seeds use the historical lane seed, then 0. Preserve existing replay-envelope and R5/R6 score IDs. Check policy/branch identity for replay verification, ghost eligibility and result comparison; local BEST prioritizes current policy versions.
- **Presentation:** Result shares the song lobby's red level and registered key-mode prefix colors, with green ⑤ only for built-in Aery5. Native menu branding uses the supplied 10 motif. Preserve the established Song Library controls, taller cover, filtering and score colors.

Protocol v6 peer battle keeps gauge/control-flow v1 and branch seed 0 because it does not negotiate the new compatibility fields. Courses keep their course gauge while using new authored branch selection. **New BMS gauge/branch records remain local: both Sites and ranked uploads are blocked until the corresponding server verifier/board conditions are supported.** Existing timing-v2 and LN-Mix-v2 online restrictions remain. No website, API, database or ranking migration/deployment is included.

The [BMS design note](bms-total-random-followup.en.md) contains formulas, source references, numeric examples and precise scope. The [configuration guide](config.en.md) records current defaults and saved settings. Dated development entries in the READMEs remain historical and do not supersede this release scope.

## Recorded development evidence

The local BMS/visual candidate **before the 1.9.1 Home language addition** passed:

| Check | Recorded result |
| --- | --- |
| Windows Release configure/build | Passed |
| CTest | 3/3; 1,279 cases; zero integration skips |
| Focused BMS tests | 108/108 |
| Offline skin editor | 35/35 |
| Native MenuApp/GameSession | 326/326 |
| Result/title visual fixtures | Eight fixtures; 480 Presents; 50 geometry checks |
| Generated native vector atlas | Passed |

The supplied Cultivator chart was read without modification. Seeds 0–3 exercised its four authored branches; the historical parser produced 0/17/0/0 notes while v2 retained 657 notes in each branch and preserved the tail. Old/new replay paths reconstructed their own chart schedules and final gauge values. Captured Result regions showed red level text, the expected five registered prefix colors and no green circled-five pixels under native Lv.

These counts establish that development stage only. The Home language implementation, Chinese catalog, epoch-19 refresh, final version markers and complete release source must pass the final checks below. Internal dispatch/geometry checks are not physical keyboard/controller or OS-click evidence. The eight development captures did not establish foreground native-input delivery.

## Final release checks

- Final Release build, complete CTest, focused BMS compatibility/branch tests and editor checks after all changes. Record exact revisions and counts in the separate release evidence.
- Native MenuApp/GameSession checks including Home language cycling, saved-profile readback, all four locale renders, loading status and post-note Esc; distinguish internal dispatch from physical-device coverage.
- Native Result/lobby color comparisons and 10-mark inspection at normal and small sizes. Validate that language labels and the new button do not overlap existing title/profile controls.
- Final client/source ZIP CRC, extracted hashes, clean defaults, defined privacy checks and correspondence with the final Git source. Build and test the independently extracted source bytes.
- PR and post-merge main ASan/OpenVINO CI against the final revisions. Keep initial failures and reruns in the evidence.
- Fresh draft and anonymous-public readback of client ZIP, source ZIP and checksums, confirming size/hash/CRC. Preserve previous release/tag/asset identities; never move an old tag or replace an existing asset.

Publication is complete only after those release operations succeed. This document does not mark pending checks passed in advance. Final source identity and archive hashes belong in the release notes/checksum asset and the separate release evidence.

## Retained failures and limits

Development evidence retains the initial restricted build stall, compact-lane test expectation correction, abnormal GPU test exit, default-encoding capture-helper failure and overlapping preview/build asset-copy error. Final development reruns passed; the first GPU process failure's underlying cause remains unconfirmed. Earlier intermittent native input/shortcut failures stay in their original release evidence and are not claimed solved by this release.

Physical Xbox/gamepad behavior, physical keyboard/audio latency, full-song listening, long sessions, other PCs, complete malformed-BMS coverage and original-LR2 executable equivalence remain unverified by these checks. New online BMS policy support, plugin runtime/API/SDK and major architecture changes are separate work. A future fixed-baseline promotion still requires explicit user selection.
