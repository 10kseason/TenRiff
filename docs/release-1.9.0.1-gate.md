# TenRiff 1.9.0.1 release scope and verification gates

Requested release line: **1.9.0.1**, 2026-10-15. The user-pinned baseline remains **1.9.0**; this release does not advance it. Preserve previous tags, release assets, installations, profiles and original chart/audio inputs. [Baseline](baseline-1.9.0.en.md) · [한국어](feedback-20261015.ko.md) · [English](feedback-20261015.en.md) · [日本語](feedback-20261015.ja.md) · [简体中文](feedback-20261015.zh-CN.md).

## Changes and compatibility

- Song previews use 0.75 of their previous gain. Gameplay audio and TenRiff scene-music gains remain unchanged.
- Profile assigns Hi-Speed -10/+10, defaulting to F5/F6 instead of half/double. F3/F4 remain ±0.25, range 0.50–50.00. Persist the two bindings in the existing profile/preset structure and reject conflicting controls.
- Xbox/XInput digital buttons and triggers work alongside either keyboard backend. A scoped worker owns potentially blocking system queries; the existing InputThread drains its internal queue and remains the sole producer of the shared input queue. The worker starts/stops with input collection, without installing an always-running service. Support Keymap/Profile capture, D-pad/A/B menu navigation and pause on otherwise unassigned Start. Use system XInput 1.4 with 9.1.0 fallback, four-controller aggregation, disconnect releases, held-button suppression at connection/focus transitions and two-second empty-slot rescans. Analog stick axes and vibration are not included.
- Native file pickers use an owning application window and one modal invocation at a time. Clear stale queued input on close and require held closing keys to be released before a new menu action.
- Calculate coincident BMS BPM changes before STOP. The reported DEATH_SENTENCE chart has 32 mines and no invisible-note channels; a STOP shortened by the old ordering placed a mine within an ordinary tap. Preserve authored mines, extreme BPM/scroll effects, original files and numerical judgement rules.

Optional `mode.bms_timing_version` in replay metadata and `bms_timing_version` in results retain old scheduling for absent/1 and use corrected scheduling for 2. Reject unsupported or malformed versions. Only affected new plays export 2; unaffected charts retain existing timing identity. Replay format 3, R1–R6 judgement/scoring and old standalone replay playback remain. Exclude mismatched ghosts and prefer verified corrected local BEST records without deleting historical records. Cache 18 refreshes derived chart length/difficulty.

The existing online service cannot distinguish the schedules. **Affected timing-v2 records remain local with an explicit status until online support is added.** Unaffected eligible records retain existing upload behavior. No web/API/database deployment or ranking migration is included. Older clients do not support timing-v2 playback or verification.

## Required delivery evidence

- Final Windows Release build, all CTest targets with zero integration skips, and skin-editor tests.
- BPM/STOP order and rounding regressions; real reported-chart tap/mine behavior at multiple sample rates/rates; legacy/new replay verification and malformed-version rejection.
- XInput decoding, trigger thresholds, multi-controller holds, connection/focus transitions and disconnect tests; actual input-thread lifecycle and menu/session capture checks. State explicitly whether a physical controller was available.
- Actual MenuApp/GameSession smoke, including Hi-Speed/profile persistence, live timing-v2 result/replay flow, ghost compatibility and owned native-dialog reopening checks. Distinguish internal action injection from physical input.
- Native render/profile fixtures where controls changed, plus client/source archive CRC, extracted-file hashes, source-to-Git correspondence, safe defaults and privacy checks.
- Fresh independent extracted-source configure/build/CTest/editor/session checks; successful PR and post-merge main CI for ASan and OpenVINO.
- Fresh draft and anonymous public downloads of client ZIP, source ZIP and SHA256SUMS; compare hashes, sizes and CRC, and verify earlier release identities remain unchanged.

This document defines the required checks, not a claim that they have already passed. Version-specific release notes record completed checks and the exact source identity; the checksum asset identifies delivered archives. Intermediate native checks predate the final XInput/timing changes and cannot substitute for the final run. Complete logs and initial failures remain in separate local evidence, outside the public source package.

## Remaining limits and follow-up

Physical Xbox-controller connection/button behavior, physical input/audio latency, full-song listening, long play, other PCs and end-to-end multiplayer require separate validation. Automated or simulated button tests cannot establish those results. Existing authored extreme BPM transitions can still look abrupt; this fix does not remove chart gimmicks or mines.

Online timing-v2 verification/support is separate follow-up work. Plugin runtime/API/SDK, major GameSession restructuring and unrelated CPU experiments are outside this release. A future fixed-baseline change still requires explicit user selection and synchronized compatibility evidence.
