# TenRiff Development Roadmap (staged)

**2026-10-15 · 1.9.0.2 릴리스 후보 — 공개 검증 절차:** 공개 1.9.0.1을 바탕으로 Options 초록색, 기본 제공 5K Aery 표에서만 초록 ⑤ 표시·등록 4/6/8/10키 표 기호 색상·레벨 빨강·점수 흰색, `lv:` 레벨 검색, 새 debounce 기본값 0ms와 오디오 음량 1% 단위를 적용합니다. 마스터 기본 10%와 고정 기준 1.9.0을 유지합니다. [변경·사용 안내](feedback-1.9.0.2.ko.md). 실제 공개 여부와 다운로드는 릴리스 페이지가 기준입니다. [릴리스 범위·검증](release-1.9.0.2-gate.md).

**2026-10-15 · 1.9.0.1:** 마스터 기본 음량 10%, 로비 아이콘 중심 정렬을 포함합니다. 미리듣기 ×0.75, 프로필에서 지정하는 F5/F6 -10/+10, Xbox/XInput 버튼 입력, 파일 선택창 중복 방지, BMS BPM/STOP 순서 수정과 옛 리플레이 호환을 제공합니다. 고정 기준은 1.9.0을 유지합니다. [변경 안내](feedback-20261015.ko.md) · [릴리스 검증 범위](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 새 고정 기준선:** 사용자가 검토한 로컬 후보를 정식 배포 버전으로 올립니다. BMS/텐리프 음악 음량 분리, 로비·필터·스킨 조작, 등급·SUBARTIST 표시, 한글 모드와 EX 판정, 업로드 상태 표시를 포함합니다. 기본/Hard는 R5, EX는 R6이며 R1–R5 기록·기존 설정을 보존합니다. [기준선](baseline-1.9.0.md) · [릴리스 검증 범위](release-1.9.0-gate.md). 아래 날짜별 항목은 당시 릴리스 이력입니다.

**2026-10-14 · 1.8.9.7:** 입력 스레드에 MMCSS Games/High와 전원 스로틀링 해제를 적용하고, 로비 메뉴 테두리를 아이콘 색의 은은한 glow로 표시합니다. 텐리프 자체 메뉴·결과 음악만 기존의 40%로 낮추며 BMS 곡·키음·미리듣기는 원래 음량을 유지합니다. 고정 기준선은 1.8.7이며 물리 입력 지연 개선은 아직 입증되지 않았습니다. [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 입력과 게임 진행/HUD의 짧은 대기에 재사용 고해상도 타이머를 적용하고, RawInput은 메시지가 오면 깨어납니다. B 비교 방식의 제품 적용이며, 수치 판정 창·R1~R5·리플레이·고정 기준 1.8.7을 유지합니다. 실제 체감은 회차마다 달라 모든 움찔의 해결을 보장하지 않습니다. [Scope / 검증](release-1.8.9.6-gate.md).

**고정 기준선: [1.9.0](baseline-1.9.0.md)** (2026-10-14 사용자 요청). Git tag `1.9.0`을 기준으로 하며 `main`이나 `latest`를 따라 자동 변경하지 않습니다. 1.8.7 기준선과 기존 릴리스는 이력으로 보존합니다.

**2026-10-13 · 1.8.9.4:** 결과의 큰 등급·점수 배치와 상세 정확도를 정리하고 Gauge Shift CLEAR 4단계 색을 구분합니다. 오디오의 BGM·키음 음량을 맨 위로 옮기며 새 기본값은 모두 100%입니다. 한국어 스킨 명칭, 명시된 obj 채보자와 BPM! 변속 표시, 같은 시점의 동일 음원 중복 재생 보정을 포함합니다. 채보자 업로드는 선택 사항이며 기존 기록 식별자는 유지합니다. [변경·호환·검증 범위](release-1.8.9.4-gate.md). 검증과 게임·웹 공개 상태는 해당 문서에 별도로 기록하며 고정 기준은 1.8.7입니다.

**2026-10-13 · 1.8.9.3:** 리절트 점수·등급·정확도와 판정 개수의 크기를 조정하고, 로비 동작 카드 색·은은한 Bloom, 스킨 레인 색을 따르는 키 설정, 곡 목록 레벨 색을 개선했습니다. 인디케이터 배경과 스킨의 대상 간격·비주얼 프리셋 항목을 제거하고 대상 레인을 노트 색상 바로 위로 옮겼습니다. [변경·검증 범위](release-1.8.9.3-gate.md). 고정 기준은 1.8.7이며 1.8.9.2의 BMS 보정과 리플레이 호환성을 유지합니다.

**2026-10-13 · 1.8.9.2:** BMS의 같은 레인·시각에 중복된 일반 노트와 롱노트 시작점을 게임 안에서 롱노트 하나로 보정합니다. 원본 파일, LN 길이와 키음을 보존하며 Native와 변환에 함께 적용합니다. 이전 리플레이는 당시 배치를 유지합니다. [동작·호환 한계](release-1.8.9.2-gate.md). 고정 기준선은 1.8.7입니다.

**2026-10-13 · 1.8.9.1:** 연속 키 설정, 전 변환기 키음·롱노트 보존, FAST/SLOW·타이밍 배경 토글, 게이지 확대, 미리보기 기어 드래그와 판정 테두리·배경 제거를 제공합니다. 기존 리플레이는 원래 변환으로 재생합니다. [동작·검증·호환 한계](release-1.8.9.1-gate.md). 고정 기준선은 1.8.7입니다.

> 문서 성격: 초기 설계·개선 방향과 당시 분석을 포함합니다. 미완료 제안이나 과거 수치는 현재 동작·기본값·성능 보장이 아닙니다. 현재 기준은 [현재 상태](current-state.md)와 [설정](config.md)을 확인하세요.

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## Current baseline
- Windows GUI/runtime is the primary supported path.
- The fixed baseline is `1.9.0` and this source targets release candidate `1.9.0.2`; public packages bundle NK3 P64 and generalized pattern MLP inference models, but no BGA upscaler model. Selecting a compatible rights-cleared upscaler ONNX only stores its path, and BGA Upscaler remains off until the user enables it and accepts the high-spec warning; there is no automatic benchmark gate.
- The active menu/runtime is BMS-family only (`.bms/.bme/.bml/.pms`) and supports native, bundled/profile TenRiff `skin.json`, and LR2 skins.
- For current shipped behavior, read `docs/current-state.md` first; this roadmap is about direction and remaining work.

## 1.9.0.2 릴리스 후보 검증 범위

아래 수치는 최종 Aery ⑤ 복원과 등록 표 기호 색상 보완 전 후보의 검증입니다. 개발 빌드와 ZIP 추출 소스의 독립 빌드 모두 Release 빌드, CTest 3/3(각 1,224개, integration skips 0), 에디터 35/35, native MenuApp/GameSession 307/307을 통과했습니다. native 검사는 검색 키 처리 22개를 포함합니다. 복원 전 렌더는 6개 fixture/360 Presents/86 geometry hit checks를 통과했습니다. 최종 Aery ⑤와 4UE/6UE/8UE/Ⓡ 색상 보완 후 빌드 및 표시 검증은 릴리스 검증 절차에서 별도로 확인합니다. Aery FHD/작은 창, 숫자 레벨의 빨강 유지, 기본 Lv·다른 표·사용자 표의 예외도 포함합니다.

첫 패키징에서는 기존 스킨 레인 숫자 단축키 검사가 한 차례 실패했습니다. 코드 변경 없이 같은 테스트 바이너리의 재검사, 전체 CTest 재실행 및 독립 빌드 검사는 통과했지만 **근본 원인은 미확정**입니다. 최초 실패도 보존하며 재실행 통과를 원인 해결로 해석하지 않습니다.

로컬 패키지·독립 빌드 근거와 초기 실패는 별도 evidence에 보존합니다. 최종 공개 패키지 대응·PR/main CI·공개 자산 검사는 [릴리스 검증 절차](release-1.9.0.2-gate.md)를 따르며 이 문서에서 미리 통과로 표시하지 않습니다. 실제 공개 상태는 릴리스 페이지가 기준입니다. 내부 키 dispatch·geometry 검사는 물리 장치 입력을 대신하지 않으며 물리 장치·지연·장시간 플레이·다른 PC는 미검증입니다. 고정 기준은 1.9.0입니다.

## 1.9.0.1 이후 확인할 범위

이번 배포는 미리듣기·배속 키·파일 선택창·BMS 타이밍·XInput 버튼 지원에 한정한다. 다음 검증은 실제 패드 연결과 장시간 플레이이며, 타이밍 버전 2의 웹 지원은 별도 작업이다. 웹이 두 타이밍을 구분하기 전에는 영향받는 새 기록을 로컬로 유지한다. 고정 기준 1.9.0과 플러그인·구조 분리의 기존 범위는 유지한다.

## 1.9.0 안정화 범위와 이후 계획

1.9.0은 현재 검증 후보의 배포와 고정 기준선 전환이다. 위의 음량·설정·화면·크레딧·EX/R6·업로드 진단이 이번 범위이며, 모든 PC의 지연 해결이나 새로운 플러그인 기능 완료를 뜻하지 않는다.

이전 1.8.8–1.8.9 계획의 플러그인 선행 조건은 이번 사용자 지정 배포 범위로 대체한다. 플러그인 실행 환경/API/SDK와 GameSession의 큰 구조 분리는 아직 구현하지 않은 별도 후속 작업이다. 향후 플러그인은 입력·오디오·판정·공식 기록의 소유권을 침범하지 않는 읽기 전용 확장부터 검토한다. 본체 버그·호환성·보안 개선은 계속하며 다음 기준선 변경은 다시 명시적 요청과 문서·호환 검증을 필요로 한다.


## 0) Fix the skeleton and master clock
- Treat **AudioThread as the master clock** for all timing-sensitive work.
- **InputThread** should timestamp events from RawInput/evdev and push them into an SPSC queue.
- Normalize chart timelines into **sample positions (int64)** so the audio thread can consume deterministic timestamps.
- **Render** only consumes snapshots to draw; judgements/scores are finalized on the audio side.

## 0.5) Harden the low-latency loop
- Implement one backend end-to-end (WASAPI/ALSA) with device padding exposure and explicit `buffer_start_samples` computation so the mixer works in the playback buffer domain.
- Fortify ClockSync with outlier rejection, sliding-window EMA updates, reset hooks on device changes/underruns, and monotonic clamping to prevent regressions during drift or spikes.
- In AudioThread, pop inputs, convert to sample time, and branch late/normal/future for keysound placement so late inputs still make sound and future ones are staged.
- Express judgement windows in samples, add HUD counters for callback budget/late inputs/xruns, and scale windows appropriately when rate is adjustable.
- ✅ Capture replays as sample-time input traces (lane/state/sample) and write JSON exports for deterministic reproduction.

## 1) Make a full song playable end-to-end
- Keep the audio backend running from the menu with silent callbacks so the master clock is stable before gameplay.
- ✅ Stand up the UI state machine (console): **Title → Song Select → Play → Result** with InputThread/SPSC ingestion.
- ✅ Windows D3D11 menu UI added (text + background + focus styling).
- ✅ Add an async SongIndexerThread plus cached index (mtime/hash) so Song Select stays responsive while scanning.
- Minimal BMS loader (essential channels only) → note scheduling → judgement → result screen.
- Use the audio engine to schedule preview audio (no UI-thread playback) and preload keysounds during Song Select.

## 1.5) Harden BMS-only chart support
- The former multi-format direction is superseded for the current release line; keep loading, indexing, replay, result, and difficulty-table behavior focused on the BMS family.
- Continue hardening real-pack encoding, keysound, BGA, long-note, and lane-layout compatibility without reintroducing archive or alternate-format import paths.
- Keep optional integrations user-supplied, disabled until explicitly enabled, and safely recoverable to native behavior.

## 2) Key remap plus 8K/10K modes
- ✅ Key remapping UI per the “리맵 UI 플로우” spec (including NKRO test).
- Once this lands, the project becomes a solid personal practice tool.

## 3) Add lane-transform and random modes
- ✅ **Mirror**, **Full Random (FR)**, and **Super Random (SR)** are implemented; defer **AR** until its behavior is specified.

## 4) Attach a launcher
- Handle folder checks, first-run config creation, and error code cataloging.
- Completing this makes the game self-contained on a local PC.

## 5) Build trusted records before public ranking
- Follow [`ranked-integrity-plan.md`](ranked-integrity-plan.md): shared ranked-eligibility reason codes, a dedicated Local Records screen, then read-only Online Records, shadow submission, and finally public verified rankings.
- The server must recompute approved BMS results from chart SHA-256 plus replay evidence; client score claims are never authoritative.
- `.osu` charts and osu-derived import, ruleset, scoring, or conversion paths are not eligible for ranked registration. Auxiliary OD8 statistics on an otherwise native BMS result remain local metadata only.
- Development slice complete: a separate headless server exposes a read-only schema-v1 BMS leaderboard snapshot and Song Select Records can switch between Local and Online without blocking local play.
- `1.5.1` fixes the self-hosted implementation as a release baseline under [`release-1.5.1-gate.md`](release-1.5.1-gate.md); operating a project-owned central official server remains separately gated.

## 6) Refactor the menu architecture as it grows
- Phases 0-6 were completed for `1.6.0`: explicit navigation, typed controllers for every settings family, and exhaustive screen descriptors are now the maintained architecture.
- Follow the staged design and per-phase gates in [`menu-refactor-plan.md`](menu-refactor-plan.md); implementation must not begin with a whole-menu rewrite.
- Trigger this work when adding a screen requires coordinated edits across the input dispatcher, render-data builder, and multiple shared cursor/return flags, or when screen behavior can no longer be covered by focused unit tests.
- First add characterization tests for navigation, settings persistence, and pointer/keyboard parity. Then move one screen family at a time out of `MenuApp` into a screen-local state/controller without changing visible behavior.
- Replace shared return-screen flags with an explicit navigation stack/back policy, and model adjustable rows (including sliders) with typed value ranges and persistence callbacks instead of row-index conditionals.
- Keep rendering DTO construction separate from input/state mutation. Finish each extraction with focused tests plus the normal client build before starting the next family; do not perform a whole-menu rewrite.
