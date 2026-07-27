# TenRiff Current State

This is the document that the next agent or any new contributor should read first. Its goal is to quickly answer: "what is this project now, where should I look, and what is still unverified?"

## Baseline
- Current project version: `1.2.1 stable`
- Direct-IP multiplayer and the preview r5 input-backend lifecycle fixes are integrated into `1.1.8 stable`
- `1.1.8` adds an osu!mania OD8 auxiliary score, first-native-`BAD` `Sudden Death (1 MISS)`, and deterministic `LN Mix 10%-90%` on top of the 1.1.7 visual refresh
- `1.2.0` connects BMS channel `04/07` and osu!mania backgrounds to the gameplay sample timeline and asynchronously upscales sub-FHD image backgrounds through LunaSR on Windows ML
- `1.2.1` switches the runtime to LunaSR `basic_v2`, applies it to gameplay BGA layers and the selected Song Select BGI, adds the single-player Esc pause menu, and adds polygon note shapes plus an optional LN tail cap.
- Baseline companion document for follow-up work: `docs/baseline-1.1.2.en.md`
- Windows GUI build is the main target
- Linux exists only as a preview-level package at `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview`
- Default surface is BMS-first
- `.osu` can be re-enabled as an option and supports 4K-10K
- The `1.2.1 stable` runtime keeps RawInput primary while continuously running a bound-key polling shadow in the same `InputThread`; startup failure or an unexpected message-pump exit switches that producer to Polling without resetting its queue or pressed state
- Menu input keeps the foreground process/root-window boundary. A RawInput startup failure, process-global registration-target loss, or hidden message-window exit switches it to Polling without waiting for a user key.
- A confirmed fallback stays active across menu and subsequent gameplay sessions for the current app run without rewriting the profile; app restart or an explicit `Options -> Input Settings -> Backend` change retries it.

## Core Architecture
- `MenuApp`
  - Center of the menu state machine
  - Manages entry into Song Select, Options, Keymap, Result, and Gameplay launch
  - Recent maintenance refactors split Song Select record/keymap/render/state boundaries into dedicated `.cpp` files
  - Optional Python-reference checks can now skip cleanly so the open-source source package can run the core test suite without a local `10k-calc` checkout
- `SongIndexerThread`
  - Background thread dedicated to chart indexing
  - Sends progress to Song Select
- `AudioThread`
  - Handles the audio master clock and mixing
- `InputThread`
  - Collects RawInput / polling input and passes it into the queue
  - Gameplay deduplicates RawInput and the bound-key polling shadow in one `InputThread` state tracker; `GameSession` does not filter the logical edges again by source
- `RenderThread` + `MenuWindow`
  - Menu and in-game HUD rendering built on D3D11 + Direct2D / DirectWrite
  - The recent maintenance refactor is organizing large implementation files into smaller fragments
- `GameSession`
  - Chart loading, gameplay audio prep, HUD snapshot, and gameplay execution boundaries

## What Works Now
- The BMS parser / normalizer / timeline pipeline has been hardened for real-world pack compatibility
- BMS explicit key headers:
  - `#4K`
  - `#6K`
  - `#8K`
  - `5+1 SP`
  - `7+1 SP`
  - if a header is present or an SP pattern is detected, compact lane mapping is applied for that key count
- BMS keysound:
  - `follow`
  - `autoplay`
  - `ignore`
- BMS long notes:
  - LN channels (`51`-`55`, `61`-`65`)
  - `#LNOBJ`
  - `#LNMODE 2` charge notes use tail release timing judgement
  - normal BMS LN tails auto-clear when held to the end
- BMS audio decode:
  - WAV native first
  - Windows Media Foundation fallback for OGG / MP3
  - `ffmpeg.exe` fallback if Media Foundation fails
- Song Select:
  - cache-first loading
  - `F5` forced reindexing
  - mouse-wheel navigation
  - left-side `KEY` quick filter toggle
  - external folder / BMS drag-and-drop
  - `.osz` files install into the active songs source through `Shift+F2` or drag-and-drop, enable osu charts, and reindex that source
  - OSZ installation preflights the complete archive, extracts through staging, commits atomically, never overwrites an existing folder, and contains `.osu` background/audio/hitsound references to the chart directory
  - recent source persistence and reopening
  - BMS / OSU / All filtering
  - difficulty / title sorting
- osu!mania:
  - 4K-10K load / launch support
  - separate keymaps per key mode
  - 4K-10K chart difficulty calculation
  - `mode.key_mode` uses an N2NC-style lane remap to convert key counts
  - `mode.key_mode=none` keeps the chart's original key count and base pattern layout intact
