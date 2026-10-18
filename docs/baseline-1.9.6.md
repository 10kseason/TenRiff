# TenRiff 1.9.6 고정 기준선

[한국어](baseline-1.9.6.md) | [English](baseline-1.9.6.en.md) | [日本語](baseline-1.9.6.ja.md) | [简体中文](baseline-1.9.6.zh-CN.md)

2026-10-19 사용자 요청으로 일반 소스 기준선을 1.9.0에서 **1.9.6**으로 올렸다. main/latest를 자동 추적하지 않는다. 아래의 이전 날짜 안내는 당시 이력이다.

기준 커밋은 `4d4645d1fc33e4db74d2cb0b235fa6cf1534cd8a`, 태그 객체는 `2982d2935a360278c2f752884680714c7acc8571`, 병합은 PR #105다. 공개 태그·배포 파일과 과거 소스·후보·프로필·원본 차트를 보존한다.

- [1.9.6](https://github.com/10kseason/TenRiff/releases/tag/1.9.6) · [PR #105](https://github.com/10kseason/TenRiff/pull/105)
- [1.9.6 policy / compatibility](release-1.9.6.ko.md) · [Normal / Hard / EX](judgement-baseline-20261018.ko.md)
- Normal: VEZ/EZ 22ms, NM 19ms, HD 15ms, VHD 10ms PG; original exact GR/GD/BAD. Hard = Normal NM; EX = Normal VHD; no extra scaling or rounding. R9/R10 current, R1–R8 historical.
- S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70; E <70 surviving, F gauge failure. AUTO and -- retained.
- Guest/custom exclusion and client_submitted boundary; BGA weights excluded, NK3 allowlist retained, OFF/empty/Auto defaults; saved profiles/calibration/shortcuts/replay formats retained.
- [1.9.7 follow-up](settings-feedback-20261019.ko.md)

| Published asset | SHA-256 |
| --- | --- |
| TenRiff-1.9.6.zip | `8fc9ab32f49b26dba8f5fe61e01c925742dd592dc207e454ccaf4c829c6907cd` |
| TenRiff-1.9.6-source.zip | `95e03aabb095145d3dccb04d8157870f60f253fc8836ffe88ba4070c32e50f30` |
