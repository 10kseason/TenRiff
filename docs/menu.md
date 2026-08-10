# Main Menu Low-Latency Blueprint

The main menu must honor the same low-latency philosophy as gameplay: audio runs as the master clock, inputs are timestamped off-thread, and rendering only consumes snapshots. This blueprint captures the rules and implementation order so menu work does not reintroduce input lag.

## 현재 구현 상태(Windows 메뉴 UI)
- `MenuApp`는 **InputThread(폴링)** → **SPSC 큐** → **메뉴 상태 머신** → **RenderThread(D3D11 윈도우 렌더)** 흐름으로 동작한다.
- `SongIndexerThread`가 백그라운드에서 곡 인덱스를 생성하고 `profiles/<name>/.tenriff/song-index/<source-hash>.json`에 캐시한다.
- 메뉴에서 오디오/그래픽/인풋/모드 설정을 변경하면 프로필 설정 파일에 저장된다.
- `Options -> Profile Setup`은 현재 프로필의 첫 실행 설정 화면을 다시 열어 언어/오디오/입력/그래픽/키맵을 즉시 저장한다.
- 플레이 시작 시에는 현재 구현상 메뉴 스레드를 중지하고 `GameSession`을 별도로 실행한다.
- **Windows 메뉴 UI는 D3D11 + Direct2D/DirectWrite 기반**으로 타이틀/곡선택(시안 레이아웃)과 기타 설정 화면(리스트 UI)을 렌더링한다.
- Skins는 native/LR2 전용이다. LR2 playskin 하나를 선택하거나 드롭하면 활성 프로필로 이식하고, `LR2files` 또는 `Theme`을 선택하면 정확한 `IIDX` 폴더와 IIDX 자산 의존 테마를 제외한 바로 아래 테마를 각각 설치한다. 형제 테마 참조를 유지하며 기존 폴더는 덮어쓰지 않는다.
- Song Select 재인덱싱 중 stage/percent/ETA와 상단 progress bar를 게임플레이 외 모든 화면에 표시한다.
- Browse에서 로컬 header JSON을 고르거나 클립보드의 http(s) BMSTable HTML/header 링크를 가져오면 현재 source를 재인덱싱해 hash 일치 곡에 표 레벨을 적용한다.
- Graphics의 `BGA` 토글은 게임플레이 이미지/영상을 완전히 끄며 선곡 미리보기는 유지한다. ONNX 모델 선택은 경로만 저장하므로 `BGA Upscaler`를 별도로 켜고 고사양 경고를 확인해야 한다.
- 입력 키 요약:
  - Title: `↑/↓` 이동, `Enter` 선택(PLAY/EDIT/OPTIONS/EXIT), `F2` 곡 폴더 선택, `F5` 새로고침, `Esc` 종료
    - 곡이 하나도 인덱싱되지 않았으면 첫 버튼은 `PLAY` 대신 `Add Songs Folder`로 보인다.
  - Song Select: `↑/↓` 곡 이동, `PgUp/PgDn` 페이지 이동, `←/→` 좌측 메뉴 포커스 전환, `Tab` 빠른 설정 진입, 빠른 설정에서 `↑/↓` 항목 선택·`←/→` 값 조정, `Enter` 선택/플레이, `-`/`+` Rate 조정, `Esc` 타이틀 복귀
    - 좌측 메뉴는 `Songs / Sources / Search / Filter / Records / Session Mix / Options`를 제공한다.
    - `Backspace`는 `Sources` 또는 `Records`에서 `Songs`로 되돌아갈 때만 쓴다.
  - Session Mix: `←/→`로 15/30/60분 목표를 고르고 `Enter`로 시작한다.
    - 현재 Song Select 검색·필터 결과만 후보로 사용하며 같은 차트는 한 번만 선택한다.
    - 로컬 최고 기록의 클리어 난이도를 기준으로 워밍업·도전·마무리 순서를 구성한다.
    - 곡 길이는 인덱스에 없으므로 목표 시간은 차트당 약 3분을 기준으로 계산한다.
  - Settings/Mode: `↑/↓` 항목 이동, `←/→` 값 변경, `Enter/Esc` 복귀
    - 긴 설정 목록은 우측 스크롤바를 클릭해 해당 위치로 바로 이동할 수 있으며, 클릭만으로 값이 바뀌지는 않는다.
    - 화면 공간 때문에 설명 일부가 생략되면 마지막 설명 줄에 `F1`과 남은 도움말 줄 수를 표시한다.
  - Keymap: `↑/↓` 선택, `Enter` 바인딩 캡처, `Esc` 복귀
    - 캡처 성공 시 즉시 `keymap.json`에 저장된다.
    - Song Select에서 열면 선택한 차트의 lane count를 우선 기준으로 편집 모드를 잡는다.
  - Result: 일반 플레이는 `Enter`로 곡 선택에 복귀한다. Session Mix 중에는 `Enter`로 다음 곡을 진행하고 `Esc`/`Backspace`로 세션을 종료한다.
  - Shared utility keys: `F1` 도움말, `F2` songs-folder browse, `F5` refresh/reindex, `F9` screenshot

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
