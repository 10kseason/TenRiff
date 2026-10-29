> **2026-10-29 · 1.9.9.7:** [일반 롱노트 조기 해제 수정 및 온라인 제출](release-1.9.9.7.ko.md). R11/R12 online submission; R9/R10 records and submissions preserved. General fixed baseline: **1.9.6**. Earlier local/unpublished notices are historical.

> **2026-10-29 · 1.9.9.6:** [인디케이터·리절트 및 라이브러리 로딩](release-1.9.9.6.ko.md). Fixed baseline: **1.9.6**. Earlier local/unpublished entries below are historical. [Verification](release-1.9.9.6-gate.md).

> **2026-10-28 · 1.9.9.5:** 심플 롱노트 단색·전체 올콤보 이미지·BGA/렌더 대기 복구·EX 프리즘과 랭크 텍스트 효과를 통합하고, 키 오버레이는 난이도 추정 없이 KPS/키 뷰어를 유지합니다. 일반 고정 기준선은 **1.9.6**입니다. 아래의 이전 버전·로컬 후보·미공개·검증 진행 중 표기는 작성 당시의 이력이며 현재 공개/검증 상태를 뜻하지 않습니다. [Release scope](release-1.9.9.5.ko.md) · [Verification](release-1.9.9.5-gate.md).

> **2026-10-27 · 1.9.9.3:** 조절 키 가속·LR2CRS 코스·BMS/KPS 오버레이·클리어 카드·랭크 색·EX 무지개 수정을 포함합니다. 일반 고정 기준선은 **1.9.6**입니다. 아래 이전 버전과 로컬 후보 설명은 당시 이력이며 현재 범위는 이 문서가 우선합니다. [1.9.9.3](release-1.9.9.3.ko.md) · [Verification](release-1.9.9.3-gate.md).

> **2026-10-24 · 1.9.9.1 릴리스 범위:** 21:9 화면·롱노트 해제 표시와 마지막 꼬리 대기·같은 레인 지뢰 앞뒤 50ms를 비우는 LN Mix v3. [변경과 호환성](release-1.9.9.1.ko.md) · [검증 게이트](release-1.9.9.1-gate.md). 일반 고정 기준선은 **1.9.6**입니다. v1/v2 리플레이는 유지하며 v3는 1.9.9.1 검증기가 필요합니다. 아래 이전 안내는 당시 이력입니다. 공개 여부와 다운로드는 [1.9.9.1 릴리스](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.1)가 기준입니다.

> **2026-10-22 · 1.9.9 · NK2 E / 16K 제출:** [1.9.9](release-1.9.9.ko.md) · [Verification](release-1.9.9-gate.md). Baseline **1.9.6**. 4/5/6/7/8/9/10/12/14/16K. Earlier dated entries below are historical.

> **2026-10-21 · 1.9.8.5 릴리스 범위:** 사용자 지정 1.9.8에 BGA 재생·검정 투명색, 선곡 순환·라이브러리·클리어 표시, 소수점 타이밍, 스크래치 없는 7K와 보조키 소유권 수정을 더합니다. 일반 고정 기준선은 **1.9.6**입니다. 아래 이전 안내는 당시 이력이며, 현재 범위와 호환 경계는 [1.9.8.5](release-1.9.8.5.ko.md), 실제 검증은 [릴리스 게이트](release-1.9.8.5-gate.md)를 따릅니다. 공개 여부·다운로드는 [릴리스 페이지](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5)가 기준입니다.

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
