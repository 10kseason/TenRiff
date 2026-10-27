> **2026-10-27 · 1.9.9.3:** Includes accelerated controls, LR2CRS courses, BMS/KPS overlay, clear cards, grade colors and EX rainbow fixes. General fixed baseline: **1.9.6**. Older release and local-candidate entries below are historical; this scope supersedes them. [1.9.9.3](release-1.9.9.3.en.md) · [Verification](release-1.9.9.3-gate.md).

> **2026-10-24 · 1.9.9.1 release scope:** 21:9 displays, released-hold visibility and final-tail waiting, and LN Mix v3 with 50ms clearance around same-lane mines. [Changes and compatibility](release-1.9.9.1.en.md) · [Verification gates](release-1.9.9.1-gate.md). The general fixed baseline stays **1.9.6**. Historical v1/v2 replays remain; v3 needs the 1.9.9.1 verifier. Earlier notices below are historical. Publication and downloads follow the [1.9.9.1 release](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.1).

> **2026-10-22 · 1.9.9 · NK2 E / 16K submissions:** [1.9.9](release-1.9.9.en.md) · [Verification](release-1.9.9-gate.md). Baseline **1.9.6**. 4/5/6/7/8/9/10/12/14/16K. Earlier dated entries below are historical.

> **2026-10-21 · 1.9.8.5 release scope:** Adds BGA playback and black-key transparency fixes, wrapping song selection, Library/clear labels, decimal timing, scratch-free 7K and secondary-key ownership to the user-selected 1.9.8. The fixed general baseline remains **1.9.6**. Earlier notices retain their historical scope; see [1.9.8.5](release-1.9.8.5.en.md) for behavior and compatibility and [release gates](release-1.9.8.5-gate.md) for verification. Publication and downloads follow the [release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5).

# TenRiff 1.9.6 Fixed Baseline

[한국어](baseline-1.9.6.md) | [English](baseline-1.9.6.en.md) | [日本語](baseline-1.9.6.ja.md) | [简体中文](baseline-1.9.6.zh-CN.md)

The explicit user request on 2026-10-19 advances the general source baseline from 1.9.0 to **1.9.6**. Do not track main/latest automatically. Earlier dated notices below are historical.

The commit is `4d4645d1fc33e4db74d2cb0b235fa6cf1534cd8a`, tag object `2982d2935a360278c2f752884680714c7acc8571`, merged by PR #105. Preserve published tags/assets, old sources/candidates, profiles and original charts.

- [1.9.6](https://github.com/10kseason/TenRiff/releases/tag/1.9.6) · [PR #105](https://github.com/10kseason/TenRiff/pull/105)
- [1.9.6 policy / compatibility](release-1.9.6.en.md) · [Normal / Hard / EX](judgement-baseline-20261018.en.md)
- Normal: VEZ/EZ 22ms, NM 19ms, HD 15ms, VHD 10ms PG; original exact GR/GD/BAD. Hard = Normal NM; EX = Normal VHD; no extra scaling or rounding. R9/R10 current, R1–R8 historical.
- S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70; E <70 surviving, F gauge failure. AUTO and -- retained.
- Guest/custom exclusion and client_submitted boundary; BGA weights excluded, NK3 allowlist retained, OFF/empty/Auto defaults; saved profiles/calibration/shortcuts/replay formats retained.
- [1.9.7 follow-up](settings-feedback-20261019.en.md)

| Published asset | SHA-256 |
| --- | --- |
| TenRiff-1.9.6.zip | `8fc9ab32f49b26dba8f5fe61e01c925742dd592dc207e454ccaf4c829c6907cd` |
| TenRiff-1.9.6-source.zip | `95e03aabb095145d3dccb04d8157870f60f253fc8836ffe88ba4070c32e50f30` |
