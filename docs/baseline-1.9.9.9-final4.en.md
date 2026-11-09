# TenRiff 1.9.9.9-final4 fixed baseline

[한국어](baseline-1.9.9.9-final4.md) | [English](baseline-1.9.9.9-final4.en.md) | [日本語](baseline-1.9.9.9-final4.ja.md) | [简体中文](baseline-1.9.9.9-final4.zh-CN.md)

On 2026-11-10, by user request, the general source baseline moved from 1.9.6 to **1.9.9.9-final4**. main/latest is not followed automatically. The policies below stay in force across the baseline change.

The baseline commit, tag and asset hashes are recorded on the [public release](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9-final4) and in the workspace `BASELINE.md`. Published tags and assets, earlier source, candidates, profiles and original charts are preserved.

- [Changes and compatibility](release-1.9.9.9-final4.en.md) · [Release gates](release-1.9.9.9-final4-gate.md) · [Previous baseline 1.9.6](baseline-1.9.6.en.md)
- Judgement: Normal PG VEZ/EZ 22 ms, NM 19 ms, HD 15 ms, VHD 10 ms with the original GR/GD/BAD. Hard = Normal NM, EX = Normal VHD. No extra scaling or rounding. [Normal / Hard / EX](judgement-baseline-20261018.en.md)
- Replays: the behaviour of R1–R14, custom variants, BEST generations and peer protocol versions never changes. A judgement change gets a new ruleset ID.
- Grades: S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70, E below 70 surviving, F on gauge failure. AUTO and -- retained.
- Online: Guest/custom exclusion and the client_submitted boundary are retained. Imported LR2 lamps are display-only and never submitted.
- Delivery: BGA weights excluded, NK3 allowlist retained, defaults OFF/empty/Auto. Personal data such as a user's LR2 database is never included.

## Compatibility with 1.9.6

Rulesets added after 1.9.6 (R11–R14), LN Mix v3, key conversion versions and the BMS timing, gauge and RANDOM policy fields are all versioned in records, and the verifier reproduces the earlier policies. Saved profiles, calibration and shortcuts load without migration. This release itself changes no judgement, score or replay calculation.
