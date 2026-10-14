# TenRiff Docs Map

**2026-10-15 · 1.9.0.1:** 마스터 기본 음량 10%, 로비 아이콘 중심 정렬을 포함합니다. 미리듣기 ×0.75, 프로필에서 지정하는 F5/F6 -10/+10, Xbox/XInput 버튼 입력, 파일 선택창 중복 방지, BMS BPM/STOP 순서 수정과 옛 리플레이 호환을 제공합니다. 고정 기준은 1.9.0을 유지합니다. [변경 안내](feedback-20261015.ko.md) · [릴리스 검증 범위](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 새 고정 기준선:** 사용자가 검토한 로컬 후보를 정식 배포 버전으로 올립니다. BMS/텐리프 음악 음량 분리, 로비·필터·스킨 조작, 등급·SUBARTIST 표시, 한글 모드와 EX 판정, 업로드 상태 표시를 포함합니다. 기본/Hard는 R5, EX는 R6이며 R1–R5 기록·기존 설정을 보존합니다. [기준선](baseline-1.9.0.md) · [릴리스 검증 범위](release-1.9.0-gate.md). 아래 날짜별 항목은 당시 릴리스 이력입니다.

**2026-10-14 · 1.8.9.7:** 입력 스레드에 MMCSS Games/High와 전원 스로틀링 해제를 적용하고, 로비 메뉴 테두리를 아이콘 색의 은은한 glow로 표시합니다. 텐리프 자체 메뉴·결과 음악만 기존의 40%로 낮추며 BMS 곡·키음·미리듣기는 원래 음량을 유지합니다. 고정 기준선은 1.8.7이며 물리 입력 지연 개선은 아직 입증되지 않았습니다. [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 입력과 게임 진행/HUD의 짧은 대기에 재사용 고해상도 타이머를 적용하고, RawInput은 메시지가 오면 깨어납니다. B 비교 방식의 제품 적용이며, 수치 판정 창·R1~R5·리플레이·고정 기준 1.8.7을 유지합니다. 실제 체감은 회차마다 달라 모든 움찔의 해결을 보장하지 않습니다. [Scope / 검증](release-1.8.9.6-gate.md).

**2026-10-13 · 1.8.9.5:** 오디오 버퍼에 따라 달라지던 LN·자동 판정 순서를 R5에서 시간순으로 통일하고, 입력·화면 시간과 BPM/STOP 경계의 노트 위치를 맞춥니다. 로비는 어두운 카드와 색상 아이콘으로, 오디오는 마스터 음량을 맨 위로 정리합니다. R1~R4 기록과 고정 기준 1.8.7은 보존합니다. [변경·검증 범위](release-1.8.9.5-gate.md).

**고정 기준선: [1.9.0](baseline-1.9.0.md)** (2026-10-14 사용자 요청). Git tag `1.9.0`을 기준으로 하며 `main`이나 `latest`를 따라 자동 변경하지 않습니다. 1.8.7 기준선과 기존 릴리스는 이력으로 보존합니다.

**2026-10-13 · 1.8.9.4:** 결과의 큰 등급·점수 배치와 상세 정확도를 정리하고 Gauge Shift CLEAR 4단계 색을 구분합니다. 오디오의 BGM·키음 음량을 맨 위로 옮기며 새 기본값은 모두 100%입니다. 한국어 스킨 명칭, 명시된 obj 채보자와 BPM! 변속 표시, 같은 시점의 동일 음원 중복 재생 보정을 포함합니다. 채보자 업로드는 선택 사항이며 기존 기록 식별자는 유지합니다. [변경·호환·검증 범위](release-1.8.9.4-gate.md). 검증과 게임·웹 공개 상태는 해당 문서에 별도로 기록하며 고정 기준은 1.8.7입니다.

**2026-10-13 · 1.8.9.3:** 리절트 점수·등급·정확도와 판정 개수의 크기를 조정하고, 로비 동작 카드 색·은은한 Bloom, 스킨 레인 색을 따르는 키 설정, 곡 목록 레벨 색을 개선했습니다. 인디케이터 배경과 스킨의 대상 간격·비주얼 프리셋 항목을 제거하고 대상 레인을 노트 색상 바로 위로 옮겼습니다. [변경·검증 범위](release-1.8.9.3-gate.md). 고정 기준은 1.8.7이며 1.8.9.2의 BMS 보정과 리플레이 호환성을 유지합니다.

**2026-10-13 · 1.8.9.2:** BMS의 같은 레인·시각에 중복된 일반 노트와 롱노트 시작점을 게임 안에서 롱노트 하나로 보정합니다. 원본 파일, LN 길이와 키음을 보존하며 Native와 변환에 함께 적용합니다. 이전 리플레이는 당시 배치를 유지합니다. [동작·호환 한계](release-1.8.9.2-gate.md). 고정 기준선은 1.8.7입니다.

**2026-10-13 · 1.8.9.1:** 연속 키 설정, 전 변환기 키음·롱노트 보존, FAST/SLOW·타이밍 배경 토글, 게이지 확대, 미리보기 기어 드래그와 판정 테두리·배경 제거를 제공합니다. 기존 리플레이는 원래 변환으로 재생합니다. [동작·검증·호환 한계](release-1.8.9.1-gate.md). 고정 기준선은 1.8.7입니다.

**2026-10-12 · 1.8.9:** 로비의 큰 BEST 숫자·키 모드 색·흰 통합 외곽선, 오른쪽 위 정보·왼쪽 아래 점수·오른쪽 아래 가로 riff map의 Studio HUD, 큰 결과 글자·분석 탭·중앙 정렬한 조작을 제공합니다. S+ 색·글꼴을 유지하고, 메뉴의 Judge Easy/Hard가 새 세션에서 사라지던 전달 경로를 수정했습니다. [동작·검증·남은 범위](release-1.8.9-gate.md), [Judge 원인과 호환성](judge-session-handoff-1.8.9.ko.md)를 참고하세요.

**2026-10-12 · 1.8.8:** 결과 분석, 자유 해상도와 F9 PNG 안내, 긴 LN Mix에 더해 화면 가독성·난이도표 입력/필터·스킨 레인 단축키·키 수 축소 시 키음 보존·단독 시작/즉시 재시작 조작을 정리했습니다. 현재 검증·패키지·공개 상태는 [1.8.8 릴리스 안내](release-1.8.8-gate.md)에서 확인하세요. 고정 기준은 1.8.7을 유지합니다.

**분석과 확장 계획:** 레인별 분포·시간대 정확도·단노트/LN 시작·해제·유지 실패를 구분하고 측정 불가 미스를 0ms 표본에 넣지 않습니다. 긴 LN Mix v2는 로컬 기록·리플레이만 허용하며 기존 BEST·Sites 업로드에서 제외합니다. 플러그인 실행 환경/API/SDK는 후속 계획이며 1.9.0에는 포함하지 않습니다. [분석 기준](result-analysis-1.8.8.ko.md), [화면·메뉴·키음 변경](release-feedback-1.8.8.ko.md), [시작·재시작 조작](gameplay-start-controls.ko.md).

**2026-10-09 · TenRiff 1.8.7:** 입력 보정과 로딩 오디오 경로를 보완하고 LR2 단독 플레이 스튜디오 덱, 750ms FAST/SLOW 유지, 키 재할당, 옵션 통합과 곡 제목 자동 맞춤을 제공합니다. [1.8.7 release](release-1.8.7-gate.md).

**2026-10-08 · TenRiff 1.8.6:** Native 단독 플레이에 스튜디오 덱 HUD를 추가합니다. 좌우 Shift를 플레이 키로 사용할 수 있으며, F5/F6은 배속 절반/두 배, F11은 채팅입니다. 클래식 HUD도 선택할 수 있습니다. [1.8.6 release](release-1.8.6-gate.md).

**2026-10-06 · TenRiff 1.8.5:** VSync OFF 플레이에서 지원되는 노트 이미지를 순서대로 묶어 그려 CPU의 반복 제출을 줄입니다. 호환되지 않는 스킨·롱노트와 VSync ON은 기존 경로를 유지합니다. 로비·메뉴 및 R1~R4 판정·리플레이 규칙은 유지합니다. [1.8.5 release](release-1.8.5-gate.md).

**2026-10-05 · TenRiff 1.8.42:** 고밀도 구간에서 프레임 예산을 조금 넘었을 때 추가로 쉬던 렌더 제한기를 수정하고, 성능 오버레이 OFF 상태의 통계 수집·정렬을 생략합니다. 노트 화질·표시량과 R1~R4 판정·리플레이 규칙은 유지합니다. [1.8.42 release](release-1.8.42-gate.md).

**2026-10-05 · TenRiff 1.8.41:** Judge Hard의 EASY 기준 PG/GR/GD를 ±17.5/55.714286/98.571429ms로 조정합니다. BAD는 모든 BMS RANK에서 Hard ±225ms·Easy ±210ms로 고정합니다. 새 기록은 R4이며 R1/R2/R3 리플레이와 기존 웹 기록을 보존합니다. [1.8.41 release](release-1.8.41-gate.md).

**2026-10-05 · TenRiff 1.8.4:** 일시정지 마우스 조작과 설정 클릭을 개선하고, 실시간 페이스메이커·연습용 오토스크래치·전체 설정 이전을 추가합니다. 고스트의 플레이어 필드 크기, LN 불투명도와 HUD 검색·프레임 대기열 제한을 수정합니다. [1.8.4 release](release-1.8.4-gate.md).

Language: Korean | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

루트 [`README.md`](../README.md)를 먼저 읽었다면, 이 문서는 그 다음 단계의 상세 문서 인덱스입니다. 설계 문서와 현재 상태 문서를 같이 담고 있으므로, 빠르게 맥락을 잡아야 할 때는 아래 순서로 읽는 것이 가장 효율적입니다.

이 프로젝트 코드는 빠른 반복과 실험을 전제로 발전한 `vibe coding` 작품이라는 점을 전제로 읽는 편이 맞습니다.

## 문서 기준과 최신 변경

현재 동작이 충돌하면 같은 커밋의 **구현 코드·회귀 테스트**를 먼저 확인하고 `current-state`와 `config` 문서를 함께 수정합니다. 번역본도 같은 계약을 설명해야 합니다. `baseline-*`와 `release-*`는 해당 버전의 기록이며, 초기 설계·로드맵의 제안은 현재 구현이나 성능 보장이 아닙니다.

- [최신 판정 범위·RANK별 표·롱노트 규칙](judgement-windows.md)
- [1.8.3 release](release-1.8.3-gate.md)
- [1.8.3 audio / LN / RANK](audio-ln-rank.ko.md)
- [Skin timing controls](timing-feedback-controls.ko.md)
- [Note fog / song ending / pause resume](skin-fade-audio-resume.ko.md)
- [1.8.2 release](release-1.8.2-gate.md)
- [1.8.1 release](release-1.8.1-gate.md)
- [1.8.1 Studio](menu-studio-1.8.1.ko.md)
- [1.8.0](release-1.8.0-gate.md)
- [1.7.10](release-1.7.10-gate.md)
- [1.7.9](release-1.7.9-gate.md)
- [1.7.8](release-1.7.8-gate.md)
- [1.7.2 history](release-1.7.2-gate.md)
- [1.7.1 history](release-1.7.1-gate.md)
- [UI](menu-visual-polish.md)
- [Gameplay / Multiplayer](gameplay-polish-followup.md)

## Recommended Reading Order
1. `docs/current-state.md`
   - 현재 제품 상태, 핵심 서브시스템, 검증된 명령, 남은 수동 검증 항목
2. `docs/ui-audit-checklist.md`
   - renderer layout 변경 뒤 반드시 다시 돌려야 하는 UI 수동 검증 매트릭스
3. [`docs/baseline-1.9.0.md`](baseline-1.9.0.md)
   - 현재 작업을 어디서부터 쌓아야 하는지 정하는 `1.9.0 fixed stable baseline` 기준선 문서
4. `docs/gameplay-guide.md`
   - 실제 플레이 기준의 시작 방법, 곡 선택, 조작, HUD, 판정, 결과 화면 안내
5. `docs/multiplayer.md`
   - 최대 8인 직접 IP 멀티플레이의 접속, BMS 전용 공통곡, 회전 선곡권, 보안 및 네트워크 제한
6. `docs/score-integrity.md`
   - replay evidence v3, 공식 로컬 베스트 검증, 레거시 상태, P2P와 서버 권위의 보안 경계
7. `docs/config.md`
   - 실제 설정/프로필/키맵 구조
8. `docs/localization.md`
   - 현재 영어/한국어/일본어 UI 구조와 이후 다국어 확장 시 건드릴 파일/경계 정리
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
- [`docs/baseline-1.9.0.md`](baseline-1.9.0.md)
  - 후속 작업이 유지해야 하는 `1.9.0 fixed stable baseline` 기준선 문서
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
- 어떤 기준선 위에서 작업을 쌓는지 정할 때는 [`docs/baseline-1.9.0.md`](baseline-1.9.0.md)를 같이 봅니다.
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

1.9.0 클라이언트의 Sites 계정 연결, 네 가지 기록 지표, 동점 비교 규칙은 [sites-leaderboard.md](sites-leaderboard.md)를 참고하세요. 이 문서는 공개 클라이언트 프로토콜 설명이며 서버 소스·운영 설정·연결 비밀값을 포함하지 않습니다.
