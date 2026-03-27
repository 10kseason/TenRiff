# Main Menu Low-Latency Blueprint

The main menu must honor the same low-latency philosophy as gameplay: audio runs as the master clock, inputs are timestamped off-thread, and rendering only consumes snapshots. This blueprint captures the rules and implementation order so menu work does not reintroduce input lag.

## 현재 구현 상태(Windows 메뉴 UI)
- `MenuApp`는 **InputThread(폴링)** → **SPSC 큐** → **메뉴 상태 머신** → **RenderThread(D3D11 윈도우 렌더)** 흐름으로 동작한다.
- `SongIndexerThread`가 백그라운드에서 곡 인덱스를 생성하고 `profiles/<name>/.tenriff/song-index/<source-hash>.json`에 캐시한다.
- 메뉴에서 오디오/그래픽/인풋/모드 설정을 변경하면 프로필 설정 파일에 저장된다.
- 플레이 시작 시에는 현재 구현상 메뉴 스레드를 중지하고 `GameSession`을 별도로 실행한다.
- **Windows 메뉴 UI는 D3D11 + Direct2D/DirectWrite 기반**으로 타이틀/곡선택(시안 레이아웃)과 기타 설정 화면(리스트 UI)을 렌더링한다.
- 입력 키 요약:
  - Title: `↑/↓` 이동, `Enter` 선택(PLAY/EDIT/OPTIONS/EXIT), `F2` 곡 폴더 선택, `F5` 새로고침, `Esc` 종료
    - 곡이 하나도 인덱싱되지 않았으면 첫 버튼은 `PLAY` 대신 `Add Songs Folder`로 보인다.
  - Song Select: `↑/↓` 곡 이동, `PgUp/PgDn` 페이지 이동, `←/→` 좌측 메뉴 포커스 전환, `Enter` 선택/플레이, `Esc` 타이틀 복귀
    - 좌측 메뉴는 `Songs / Sources / Search / Filter / Records / Options`만 유지한다.
    - `Backspace`는 `Sources` 또는 `Records`에서 `Songs`로 되돌아갈 때만 쓴다.
  - Settings/Mode: `↑/↓` 항목 이동, `←/→` 값 변경, `Enter/Esc` 복귀
  - Keymap: `↑/↓` 선택, `Enter` 바인딩 캡처, `Esc` 복귀
    - 캡처 성공 시 즉시 `keymap.json`에 저장된다.
    - Song Select에서 열면 선택한 차트의 lane count를 우선 기준으로 편집 모드를 잡는다.
  - Result: `Enter`로만 곡 선택 복귀
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
- **SongIndexerThread** scans folders for path/title/artist/BPM/key count/mode/preview audio. Progress updates are posted to the UI; interaction stays responsive.
- **Cache index** (`song_index.json` or SQLite) with mtime/hash checks to avoid full rescans. First run can be slow; subsequent runs should be instant.
- **Preview audio** is scheduled through the audio engine: UI enqueues preview requests, AudioThread mixes them so timing stays aligned.
- Empty-state screens should expose a persistent `Add Songs Folder` action; drag-and-drop stays supported but secondary.
- RawInput이 메뉴에서 비정상적으로 멎으면 `Quick Setup`, `Title`, `Song Select`, `Options`, `Keymap`, `NKRO Test`에서 자동으로 폴링으로 폴백한다.

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
- Keep a per-key state machine (UP/DOWN) so duplicate DOWNs while DOWN and UPs while UP are dropped; collapse down→up→down chatter within the configured debounce window (default `8 ms`).
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
