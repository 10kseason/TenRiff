# 메뉴 구조와 조작

이 문서의 현재 동작은 1.7.1 클라이언트를 기준으로 합니다. 아래 접힌 초기 설계안은 구현 완료 목록이 아닙니다.

## 현재 화면과 조작

- 홈: `Play / Multiplayer / Options / Exit`. 곡이 없으면 첫 버튼은 `Add Songs Folder`입니다.
- 선곡: 상단 `Songs / Sources / Records / Session Mix / Options`, 중앙 하단 `Search / Sort·Filter / Difficulty Table`을 사용합니다. 예전 좌측 KEY/메뉴 레일 안내는 현재 화면과 다릅니다.
- 난이도표 카드: 이름·URL 영역은 URL 편집창, `File`은 로컬 JSON 선택, `Reset`은 기본 LV 복귀입니다. `Enter` 적용, `Esc` 취소이며 잘못된 주소는 기존 표를 유지합니다. Filters의 난이도표 행도 같은 가져오기 경로를 사용합니다.
- 옵션: 5열×2행의 10개 카드입니다. Key Mode, Keymap, Skins, Graphics, Audio, Input, Calibration, Profile Setup, Mods, Key Test로 이동합니다.
- 공통 설정: 방향키로 선택·조절하고 `Enter`는 선택 항목의 동작을 실행합니다. `Esc / Backspace`는 뒤로 이동합니다. 긴 사용 안내는 페이지 버튼으로 읽습니다. 스킨 설정은 실시간 미리보기를 유지합니다.
- 키맵: 4K–10K, 12K, 14K, 16K를 지원하며 성공한 바인딩은 즉시 저장합니다.
- 결과: 싱글 결과 연출 중 `Space`로 건너뜁니다. 준비 후 `R / Left` 재시도, `F1` 리플레이, `Enter / Esc / Backspace` 복귀입니다. Session Mix의 Enter는 다음 곡, Esc/Backspace는 세션 종료입니다. 멀티 결과는 로비로 복귀합니다.
- F1/F2/F5 같은 기능키는 화면별 역할이 다릅니다. 정확한 조작은 [플레이 안내](gameplay-guide.md)를 참고하세요.

## 소유권과 데이터 흐름

- [MenuApp](../src/app/MenuApp.cpp)은 InputThread의 RawInput/폴링 키 이벤트와 윈도우의 포인터 이벤트를 처리합니다. [MenuNavigator](../src/app/menu/MenuNavigator.h)가 화면과 Back 이력을 소유합니다.
- 각 설정 controller가 선택·수정 상태를 소유하고 MenuApp이 저장, 파일 선택, 재인덱싱, 장치 재시작을 실행합니다. [MenuScreenDescriptor](../src/app/menu/MenuScreenDescriptor.h)가 화면 메타데이터와 view 경로를 정의합니다.
- 렌더러는 불변 snapshot을 소비합니다. 메뉴의 음악은 [MenuMusicController](../src/app/MenuMusicController.cpp), 선곡 미리듣기 상태는 [SongSelectScreen](../src/app/SongSelectScreen.h)이 소유합니다.
- [launch_gameplay](../src/app/MenuAppTail.inl)은 메뉴 입력과 미리듣기를 멈추고 GameSession을 시작합니다. 게임플레이의 오디오·입력 생명주기는 GameSession이 관리합니다. 메뉴부터 같은 오디오 장치를 계속 공유한다는 초기 제안은 현재 계약이 아닙니다.
- 곡 인덱싱은 SongIndexerThread가 수행하며 캐시는 `profiles/<name>/.tenriff/song-index/<source-hash>.json`입니다. 실제 경로와 설정은 [현재 상태](current-state.md), [설정](config.md)을 참고하세요.

## 유지보수

[메뉴 리팩터 계획](menu-refactor-plan.md)의 Phase 0–6은 완료됐습니다. 새 기능은 기존 controller·명시적 화면 경로에 추가하고 관련 테스트와 [UI 확인표](ui-audit-checklist.md)를 사용합니다. 화면별 실제 시각 구조는 [메뉴 UI](menu-visual-polish.md), 멀티·판정은 [1.7.1 후속 변경](gameplay-polish-followup.md)에 정리되어 있습니다.

<details>
<summary>초기 설계안 — 현재 구현 계약 아님</summary>

