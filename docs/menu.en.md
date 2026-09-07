# Menu structure and controls

Current behavior below targets client 1.7.1. The collapsed early blueprint is not a list of implemented features.

## Current screens and controls

- Home: `Play / Multiplayer / Options / Exit`; the first action becomes `Add Songs Folder` when no charts are indexed.
- Song Select: top tabs `Songs / Sources / Records / Session Mix / Options`, with `Search / Sort·Filter / Difficulty Table` below the center panel. The old left-side KEY/menu rail is no longer the native layout.
- Difficulty-table card: the name/URL area opens an editor; `File` selects local JSON; `Reset` restores native LV. Enter applies, Esc cancels, and invalid addresses preserve the current table. The Filters row shares the same import path.
- Options: ten cards in five columns and two rows: Key Mode, Keymap, Skins, Graphics, Audio, Input, Calibration, Profile Setup, Mods and Key Test.
- Shared settings: arrows select/adjust; Enter activates the selected item; Esc/Backspace goes back. Long help uses page buttons, while Skin Settings keeps a live preview.
- Keymap: supports 4K–10K, 12K, 14K and 16K; successful binding changes save immediately.
- Results: Space skips the single-player reveal. Once ready, R/Left retries, F1 opens replay, and Enter/Esc/Backspace returns. In Session Mix, Enter advances and Esc/Backspace ends the session. Multiplayer results return to the lobby.
- Function keys such as F1/F2/F5 are context-dependent; see the [gameplay guide](gameplay-guide.en.md).

## Ownership and data flow

- [MenuApp](../src/app/MenuApp.cpp) consumes RawInput/polling keyboard events from InputThread and window pointer events. [MenuNavigator](../src/app/menu/MenuNavigator.h) owns the current screen and Back history.
- Typed settings controllers own selection/mutation state; MenuApp applies persistence, file pickers, reindexing and device restarts. [MenuScreenDescriptor](../src/app/menu/MenuScreenDescriptor.h) defines screen metadata and view routing.
- Rendering consumes immutable snapshots. [MenuMusicController](../src/app/MenuMusicController.cpp) owns menu music; [SongSelectScreen](../src/app/SongSelectScreen.h) owns song-preview state.
- [launch_gameplay](../src/app/MenuAppTail.inl) stops menu input and previews before starting GameSession, which owns gameplay audio/input lifecycles. Sharing one continuously open audio device from the menu was an early proposal, not the current contract.
- SongIndexerThread handles indexing with caches at `profiles/<name>/.tenriff/song-index/<source-hash>.json`. See [current state](current-state.en.md) and [configuration](config.en.md).

## Maintenance

Phases 0–6 of the [menu refactor](menu-refactor-plan.md) are complete. Extend existing controllers and explicit screen routes, with focused tests and the [UI checklist](ui-audit-checklist.md). See [menu presentation](menu-visual-polish.md) and [1.7.1 follow-up](gameplay-polish-followup.md) for rendering ownership.

<details>
<summary>Early blueprint — not the current implementation contract</summary>

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
- **SongIndexerThread** scans BMS-family files for path / title / artist / BPM / key count / mode / preview audio. Stage / percent / ETA and a top progress bar remain visible on every non-gameplay screen, including the first folder load.
- **Cache index** (`song_index.json` or SQLite) with mtime / hash checks to avoid full rescans. First run can be slow; subsequent runs should be instant.
- **Preview audio** is decoded off-thread and mixed by AudioThread. Explicit previews are preferred; fragmented BMS charts fall back to a bounded BGM / keysound event mix.
- Empty-state screens should expose a persistent `Add Songs Folder` action; external folders and BMS files also support drag-and-drop.
- In Browse > Difficulty Table, copy an http(s) BMSTable page/header link and press `Enter` to import it into the profile cache. `Right` selects local header JSON and `Left` clears it; changes reapply levels to MD5/SHA-256 matches.
- Song Select `-` / `+` changes and saves `speed.rate` immediately unless search text entry is active.

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

</details>
