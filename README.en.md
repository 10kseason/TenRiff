# TenRiff

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md)

TenRiff is a Windows GUI-based BMS-first rhythm game runtime/launcher project. The goal is to build a standalone rhythm-game client centered on a practical BMS play environment, with direct control over the judgement, audio, input, and rendering pipeline. The current project version is `0.994`, and the project uses the MIT License. Bundled third-party notices are collected in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

This README is an introduction that explains "what to look at first when you open the project." For the more detailed current behavior, the current `0.994` project state, the `0.9.92` baseline, the config structure, and the design documents, continue reading from [`docs/README.en.md`](docs/README.en.md).

## Project At a Glance

- Primary target platform: Windows
- Default chart surface: BMS-first
- Optional supported charts: `.osu` osu!mania 4K-10K
- Graphics path: D3D11 + Direct2D/DirectWrite
- Audio path: WASAPI
- Input path: RawInput or high-rate polling
- License: [MIT](LICENSE)
- Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- Release changelog: [CHANGELOG.md](CHANGELOG.md)

## Credits / Attribution

TenRiff's current key-mode converter implementation includes an adaptation/port based on the N2NC idea and code from `krrcream-Toolkit`.

- Original project: <https://github.com/krrcream/krrcream-Toolkit>
- Applied scope: porting the key-mode conversion logic from `Tools/N2NC/N2NC.cs` into TenRiff's C++ `GameplayChart` structure
- Current TenRiff implementation location: `src/gameplay/KeyModeConverter.*`
- License / source notice: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)

Whenever possible, the original author `krrcream` and the upstream toolkit are kept credited, and TenRiff-side changes/integration details are described separately.

## What You Can Do Now

The codebase is currently at the level where you can "open the menu, choose a song, load a chart, play it, and review the result and local records."

- BMS parser / normalizer / timeline handling
  - Headers, dictionaries, and measure commands
  - `#MEASURE` fraction handling
  - Compact lane mapping by declared key count when `#4K / #6K / #8K` headers are present
  - `#LNOBJ`, LN channels (`51`-`55`, `61`-`65`)
  - CP932 (Shift-JIS) legacy BMS text support
- BMS audio handling
  - Native WAV decoding
  - Native OGG Vorbis decoding via `stb_vorbis`, with a Windows Media Foundation fallback if needed
  - MP3 via Windows Media Foundation fallback
  - `ffmpeg.exe` fallback when needed
  - `follow / autoplay / ignore` keysound mode
- Song Select
  - Cache-first loading
  - `F5` forced reindexing
  - Search, key-count filtering, difficulty filtering
  - `LV ASC/DESC`, `TITLE A-Z/Z-A` sorting
  - External folder / BMS drag-and-drop
  - Recent source persistence / reopening
  - `BMS / OSU / All` filtering
- Gameplay / HUD
  - Real-time HUD
  - Staged chart-loading progress
  - `Esc` cancel during gameplay loading
  - Display offset
  - Performance overlay
  - Note head/tail bitmap cache + static playfield command-list cache
- Options / skins
  - Hi-Speed, Rate, gauge, audio, input, and graphics settings
  - Judgement-line position, note size, and lane color editing in the `Skins` screen
  - `5K`-`10K` lane color editing with a live preview
- Results / local records
  - Result screen
  - Replay / result JSON export
  - Per-song local record accumulation
  - Best-record selection with clear-status priority

## What Is Still Limited

The project is usable, but it is not yet a fully finished product.

- Windows GUI is the main path.
- Linux GUI/audio/input backends are still incomplete.
- Some GUI paths are validated primarily through build/tests, and manual in-game verification still remains.
- Older design documents and the current implementation may differ in places, so the current-state document should always be consulted first.

## Quick Start

### 1. Prepare the repository layout

These directories are the usual entry points:

- `src/`: runtime/game code
- `tests/`: unit and smoke tests
- `docs/`: current-state and design documents
- `config/`: global default config
- `profiles/`: runtime profiles, keymaps, local results
- `songs/`: chart root

### 2. Release build

The following is a typical Windows build example:

```powershell
cmake -S . -B build-dist -G "Visual Studio 17 2022" -A x64
cmake --build build-dist --config Release --target tenriff
cmake --build build-dist --config Release --target bms_parser_tests
```

If Windows Defender or another antivirus briefly locks `TenRiff.exe`, use the wrapper below:

```powershell
.\tools\build_with_retry.ps1 -BuildDir build-dist -Config Release -Targets tenriff,bms_parser_tests
```

### 3. Run the tests

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 4. Run the app

Direct launch:

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

Launcher script:

```powershell
.\launch_win.bat
```

## Config and Runtime Data

TenRiff separates global config from profile config.

- Global config: `config/config.json`
- Profile config: `profiles/<name>/config.json`
- Keymap: `profiles/<name>/keymap.json`
- Song index cache: `profiles/<name>/.tenriff/song-index/<source-hash>.json`
- Replay export: `profiles/<name>/replays/*.json`
- Result export: `profiles/<name>/results/*.json`
- Runtime log: `logs/run.log`
- Crash log: `logs/crash-*.log`

For a detailed look at the config structure, the fastest starting point is [`docs/config.en.md`](docs/config.en.md).

## Reading Order

This README only covers the introduction. For the details, the most efficient reading order is:

1. [`docs/README.en.md`](docs/README.en.md)
   - Full documentation map
2. [`docs/current-state.en.md`](docs/current-state.en.md)
   - What actually works right now
3. [`docs/baseline-0.9.92.en.md`](docs/baseline-0.9.92.en.md)
   - The `0.9.92` baseline document that follow-up work should use as a reference
4. [`docs/gameplay-guide.en.md`](docs/gameplay-guide.en.md)
   - How to start playing, basic controls, HUD/judgement/result screen explanation from a practical player perspective
5. [`docs/config.en.md`](docs/config.en.md)
   - Config/profile/keymap structure
6. [`docs/menu.en.md`](docs/menu.en.md)
   - Menu / state machine / song-select flow
7. [`docs/core-loop.en.md`](docs/core-loop.en.md)
   - Play loop and data flow
8. [`docs/roadmap.en.md`](docs/roadmap.en.md)
   - Long-term direction for future work

## Document Interpretation Rules

Design documents and the current code may appear to disagree. In that case, the priority order is:

1. Current code
2. [`docs/current-state.en.md`](docs/current-state.en.md)
3. [`docs/config.en.md`](docs/config.en.md)
4. Older design documents

In other words, when judging "current behavior," the current-state document should take precedence over older design notes.

## What To Read Next

- If you want to know how to actually play, read [`docs/gameplay-guide.en.md`](docs/gameplay-guide.en.md)
- If you want to understand the settings, read [`docs/config.en.md`](docs/config.en.md)
- If you want to understand the menu flow, read [`docs/menu.en.md`](docs/menu.en.md)
- If you want to understand the play loop, read [`docs/core-loop.en.md`](docs/core-loop.en.md)
- If you want a quick overview of the overall state, read [`docs/current-state.en.md`](docs/current-state.en.md)