- Native difficulty:
  - BMS/osu!mania LV/CR calculation evaluates only LN head/tail miss-ms at 0.5x, so `300ms` is treated as `150ms`; runtime gameplay judgement windows remain unchanged
- Lane transform:
  - Random supports `Off / Mirror / FR / SR`; Mirror reverses the final lanes after key-mode conversion, with 10K/16K mirrored independently inside each player half
  - Mod Manager `LN Mix 10%-90%` preserves existing holds, excludes heads overlapping an existing same-lane span, and uses `Random Seed` to deterministically convert the requested share of taps that can form a hold of at least 50ms while leaving 50ms before the next same-lane note
- Skins / gameplay feel:
  - `rect` / `circle` note shape
  - note border on/off
  - combo Y adjustment
  - judge line / lane width / lane spacing / note width / divider width / 16K center gap / note height / LN body width adjustment
  - per-key-mode lane-width arrays and inter-lane spacing arrays are persisted and applied through the same layout math in preview, live gameplay, and the ghost field
  - `.osk` files install into the active profile's `skins` directory through the Skins file picker or drag-and-drop, using the same transactional, no-overwrite archive policy as OSZ
  - supported osu!mania note/LN images and `ColumnWidth`, `ColumnSpacing`, `ColumnLineWidth`, and `HitPosition` are applied to the gameplay layout
  - every valid archive file is preserved, but TenRiff does not claim pixel-perfect rendering of unsupported osu! modes or UI assets
  - `skin.lr2_resolution_mode` stores LR2 playskin resolution override tokens as `auto / sd / hd / fhd`
  - LR2 auto-detect uses the playskin `#DST_NOTE` coordinate range instead of asset names
  - eased future-note entry from above the field
  - gameplay ends right after the last judged note is handled
- Judge:
  - default `GOOD` window is `75ms`
  - default `BAD` window is `340ms`
  - if the pending same-lane note is already a `BAD` while the immediate next note is clearly `GOOD` or better, the pending note is recorded as a miss and the current press scores the next note instead of locking the stream into repeated `BAD`s
  - note-consuming failures (auto-miss, too-early consume, hold break / tail miss) stay `BAD`
  - very early non-consuming presses are handled as LR2-style `POOR` and are visible again in result / replay / UI paths
  - `POOR` preserves combo, stays out of score / accuracy totals, and uses dedicated `PR` gauge damage values
  - gauge modes support `EX-Hard / Hard / Normal / Easy`; all start at `100%` and fail immediately at `0%`
  - `Sudden Death (1 MISS)` fails immediately on the first osu!mania OD8 object `MISS`; native `BAD` timing alone and empty-key `POOR` are ignored, and the option is mutually exclusive with Practice No-Fail
  - Gameplay and Result show an auxiliary `OSU OD8` score converted from real input timing with osu!mania stable OD8 windows and ScoreV1 (maximum 1,000,000); native TenRiff score and ranking stay unchanged
  - live gameplay `ClockSync` uses centered anchor regression instead of large absolute Windows QPC values and automatically rebases after sustained clock discontinuities
  - stale backlog is classified from QPC event age and the `BAD` window; a fresh input whose sample mapping drifts far from the current playback anchor falls back to that anchor instead of becoming permanently non-scoring catch-up
  - tail release timing applies only to osu hold and BMS `#LNMODE 2` charge notes
  - when two keyboards press the same key, the logical `Pressed` state remains active until the last input source releases it
- Graphics:
  - resolution presets (`720p`, `1080p`, `qhd`, `native`)
  - `refresh_hz` (`60..1050`, default `300`)
  - VSync off: menu effective cap `300`, gameplay can use the configured target up to `1050`
  - VSync on: present refresh follows the active monitor Hz and render pacing targets `monitor_hz * 2` (`1050` clamp)
  - `visual_offset_ms`
  - `performance_overlay`
  - `background_upscale_mode=lunasr|off`: asynchronously produces an FHD background on a Windows ML worker and keeps native scaling active until completion or after failure
- Gameplay performance:
  - static playfield command-list cache
  - note head / tail bitmap cache
  - fixed-size HUD note transport
- Loading UX:
  - Song Select indexing progress display
  - gameplay chart-loading progress display
  - `Esc` cancel during gameplay loading
- Profile UX:
  - `Options -> Profile Setup` reopens the first-run setup surface for the active profile and saves language, audio, input, graphics, and keymap changes immediately
