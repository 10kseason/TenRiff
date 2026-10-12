# TenRiff Development Roadmap (staged)

**고정 기준선: [1.8.7](baseline-1.8.7.md)** (2026-10-10). Git tag `1.8.7`, commit `6b060ad47b950ad9e0dbc516fbb40189e8e21347`을 사용하며 `latest`나 새 릴리스로 자동 변경하지 않습니다.

**2026-10-13 · 1.8.9.1:** 연속 키 설정, 전 변환기 키음·롱노트 보존, FAST/SLOW·타이밍 배경 토글, 게이지 확대, 미리보기 기어 드래그와 판정 테두리·배경 제거를 제공합니다. 기존 리플레이는 원래 변환으로 재생합니다. [동작·검증·호환 한계](release-1.8.9.1-gate.md). 고정 기준선은 1.8.7입니다.

> 문서 성격: 초기 설계·개선 방향과 당시 분석을 포함합니다. 미완료 제안이나 과거 수치는 현재 동작·기본값·성능 보장이 아닙니다. 현재 기준은 [현재 상태](current-state.md)와 [설정](config.md)을 확인하세요.

This roadmap captures the recommended high-level order for building out the game loop while avoiding scope creep. Each stage locks in direction before layering extra features.

## Current baseline
- Windows GUI/runtime is the primary supported path.
- The fixed baseline is `1.8.7` and this source targets `1.8.9.1`; public packages bundle NK3 P64 and generalized pattern MLP inference models, but no BGA upscaler model. Selecting a compatible rights-cleared upscaler ONNX only stores its path, and BGA Upscaler remains off until the user enables it and accepts the high-spec warning; there is no automatic benchmark gate.
- The active menu/runtime is BMS-family only (`.bms/.bme/.bml/.pms`) and supports native, bundled/profile TenRiff `skin.json`, and LR2 skins.
- For current shipped behavior, read `docs/current-state.md` first; this roadmap is about direction and remaining work.

## 1.8.8–1.8.9 분석과 1.9.0 안정화 계획 (2026-10-12)

현재 소스는 **1.8.9 릴리스 라인**입니다. 1.8.8의 분석·해상도·LN Mix·키음·시작 조작 위에서 로비·HUD·결과 가독성을 개선하고, 메뉴의 Judge Easy/Hard 선택을 새 세션에 전달하는 수정을 소스에 반영했습니다. 완료한 로컬 검사·성능 측정 결과와 한계, 남은 패키지·CI·공개 단계를 구분합니다. 확정된 동작과 새 검증·공개 상태는 [1.8.9 릴리스 검증](release-1.8.9-gate.md)에 기록합니다. 플러그인 실행 환경·API·SDK는 아직 구현하지 않았으며 `1.9.0`은 향후 안정화 목표입니다. 고정 기준 `1.8.7`, 기존 태그·배포 파일과 1.8.8 기록은 그대로 유지합니다. 뒤의 초기 단계 목록이 미완료 본체 기능 추가를 자동으로 승인하지는 않습니다.

| 단계 | 완료 조건 |
| --- | --- |
| 1.8.8 | 레인별 FAST/SLOW·시간대 정확도·단노트/LN 사건·분포와 표본 수, 자유 해상도·결과 PNG·긴 LN Mix, 화면 가독성·메뉴 입력·키음 보존·단독 시작/재시작을 [릴리스 조건](release-1.8.8-gate.md)에 따라 검증 |
| 1.8.9 | S+ 색을 유지한 로비·Studio HUD·결과 UI와 Judge 세션 전달 수정은 소스에 반영. 극단 필드·성능 오버레이의 상단 정보 숨김을 포함한 [새 릴리스 조건](release-1.8.9-gate.md)의 완료한 로컬 검사·성능 한계와 남은 패키지·CI·공개 단계 구분 |
| 1.9.0 이전 시험판 | 제한된 읽기 전용 플러그인 API로 대체 결과 분석 화면과 개인 선곡 필터를 본체 코드 수정 없이 구현·검증 |
| 1.9.0 안정화 이후 | 본체 기능 범위를 유지하고 버그·보안·호환성 수정과 플러그인 계열 확장을 중심으로 관리 |

- **분석 원칙:** 적용 판정에서 의미 있는 LN 항목만 보여 주며 분석을 위해 판정·점수·콤보·게이지 규칙을 바꾸지 않습니다. 일반 LN의 자동 완료는 해제 타이밍 표본이 아닙니다. 측정할 수 없는 미스는 별도 집계하고 `0 ms` 표본으로 넣지 않습니다. 평균과 함께 분포·표본 수·흩어짐을 보여 주어 `−20 ms / +20 ms`가 서로 상쇄되어도 정확한 타건으로 오해하지 않게 합니다. [분석 기준과 상세 계획](result-analysis-1.8.8.ko.md)을 따릅니다.
- **본체와 플러그인 경계 (계획):** 입력, 오디오 시계, 판정, 점수, 공식 기록 저장·검증, 멀티플레이 규칙·세션은 본체가 소유합니다. 첫 API는 읽기 전용 데이터, 별도 개인 설정·분석 결과 저장, 허용된 결과 화면·선곡 정렬/필터·HUD 영역 및 메뉴 확장만 제공합니다.
- **실행 격리 (계획):** 외부 코드는 별도 실행 환경에서 처리합니다. 게임은 계산 완료를 기다리지 않고 입력·오디오 경로에서 외부 코드를 직접 호출하지 않습니다. 접근 권한 제한과 실행량/시간 제한을 각각 적용하고 오류·무한 루프·과도한 출력을 격리합니다. 실행 기술과 구체적인 예산은 시험판 검증 후 결정합니다.
- **멀티플레이 (계획):** 본체가 외부 플러그인 실행과 결과 적용을 차단하고 플러그인이 영향을 준 플레이 상태를 기본 규칙으로 다시 구성합니다. 플러그인의 자발적 비활성화에 의존하지 않으며 개인 키 배치·오디오 오프셋은 유지합니다. 향후 싱글에서 판정·채보·입력을 바꾸는 확장을 허용한다면 해당 기록을 기본 기록과 분리합니다.
- **API 고정 조건 (계획):** 위 두 샘플을 만들 때마다 엔진을 고쳐야 한다면 API는 아직 고정하지 않습니다. API 문서, 바로 실행되는 샘플, 오류 로그, 플러그인 없이 시작하는 안전 모드를 작은 SDK로 제공합니다. AI와 사용자가 정해진 API와 샘플만으로 모드를 만들 수 있는지 검증합니다.
- **버전 정책 (계획):** `1.9.0`은 기능·호환성의 안정화선이며 숫자와 파일을 영구 동결한다는 뜻이 아닙니다. 본체 수정판, 플러그인, 공개 API 호환성 버전을 구분하고 기존 모드·기록의 호환성을 검증합니다. 향후 고정 기준 변경은 별도 승인과 문서 동기화를 거칩니다.

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
