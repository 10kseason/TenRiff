# TenRiff 1.9.0 Fixed Baseline

[한국어](baseline-1.9.0.md) | [English](baseline-1.9.0.en.md) | [日本語](baseline-1.9.0.ja.md) | [简体中文](baseline-1.9.0.zh-CN.md)

**Fixed baseline: [1.9.0](baseline-1.9.0.en.md)** (explicit user request, 2026-10-14). Use Git tag `1.9.0`; do not advance automatically with `main` or `latest`. Preserve the 1.8.7 baseline and previous releases as history.

## Identity and preservation

The explicit request of 2026-10-14 advances the baseline from 1.8.7 to **1.9.0**. Preserve [1.8.7](baseline-1.8.7.en.md) as history and keep prior tags, assets, profiles and original charts. Pin the commit resolved by `git rev-parse 1.9.0^{commit}`, not a moving main/latest. The version-specific release notes record the final commit and asset SHA-256 values; the downloadable checksum file identifies the ZIPs without embedding self-referential archive hashes in source. Windows GUI and BMS-family charts remain the product scope.

## Behavior and compatibility

- Separate whole-BMS volume from TenRiff menu/result music. Relative BMS gain is 1.0 at 100%; scene music has 0.1 reference gain; master defaults to 70%. Preserve saved user values.
- Session-only Normal/Hard/EX: EX keeps chart RANK scaling with EASY-rank PG14/GR44.571429/GD78.857143ms; BAD250ms is fixed across ranks. Automatic miss340ms, Hard hold policy and 1.10 score factor remain.
- New Normal/Hard records use R5; EX uses R6. Preserve R1–R5, historical Easy and conversion-version replay behavior. Old clients are not guaranteed to verify R6.
- Surviving grade thresholds: S++99/S+98/S95/A+93/A90/B85/C80/Dotherwise; failure F. Display changes do not rewrite old score/judgement evidence.
- Cache17 rereads SUBARTIST/OBJ metadata without rewriting chart/replay files. New defaults are EX gauge, fast/minimal indexing and ghost off. Skin/font/indicator changes retain judgement/input/batching contracts.
- Keep precise waits and input MMCSS/power policy; CPU utilization alone does not establish physical latency gains.
- Sites accepts client_submitted R6 records and displays upload status. It does not newly verify replays server-side; retain existing authentication/consent and multiplayer trust boundaries.

## Delivery and limits

Public scope includes client/verifier/build materials and allowed NK3 runtimes/inference models, excluding server implementations, accounts, profiles, songs, logs, secrets, training data, BGA upscaler models and standalone converter executables. Follow [release gates](release-1.9.0-gate.md), [source scope](../SOURCE_PACKAGE_SCOPE.txt) and [current state](current-state.en.md). Separate candidate checks from final 1.9.0 build, independent extraction, CI and remote readback evidence. Real authenticated uploads, the original missing-record cause, physical input/audio latency, long play and other PCs remain unverified. Plugin runtime/API/SDK and major GameSession extraction are not included. Future baseline changes require explicit authorization, four-language documentation and compatibility evidence.
