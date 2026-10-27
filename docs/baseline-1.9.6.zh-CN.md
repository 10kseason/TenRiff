> **2026-10-27 · 1.9.9.3:** 包含长按调节、LR2CRS课程、BMS/KPS叠加、通关卡片、等级配色和EX彩虹修复。通用固定基线仍为 **1.9.6**。下方旧版本与本地候选描述属于历史记录，以本版范围为准。 [1.9.9.3](release-1.9.9.3.zh-CN.md) · [Verification](release-1.9.9.3-gate.md).

> **2026-10-24 · 1.9.9.1 发布范围：** 21:9显示、LN松开显示与最后尾部等待、在同轨地雷前后留出50ms的LN Mix v3。[变化与兼容性](release-1.9.9.1.zh-CN.md) · [发布检查](release-1.9.9.1-gate.md)。通用固定基线仍为 **1.9.6**。保留旧v1/v2回放，v3需要1.9.9.1验证器。下方旧说明保留当时历史，发布状态与下载以[1.9.9.1发布](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.1)为准。

> **2026-10-22 · 1.9.9 · NK2 E / 16K提交:** [1.9.9](release-1.9.9.zh-CN.md) · [Verification](release-1.9.9-gate.md). Baseline **1.9.6**. 4/5/6/7/8/9/10/12/14/16K. Earlier dated entries below are historical.

> **2026-10-21 · 1.9.8.5 发布范围：** 在用户指定的1.9.8上增加BGA播放与黑色透明、选曲循环、曲库与通关显示、小数时间、无划盘7K及第二按键控制权修正。通用固定基线仍为 **1.9.6**。下方旧说明保留当时历史；行为和兼容边界见[1.9.8.5](release-1.9.8.5.zh-CN.md)，验证见[发布检查](release-1.9.8.5-gate.md)，发布状态与下载以[发布页面](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5)为准。

# TenRiff 1.9.6 固定基线

[한국어](baseline-1.9.6.md) | [English](baseline-1.9.6.en.md) | [日本語](baseline-1.9.6.ja.md) | [简体中文](baseline-1.9.6.zh-CN.md)

根据2026-10-19用户的明确要求，通用源码基线从1.9.0提升至 **1.9.6**。不自动跟随main/latest。下方较早日期的说明为历史记录。

提交为 `4d4645d1fc33e4db74d2cb0b235fa6cf1534cd8a`，标签对象为 `2982d2935a360278c2f752884680714c7acc8571`，由PR #105合并。保留已发布标签、资源及旧源码、候选、配置和原谱面。

- [1.9.6](https://github.com/10kseason/TenRiff/releases/tag/1.9.6) · [PR #105](https://github.com/10kseason/TenRiff/pull/105)
- [1.9.6 policy / compatibility](release-1.9.6.zh-CN.md) · [Normal / Hard / EX](judgement-baseline-20261018.zh-CN.md)
- Normal: VEZ/EZ 22ms, NM 19ms, HD 15ms, VHD 10ms PG; original exact GR/GD/BAD. Hard = Normal NM; EX = Normal VHD; no extra scaling or rounding. R9/R10 current, R1–R8 historical.
- S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70; E <70 surviving, F gauge failure. AUTO and -- retained.
- Guest/custom exclusion and client_submitted boundary; BGA weights excluded, NK3 allowlist retained, OFF/empty/Auto defaults; saved profiles/calibration/shortcuts/replay formats retained.
- [1.9.7 follow-up](settings-feedback-20261019.zh-CN.md)

| Published asset | SHA-256 |
| --- | --- |
| TenRiff-1.9.6.zip | `8fc9ab32f49b26dba8f5fe61e01c925742dd592dc207e454ccaf4c829c6907cd` |
| TenRiff-1.9.6-source.zip | `95e03aabb095145d3dccb04d8157870f60f253fc8836ffe88ba4070c32e50f30` |
