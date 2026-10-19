# TenRiff 1.9.8 — Skin X/Y positioning and lane-divider brightness

2026-10-19

[한국어](release-1.9.8.ko.md) | [English](release-1.9.8.en.md) | [日本語](release-1.9.8.ja.md) | [简体中文](release-1.9.8.zh-CN.md)

Judgement, Combo, the FAST/SLOW indicator and the FAST/SLOW bar now each expose X and Y in Skin Settings. Restoring the existing hidden X controls preserves their zero defaults and saved coordinates; changing X leaves Y unchanged. Each X moves from −600 to 600px in 10px steps immediately before its Y row. Native/TenRiff/LR2 support localized search, keyboard left/right and pointer −/+.

Lane Divider Brightness uses 0–100% in 1% steps. The default 100% preserves the existing color; 0% is black. Only RGB changes: visibility, alpha, width and measure lines remain unchanged. The optional `skin.lane_divider_brightness` stores `0..1`, default `1`, in profiles and portable skins, and applies to preview, Classic/Studio and ghost rendering.

Reusing existing X storage and rendering avoids resetting or migrating coordinates. The judgement line remains attached to the lanes and follows existing field dragging for horizontal placement. Fixed judgement, score, grades, input, audio and replay contracts, R9/R10 and historical R1–R8, peer v7 and Guest/client_submitted boundaries remain unchanged.

All 1.9.7 behavior remains included: Common Keys and collision checks, larger colored tabs, blue FAST/red SLOW, 1% controls, Game Mode Settings/Normal labels, fresh play/Retry seeds with recorded replay seeds, and 12.5% scene-music reference gain. The general fixed baseline stays 1.9.6, preserving prior releases, candidates, installations and profiles. Website, database and public Skin Editor changes are outside this release.

Verification gates describe required work, not completed results. Build, test, package and download evidence is recorded separately. Physical input, IME, listening, long play, other PCs and arbitrary external skins require separate validation.

[X/Y](skin-xy-20261019.en.md) · [Baseline 1.9.6](baseline-1.9.6.en.md) · [Normal / Hard / EX](judgement-baseline-20261018.en.md) · [Verification](release-1.9.8-gate.md) · [1.9.8 release](https://github.com/10kseason/TenRiff/releases/tag/1.9.8)
