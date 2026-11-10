# TenRiff 1.9.9.9-final4 고정 기준선

[한국어](baseline-1.9.9.9-final4.md) | [English](baseline-1.9.9.9-final4.en.md) | [日本語](baseline-1.9.9.9-final4.ja.md) | [简体中文](baseline-1.9.9.9-final4.zh-CN.md)

2026-11-10 사용자 요청으로 일반 소스 기준선을 1.9.6에서 **1.9.9.9-final4**으로 올렸다. main/latest를 자동 추적하지 않는다. 아래 정책은 기준선이 바뀌어도 그대로 유지한다.

기준 커밋·태그·배포 파일 해시는 [공개 릴리스](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9-final4)와 작업 공간의 `BASELINE.md`에 기록한다. 공개 태그·배포 파일과 과거 소스·후보·프로필·원본 차트를 보존한다.

- [변경과 호환성](release-1.9.9.9-final4.ko.md) · [릴리스 게이트](release-1.9.9.9-final4-gate.md) · [이전 기준선 1.9.6](baseline-1.9.6.md)
- 판정: Normal PG VEZ/EZ 22ms, NM 19ms, HD 15ms, VHD 10ms, 원래의 GR/GD/BAD. Hard = Normal NM, EX = Normal VHD. 추가 배율·반올림 없음. [Normal / Hard / EX](judgement-baseline-20261018.ko.md)
- 리플레이: R1~R14와 custom 변형, BEST 세대, peer 프로토콜의 기존 동작을 바꾸지 않는다. 판정을 바꾸면 새 룰셋 ID를 쓴다.
- 등급: S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70, 생존 70 미만 E, 게이지 실패 F. AUTO와 -- 유지.
- 온라인: Guest/custom 제외와 client_submitted 경계 유지. 가져온 LR2 램프는 표시 전용이며 제출 대상이 아니다.
- 배포: BGA 가중치 미포함, NK3 허용 목록 유지, 기본값 OFF/빈 경로/Auto. 사용자의 LR2 DB 등 개인 데이터는 포함하지 않는다.

## 1.9.6 대비 호환성

1.9.6 이후 추가된 룰셋(R11~R14), LN Mix v3, 키 변환 버전, BMS 타이밍·게이지·RANDOM 정책 필드는 모두 기록에 버전으로 남고 검증기가 과거 정책을 그대로 재현한다. 저장된 프로필·보정값·단축키는 마이그레이션 없이 읽는다. 이 릴리스 자체는 판정·점수·리플레이 계산을 바꾸지 않는다.
