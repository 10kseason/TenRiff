> **2026-10-22 · 1.9.9 · NK2 E / 16K送信:** [1.9.9](release-1.9.9.ja.md) · [Verification](release-1.9.9-gate.md). Baseline **1.9.6**. 4/5/6/7/8/9/10/12/14/16K. Earlier dated entries below are historical.

> **2026-10-21 · 1.9.8.5 リリース範囲:** ユーザー指定の1.9.8にBGA再生・黒の透過、選曲循環・ライブラリ・クリア表示、小数のタイミング、スクラッチなし7Kと第2キー所有権の修正を追加します。一般固定基準は **1.9.6** のままです。以下の旧案内は当時の履歴です。動作と互換性は[1.9.8.5](release-1.9.8.5.ja.md)、検証は[リリースゲート](release-1.9.8.5-gate.md)、公開状態とダウンロードは[リリースページ](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5)を参照してください。

# TenRiff 1.9.6 固定基準

[한국어](baseline-1.9.6.md) | [English](baseline-1.9.6.en.md) | [日本語](baseline-1.9.6.ja.md) | [简体中文](baseline-1.9.6.zh-CN.md)

2026-10-19の明示的なユーザー依頼により、一般ソース基準を1.9.0から **1.9.6** へ更新した。main/latestには自動追従しない。以下の以前の日付の案内は履歴である。

コミットは `4d4645d1fc33e4db74d2cb0b235fa6cf1534cd8a`、タグオブジェクトは `2982d2935a360278c2f752884680714c7acc8571`、PR #105で統合。公開タグ・配布物、以前のソース・候補・プロフィール・原譜面を保持する。

- [1.9.6](https://github.com/10kseason/TenRiff/releases/tag/1.9.6) · [PR #105](https://github.com/10kseason/TenRiff/pull/105)
- [1.9.6 policy / compatibility](release-1.9.6.ja.md) · [Normal / Hard / EX](judgement-baseline-20261018.ja.md)
- Normal: VEZ/EZ 22ms, NM 19ms, HD 15ms, VHD 10ms PG; original exact GR/GD/BAD. Hard = Normal NM; EX = Normal VHD; no extra scaling or rounding. R9/R10 current, R1–R8 historical.
- S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70; E <70 surviving, F gauge failure. AUTO and -- retained.
- Guest/custom exclusion and client_submitted boundary; BGA weights excluded, NK3 allowlist retained, OFF/empty/Auto defaults; saved profiles/calibration/shortcuts/replay formats retained.
- [1.9.7 follow-up](settings-feedback-20261019.ja.md)

| Published asset | SHA-256 |
| --- | --- |
| TenRiff-1.9.6.zip | `8fc9ab32f49b26dba8f5fe61e01c925742dd592dc207e454ccaf4c829c6907cd` |
| TenRiff-1.9.6-source.zip | `95e03aabb095145d3dccb04d8157870f60f253fc8836ffe88ba4070c32e50f30` |
