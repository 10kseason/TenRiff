# Main Menu Low-Latency Blueprint

The main menu must follow the same low-latency philosophy as gameplay: audio runs as the master clock, inputs are timestamped off-thread, and rendering only consumes snapshots. This blueprint captures the rules and implementation order so menu work does not reintroduce input lag.

## Current Implementation State (Windows Menu UI)
- `MenuApp` runs as **InputThread (polling)** -> **SPSC queue** -> **menu state machine** -> **RenderThread (D3D11 window render)**.
- `SongIndexerThread` builds the song index in the background and caches it at `profiles/<name>/.tenriff/song-index/<source-hash>.json`.
- When audio / graphics / input / mode settings are changed in the menu, they are saved to the profile config file.
- `Options -> Profile Setup` reopens first-run setup for the active profile and immediately saves language, audio, input, graphics, and keymap changes.
- When play starts, the current implementation stops the menu thread and runs `GameSession` separately.
- The **Windows menu UI is built on D3D11 + Direct2D / DirectWrite** and renders Title / Song Select (cyan layout) and other settings screens (list UI).
- Input summary:
  - Title: `↑ / ↓` move, `Enter` select (PLAY / EDIT / OPTIONS / EXIT), `Esc` quit
  - Song Select: `↑ / ↓` song movement, `← / →` switch focus on the left menu, `Enter` select / play, `Esc` back
  - Settings / Mode: `↑ / ↓` move items, `← / →` change values, `Enter / Esc` return
  - Keymap: `↑ / ↓` select, `Enter` capture binding, `Esc` return
  - Result: return to Song Select with `Enter` only
  - Shared utility keys: `F1` help, `F2` songs-folder browse, `F5` refresh / reindex, `F9` screenshot

## Non-Negotiable Rules
- **Keep the audio device open from the menu.** Initialize the audio backend when entering the menu and run silent callbacks (zero buffers) so `playhead_samples` / `buffer_start_samples` remain valid before gameplay begins. Avoid reopening the device when starting a song to prevent warm-up jitter.
- **Menu input uses InputThread + SPSC only.** Consume UI actions from the same RawInput / evdev ingestion path. Never let the render / UI event loop timestamp inputs directly.
- **The audio thread stays allocation / I/O / lock free.** Do not introduce file I/O, heap allocations, or locks in audio callbacks for menu previews.
- **Render is read-only.** It consumes snapshots and never mutates authoritative timing or timestamps inputs.
- **Heavy work is offloaded.** Folder scans, metadata parsing, and replay / result saves run on background jobs so the UI thread never blocks.

## State Machine Skeleton
States render UI and consume already-timestamped input events; heavyweight work is delegated to jobs.
- `TitleState`
- `SongSelectState`
- `GameplayState` (chart playback)
- `ResultState`
- Later: `SettingsState`, `KeymapState`, `LatencyToolsState`

### Flow
`Title -> SongSelect -> Gameplay -> Result` is the minimal playable loop. Each transition should reuse the live audio clock and keep InputThread running.

## Song Select Without Hitching
- **SongIndexerThread** scans folders for path / title / artist / BPM / key count / mode / preview audio. Progress updates are posted to the UI; interaction stays responsive.
- **Cache index** (`song_index.json` or SQLite) with mtime / hash checks to avoid full rescans. First run can be slow; subsequent runs should be instant.
- **Preview audio** is scheduled through the audio engine: the UI enqueues preview requests, and AudioThread mixes them so timing stays aligned.
- Empty-state screens should expose a persistent `Add Songs Folder` action; drag-and-drop stays supported but secondary.

## Settings: Latency-First Surface
Put these on the first page so users see latency-critical toggles immediately:
- Audio backend (wasapi / asio, alsa / jack)
- Sample rate (48 kHz recommended)
- Buffer size (128 / 192 / 256) with optional adaptive step-up
- RawInput / evdev grab toggle (warn when off)
- VSYNC off / driver frame-queue guidance
- `input_offset_ms` and separate `visual_offset_ms`
- HUD toggles (latency overlay / xrun / late counter)

## Key Remap and NKRO Test
- Capture the **next input event** from InputThread to bind keys; never block by polling the render loop.
- Keep a per-key state machine (UP / DOWN) so duplicate DOWNs while DOWN and UPs while UP are dropped; preserve real down -> up -> down transitions so fast taps and releases do not get swallowed.
- Successful key captures should save immediately; there is no separate hidden save chord.
- The NKRO test remains a visible tool screen, but not a hidden keyboard shortcut.
- The NKRO test shows the current pressed set and highlights ghosting / missing keys in real time using the same input events.

## Transition Into Gameplay Without Lag Spikes
1) **Preload stage (in menu):** load / normalize the chart into sample positions; pre-decode / preload keysounds.
2) **Warm start (on entry):** with audio already running, schedule `song_start_samples` a few buffers in the future relative to `buffer_start_samples`.
3) **Start:** at that sample time, the render / judgement / keysound paths attach so the first note feels locked in.

## Result Screen Hygiene
- Show results immediately; run replay / log saves as background jobs with a "Saving…" indicator.
- Replays store `{lane, state, sample}` so latency bugs can be reproduced deterministically.

## Recommended Implementation Order
1) Pick a UI framework (for example SDL + ImGui, or custom) while ensuring input / timing remain under the existing pipeline.
2) Implement the state machine and the four screens (`Title / SongSelect / Gameplay / Result`) so navigation works end-to-end.
3) Add SongIndexerThread + cached index + responsive SongSelect UI.
4) Surface latency-first settings and apply them live where possible; note when backend changes require a restart.
5) Add key remap + NKRO test following the input pipeline rules.
