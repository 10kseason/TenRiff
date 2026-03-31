# TenRiff Current State

This is the document that the next agent or any new contributor should read first. Its goal is to quickly answer: "what is this project now, where should I look, and what is still unverified?"

## Baseline
- Current project version: `1.1.2`
- The `1.1.2` line is named the public `final stable` version
- Baseline companion document for follow-up work: `docs/baseline-1.1.2.en.md`
- Windows GUI build is the main target
- Linux exists only as a preview-level package at `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview`
- Default surface is BMS-first
- `.osu` can be re-enabled as an option and supports 4K-10K
- The `1.1.2` release line keeps the `1.0.9` gameplay playback-head timing fix, but pins live gameplay capture to `Polling` for stability while keeping the gameplay session on an always-allow input gate regardless of foreground state
- On the same `1.1.2` line, menu input still uses the foreground process/root-window boundary, while restart-style backend fallback and persisted input-backend rewrites remain removed and the saved backend preference is preserved as configuration

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
  - recent source persistence and reopening
  - BMS / OSU / All filtering
  - difficulty / title sorting
- osu!mania:
  - 4K-10K load / launch support
  - separate keymaps per key mode
  - 4K-10K chart difficulty calculation
  - `mode.key_mode` uses an N2NC-style lane remap to convert key counts
  - `mode.key_mode=none` keeps the chart's original key count and base pattern layout intact
- Skins / gameplay feel:
  - `rect` / `circle` note shape
  - note border on/off
  - combo Y adjustment
  - judge line / lane width / lane spacing / note width / divider width / 16K center gap / note height / LN body width adjustment
  - per-key-mode lane-width arrays and inter-lane spacing arrays are persisted and applied through the same layout math in preview, live gameplay, and the ghost field
  - osu!mania `ColumnLineWidth` is read and applied to lane divider width
  - `skin.lr2_resolution_mode` stores LR2 playskin resolution override tokens as `auto / sd / hd / fhd`
  - LR2 auto-detect uses the playskin `#DST_NOTE` coordinate range instead of asset names
  - eased future-note entry from above the field
  - gameplay ends right after the last judged note is handled
- Judge:
  - default `GOOD` window is `75ms`
  - default `BAD` window is `340ms`
  - note-consuming failures (auto-miss, too-early consume, hold break / tail miss) stay `BAD`
  - very early non-consuming presses are handled as LR2-style `POOR` and are visible again in result / replay / UI paths
  - `POOR` preserves combo, stays out of score / accuracy totals, and uses dedicated `PR` gauge damage values
  - live gameplay input uses the `ClockSync` estimate directly, and stale backlog compression now follows the `BAD` window again to match the `0.999` boundary
  - tail release timing applies only to osu hold and BMS `#LNMODE 2` charge notes
  - when two keyboards press the same key, the logical `Pressed` state remains active until the last input source releases it
- Graphics:
  - resolution presets (`720p`, `1080p`, `qhd`, `native`)
  - `refresh_hz` (`60..1050`, default `300`)
  - VSync off: menu effective cap `300`, gameplay can use the configured target up to `1050`
  - VSync on: present refresh follows the active monitor Hz and render pacing targets `monitor_hz * 2` (`1050` clamp)
  - `visual_offset_ms`
  - `performance_overlay`
- Gameplay performance:
  - static playfield command-list cache
  - note head / tail bitmap cache
  - fixed-size HUD note transport
- Loading UX:
  - Song Select indexing progress display
  - gameplay chart-loading progress display
  - `Esc` cancel during gameplay loading

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
  - on `D:\Stellaverse (2025-12-14)` safe full-index, `46,636` candidates / `46,602` indexed entries
  - peak memory roughly `working set 453MB`, `private 524MB`
  - on a 1024-chart sample from the same library, fast-profile throughput is about `2.05x` vs safe
- Cache schema:
  - `version = 8`
  - includes `include_osu`
  - optional `layout_label`

## Runtime / Packaging Rules
- New user profiles are created automatically
- The last staged distribution package is `Baepoks/TenRiff-1.1.2`
- Distribution packages do not include `Songs`
- Distribution packages include the runtime `Mainmusic/` assets used for menu BGM
- When updating distribution builds, only built artifacts should be copied into `Baepoks/`
- If a source-only / public handoff is requested, the user's preference is to write an include/exclude list first
- The last staged public source package is versioned separately, e.g. `opensource-Tenriff-source/TenRiff-1.1.2-source`
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
- `cmake -S opensource-Tenriff-source/TenRiff-1.1.2-source -B opensource-Tenriff-source/TenRiff-1.1.2-source/build-check -G "Visual Studio 17 2022" -A x64`
- `cmake --build opensource-Tenriff-source/TenRiff-1.1.2-source/build-check --config Release --target bms_parser_tests`
- `opensource-Tenriff-source/TenRiff-1.1.2-source/build-check/Release/bms_parser_tests.exe`

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