- Direct-IP multiplayer:
  - A joiner matches the host chart by exact hash and size across the active source and existing profile-local caches for `recent_song_sources`
  - It never scans the whole disk or starts a rescan, and cached paths outside their source root are rejected
  - The multiplayer-only gauge is a one-way shift: `Normal` at or below `33%` changes once to `Easy 100%`, never shifts back, and reaching `Easy 0%` gives that player GAME OVER
  - The live score-gap bar is local-player-relative: `-10,000 / 0 / +10,000` map to the `LOSS` endpoint, center, and `WIN` endpoint; reaching an endpoint is display-only and never ends the match
  - If one player reaches GAME OVER first, the other player continues; the defeated player waits on an aggregate spectator surface showing peer score, combo, gauge, and status until both results are ready
  - The peer protocol does not transport exact lane input, per-note judgement, or hold state, so it does not render a guessed remote note field

## Song Indexing Model
- When the song source changes, the profile-local cache at `profiles/<name>/.tenriff/song-index/<source-hash>.json` is read first
- If the cache is missing or invalid, background indexing starts
- Indexing profiles:
  - `safe` as the default
  - `fast` as the optional choice
  - controlled by the Mode Settings `Indexing` row and `config.mode.song_index_profile`
- Indexing stages:
  - `SCANNING FILES`
  - `BUILDING METADATA`
  - `WRITING CACHE`
- Memory hardening for large libraries:
  - two-pass enumerate + small batch metadata build
  - the `safe` profile keeps RAM high-water under control with a one-worker-oriented budget and frequent heap trimming on large scans
  - BMS parsing for indexing uses a lower-memory path that skips asset maps, unnecessary headers, and non-essential commands
  - cache save uses streaming writes instead of a giant JSON tree
- Measurement:
  - on a 46k-chart Windows benchmark library safe full-index, `46,636` candidates / `46,602` indexed entries
  - peak memory roughly `working set 453MB`, `private 524MB`
  - on a 1024-chart sample from the same library, fast-profile throughput is about `2.05x` vs safe
- Cache schema:
  - `version = 10`
  - includes `include_osu`
  - optional `layout_label`

## Runtime / Packaging Rules
- New user profiles are created automatically
- The current official P2P distribution line is `TenRiff 1.2.1 stable`
- Distribution packages do not include `Songs`
- Distribution packages include the runtime `Mainmusic/` assets used for menu BGM
- Distribution updates include only built artifacts and required runtime assets
- Confirm the include/exclude list before a source-only or public handoff
- Keep preview source branches and tags versioned separately from stable releases
- When refreshing a public source package, do not stop at syncing docs/files only; also verify that the staged source-package folder itself can configure, build, and run the core test binary standalone

## Config / Profile Reality
- The real default values live in `config/config.json`
- The runtime profile lives in `profiles/<name>/config.json`
- The keymap lives in `profiles/<name>/keymap.json`
- `keymap.json` has a `modes.{4k..10k}` per-mode binding structure
- Stale profiles are partially corrected by runtime migration
  - BMS-first default
  - keysound policy
  - osu key-mode mismatch, etc.

## High-Value Files
- `src/app/MenuApp.cpp`
- `src/app/GameSession.cpp`
- `src/app/SongIndex.cpp`
- `src/app/SongIndexerThread.cpp`
- `src/render/MenuWindow.cpp`
- `src/render/RenderThread.cpp`
- `src/app/ChartLoader.cpp`
- `src/chart/BmsParser.cpp`
- `src/config/Config.*`
- `src/config/Keymap.*`

## Validated Commands
- `cmake --build build --config Release --target tenriff`
- `cmake --build build --config Release --target bms_parser_tests`
- `cmake --build build --config Release --target bms_realworld_smoke`
- `ctest --test-dir build -C Release --output-on-failure -R bms_parser_tests`
- `cmake -S . -B build-check -G "Visual Studio 17 2022" -A x64`
- `cmake --build build-check --config Release --target bms_parser_tests`
- `.\build-check\Release\bms_parser_tests.exe`

## Still Manual-Validation Heavy
- Song Select fast-scroll crash reproduction on a real CJK-heavy library
- long-running full-index RAM / commit recheck for the fast profile
- gameplay low-FPS / 0.1% / 0.01% low verification
- coexistence with OBS / Discord / Game Bar while live-applying graphics settings
- GUI verification for drag-and-drop / external Korean-path sources
- separate keymap verification for 4K-10K `.osu`
- Linux is still not a real runnable build

## Best Next Read
- For runtime / config, read `docs/config.en.md`
- For menu / indexing / the state machine, read `docs/menu.en.md`
- For play loop / audio / judgement, read `docs/core-loop.en.md`
