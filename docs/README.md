# TenRiff Docs Map

Language: Korean | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

루트 [`README.md`](../README.md)를 먼저 읽었다면, 이 문서는 그 다음 단계의 상세 문서 인덱스입니다. 설계 문서와 현재 상태 문서를 같이 담고 있으므로, 빠르게 맥락을 잡아야 할 때는 아래 순서로 읽는 것이 가장 효율적입니다.

이 프로젝트 코드는 빠른 반복과 실험을 전제로 발전한 `vibe coding` 작품이라는 점을 전제로 읽는 편이 맞습니다.

## 문서 기준과 최신 변경

현재 동작이 충돌하면 같은 커밋의 **구현 코드·회귀 테스트**를 먼저 확인하고 `current-state`와 `config` 문서를 함께 수정합니다. 번역본도 같은 계약을 설명해야 합니다. `baseline-*`와 `release-*`는 해당 버전의 기록이며, 초기 설계·로드맵의 제안은 현재 구현이나 성능 보장이 아닙니다.

- [1.7.7](release-1.7.7-gate.md)
- [1.7.2 history](release-1.7.2-gate.md)
- [1.7.1 history](release-1.7.1-gate.md)
- [UI](menu-visual-polish.md)
- [Gameplay / Multiplayer](gameplay-polish-followup.md)

## Recommended Reading Order
1. `docs/current-state.md`
   - 현재 제품 상태, 핵심 서브시스템, 검증된 명령, 남은 수동 검증 항목
2. `docs/ui-audit-checklist.md`
   - renderer layout 변경 뒤 반드시 다시 돌려야 하는 UI 수동 검증 매트릭스
3. `docs/baseline-1.5.1.md`
   - 현재 작업을 어디서부터 쌓아야 하는지 정하는 `1.5.1 fixed stable baseline` 기준선 문서
4. `docs/gameplay-guide.md`
   - 실제 플레이 기준의 시작 방법, 곡 선택, 조작, HUD, 판정, 결과 화면 안내
5. `docs/multiplayer.md`
   - 최대 8인 직접 IP 멀티플레이의 접속, BMS 전용 공통곡, 회전 선곡권, 보안 및 네트워크 제한
6. `docs/score-integrity.md`
   - replay evidence v3, 공식 로컬 베스트 검증, 레거시 상태, P2P와 서버 권위의 보안 경계
7. `docs/config.md`
   - 실제 설정/프로필/키맵 구조
8. `docs/localization.md`
   - 현재 영어/한국어 UI 구조와 이후 다국어 확장 시 건드릴 파일/경계 정리
9. `docs/menu.md`
   - 메뉴/상태머신/곡 선택 흐름
10. `docs/core-loop.md`
   - 플레이 루프와 데이터 흐름
11. `docs/roadmap.md`
   - 중장기 작업 방향
12. `docs/developer-extension-guide.md`
   - 새 mode/mod, UI row, runtime migration, replay/result, 테스트를 어디에 추가해야 하는지 설명하는 개발자 문서

## Which Docs Are Source Of Truth
- `docs/current-state.md`
  - 현재 구현 상태의 요약 문서
- `docs/baseline-1.5.1.md`
  - 후속 작업이 유지해야 하는 `1.5.1 fixed stable baseline` 기준선 문서
- `docs/config.md`
  - 실제 `config/config.json`, `profiles/<name>/config.json`, `keymap.json` 기준

## Historical / Design Docs
- `docs/menu.md`
  - 메뉴/상태머신/저지연 방향성 설계 문서
- `docs/core-loop.md`
  - 플레이 루프 초기 설계 및 데이터 흐름 설명
- `docs/localization.md`
  - 현재 UI 현지화 구조와 향후 다국어 확장용 참고 문서
- `docs/latency.md`, `docs/modes.md`, `docs/gap-analysis.md`, `docs/roadmap.md`
  - 기능별 설계/분석/중장기 방향 문서
- `docs/developer-extension-guide.md`
  - 현재 코드 기준 유지보수/확장 작업 절차 문서

## Translation Coverage
- 루트 `README.md`는 `README.en.md`, `README.zh-CN.md`, `README.ja.md` 번역본을 가집니다.
- `docs/` 안의 주요 문서는 원본 옆에 `.en.md`, `.zh-CN.md`, `.ja.md` suffix 파일로 번역본을 둡니다.
- 번역 문서와 원문이 충돌하면, 현재 동작은 같은 커밋의 구현 코드·회귀 테스트로 확인하고 원문과 번역 문서를 함께 정정합니다.

## Acknowledgements
- OpenAI Codex, ChatGPT, Claude Code, Gemini, 그리고 프로젝트 검증을 도와주신 게스트 테스터분들께 감사드립니다.

## Practical Rule
- 현재 동작을 확인할 때는 `docs/current-state.md`를 우선 봅니다.
- 어떤 기준선 위에서 작업을 쌓는지 정할 때는 `docs/baseline-1.5.1.md`를 같이 봅니다.
- 오래된 설계와 현재 코드가 다를 수 있으므로, 충돌하면 구현 코드·회귀 테스트를 먼저 확인하고 `docs/current-state.md`, `docs/config.md`를 정정합니다.

## Skin Customization

- [Portable skin presets](skin-presets.md): `.trskin` 파일로 현재 스킨 설정과 자산을 다른 PC에 전달
- [곡 소스와 기본 난이도표](library-management.md): 목록에서만 소스 제거, 폴더 추가, 에리/리바이브 표 선택
- [LR2 게이지 비교](lr2-gauge-audit.ko.md): 일반 게이지 차이와 단위인정의 수치·경계·LN 적용
- `docs/skin-agent-guide.md`: AI 에이전트용 스킨 제작 계약, 프롬프트, 검증 체크리스트
- `docs/skin-format.md`: TenRiff `skin.json` v1 로비/인게임 스킨 제작 및 가져오기 가이드
- `docs/ranked-integrity-plan.md`: BMS 전용 검증 랭킹, 기록 보기, 온라인 기록, osu 관련 랭크 제외 단계 계획
- `docs/tenriff-skin.schema.json`: 편집기 자동 완성 및 구조 검증용 JSON Schema

## ASIO와 플레이 설정

[네이티브 ASIO](asio-audio.md), [대표 BPM](reference-bpm.md), [휴대용 스킨 프리셋](skin-presets.md), [곡 소스와 난이도표](library-management.md), [LR2 게이지 비교](lr2-gauge-audit.ko.md)를 참고하세요.

1.7.7 클라이언트의 Sites 계정 연결, 네 가지 기록 지표, 동점 비교 규칙은 [sites-leaderboard.md](sites-leaderboard.md)를 참고하세요. 이 문서는 공개 클라이언트 프로토콜 설명이며 서버 소스·운영 설정·연결 비밀값을 포함하지 않습니다.
