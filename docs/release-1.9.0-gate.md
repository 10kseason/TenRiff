# TenRiff 1.9.0 release scope and verification gates

Release line and explicitly requested fixed baseline: **1.9.0**, 2026-10-14. [Baseline contract](baseline-1.9.0.en.md) / [한국어](baseline-1.9.0.md). Previous tags, assets, installations and profiles remain preserved.

## Changes

Promotes the reviewed feedback candidate: independent whole-BMS and TenRiff scene-music volume, quieter scene music, compact lobby controls, filter/sort fixes, indicator scaling, simple rectangular notes and private UI fonts; new EX-gauge/minimal-index/ghost-off defaults; larger record grades, revised accuracy thresholds, result metadata/BPM!/table levels, Aery-only ⑤ color, SUBARTIST/OBJ credits, localized internal mods, parent Normal/Hard/EX selection and visible upload status. [Controls](controls-1.9.0.ko.md), [EX contract](judge-ex-1.9.0.ko.md), [Grades](grade-display.ko.md), [Upload diagnostics](web-upload-diagnostics.ko.md).

EX uses R6 so existing R1–R5 replay judgement is unchanged. Normal/Hard remain R5. EX retains chart RANK ratios, uses EASY-rank PG14/GR44.571429/GD78.857143ms, fixed BAD250ms, automatic-miss340ms, Hard hold behavior and 1.10 score factor. Source metadata cache17 rereads credits without modifying original charts or result/replay evidence. Existing saved values remain; new defaults do not silently reset profiles.

## Required delivery evidence

- Final Windows Release build, CTest with zero integration skips and skin-editor tests.
- Actual MenuApp/GameSession smoke plus bounded native screenshots; distinguish injected internal actions from physical input and image geometry from event dispatch.
- Client/source archive CRC, extracted-file hashes, source-to-Git correspondence, safe defaults and privacy scan.
- Fresh independent extracted-source configure/build/CTest/editor/session checks.
- Successful PR CI and post-merge main CI (ASan and OpenVINO), exact merged-tree correspondence.
- Fresh draft and anonymous public downloads of client ZIP, source ZIP and SHA256SUMS; compare sizes/hashes/CRC and earlier release identities.

Candidate-local evidence is retained separately: 1193 C++ cases, 242 native checks, 19 render fixtures, 35 editor tests, web unit30/API71/native payload40. These numbers do not claim that the 1.9.0 release checks have already run. Final version-specific release notes record the completed checks and exact source identity; the downloadable checksum file identifies the delivered archives. The local release evidence folder contains complete logs and failures, but private workspace evidence is not shipped.

## Limits and non-goals

The existing public Sites service already supports EX/R6 and remains client_submitted, not server replay verification. Real authenticated score submission and the original missing-record cause remain unconfirmed. No account secrets are read or reissued; no missing old records are automatically reuploaded. No new schema migration or ranking-identity change is required for this release.

Physical input/audio latency, long actual-song play, other PCs, and end-to-end multiplayer remain separate validation. Existing MMCSS/waits do not imply universal lag resolution. Old clients may reject new R6 records. The plugin runtime/API/SDK, major GameSession split and the separate CPU experiment are outside this release. Follow-up baseline changes require explicit user selection; main/latest cannot move the pinned baseline.