## Non-negotiable rules
- **Keep the audio device open from the menu.** Initialize the audio backend on menu entry and run silent callbacks (zero buffers) so `playhead_samples`/`buffer_start_samples` stay valid before gameplay begins. Avoid reopening devices when starting a song to prevent warm-up jitter.
- **Menu input uses InputThread + SPSC only.** Consume UI actions from the same RawInput/evdev ingestion path. Never let the render/UI event loop timestamp inputs directly.
- **Audio thread stays allocation/I/O/lock free.** Do not introduce file I/O, heap allocs, or locks in audio callbacks for menu previews.
- **Render is read-only.** It consumes snapshots and never mutates authoritative timing or timestamps inputs.
- **Heavy work is offloaded.** Folder scans, metadata parsing, and replay/result saves run on background jobs so the UI thread never blocks.

## State machine skeleton
States render UI and consume already-timestamped input events; heavyweight work is delegated to jobs.
- `TitleState`
- `SongSelectState`
- `GameplayState` (chart playback)
- `ResultState`
- Later: `SettingsState`, `KeymapState`, `LatencyToolsState`

### Flow
`Title → SongSelect → Gameplay → Result` is the minimal playable loop. Each transition should reuse the live audio clock and keep InputThread running.

## Song select without hitching
- **SongIndexerThread** scans BMS-family files for path/title/artist/BPM/key count/mode/preview audio. Stage/percent/ETA and a top progress bar stay visible on every non-gameplay screen, including the first folder load.
- **Cache index** (`song_index.json` or SQLite) with mtime/hash checks to avoid full rescans. First run can be slow; subsequent runs should be instant.
- **Preview audio** is decoded off-thread and mixed by AudioThread. Explicit previews are preferred; fragmented BMS charts fall back to a bounded BGM/keysound event mix.
- Empty-state screens should expose a persistent `Add Songs Folder` action; external folders and BMS files also support drag-and-drop.
- Browse의 Difficulty Table에서 http(s) BMSTable 페이지/header 링크를 클립보드에 복사하고 `Enter`를 누르면 profile cache로 가져온다. `Right`는 로컬 header JSON 선택, `Left`는 해제이며 변경 시 MD5/SHA-256 일치 레벨을 다시 적용한다.
- Song Select `-` / `+` changes and saves `speed.rate` immediately unless search text entry is active.
- RawInput 스레드나 이벤트 전달이 메뉴에서 비정상적으로 멎으면 멀티플레이/리절트를 포함한 메뉴 화면 전체에서 현재 세션만 자동으로 폴링으로 폴백하고, 저장된 backend 설정은 바꾸지 않는다.

## Settings: latency-first surface
Put these on the first page so users see latency-critical toggles immediately:
- Audio backend (wasapi/asio, alsa/jack)
- Sample rate (48 kHz recommended)
- Buffer size (128/192/256) with optional adaptive step-up
- RawInput/evdev grab toggle (warn when off)
- VSYNC off / driver frame-queue guidance
- `input_offset_ms` and separate `visual_offset_ms`
- HUD toggles (latency overlay/xrun/late counter)

## Key remap and NKRO test
- Capture the **next input event** from InputThread to bind keys; never block by polling the render loop.
- Keep a per-key state machine (UP/DOWN) so duplicate DOWNs while DOWN and UPs while UP are dropped; preserve real down→up→down transitions so fast taps and releases do not get swallowed.
- Successful key captures should save immediately; there is no separate hidden save chord.
- The NKRO test remains a visible tool screen, but not a hidden keyboard shortcut.
- NKRO test shows current pressed set and highlights ghosting/missing keys in real time using the same input events.

## Transition into gameplay without lag spikes
1) **Preload stage (in menu):** load/normalize chart into sample positions; pre-decode/preload keysounds.
2) **Warm start (on entry):** with audio already running, schedule `song_start_samples` a few buffers in the future relative to `buffer_start_samples`.
3) **Start:** at that sample time, render/judgement/keysound paths attach so the first note feels locked-in.

## Result screen hygiene
- Show results immediately; run replay/log saves as background jobs with a "Saving…" indicator.
- Replays store `{lane, state, sample}` so latency bugs can be reproduced deterministically.

## Recommended implementation order
1) Pick UI framework (e.g., SDL + ImGui or custom) ensuring input/timing remain under the existing pipeline.
2) Implement the state machine and four screens (`Title/SongSelect/Gameplay/Result`) so navigation works end-to-end.
3) Add SongIndexerThread + cached index + responsive SongSelect UI.
4) Surface latency-first settings and apply live where possible; note when backend changes require restart.
5) Add key remap + NKRO test following the input pipeline rules.

</details>
