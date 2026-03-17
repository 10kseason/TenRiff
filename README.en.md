# TenRiff

Language: [한국어](README.md) | English | [简体中文](README.zh-CN.md)

TenRiff is a Windows GUI BMS-first rhythm-game runtime and launcher project. The goal is to build a standalone rhythm-game client centered on practical BMS play, with direct control over judgement, audio, input, and rendering pipelines. The current project version is `0.9.3`, and the project is distributed under the MIT license. Bundled third-party notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

This README is an onboarding document for first-time readers. For detailed current behavior, the current `0.9.3` project state, the `0.8.0` baseline, configuration structure, and design docs, continue with [docs/README.md](docs/README.md).

## Project Overview

- Primary target platform: Windows
- Primary chart surface: BMS-first
- Optional supported charts: `.osu` osu!mania 4K-10K
- Graphics path: D3D11 + Direct2D/DirectWrite
- Audio path: WASAPI
- Input path: RawInput or high-polling-rate keyboard polling
- License: [MIT](LICENSE)
- Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- Release history: [CHANGELOG.md](CHANGELOG.md)

## Credits / Attribution

TenRiff's current keymode converter includes an adapted port of the N2NC keymode-conversion ideas and code from `krrcream-Toolkit`.

- Original project: <https://github.com/krrcream/krrcream-Toolkit>
- Imported scope: keymode conversion logic based on `Tools/N2NC/N2NC.cs`, adapted to TenRiff's C++ `GameplayChart` runtime model
- Current TenRiff implementation: `src/gameplay/KeyModeConverter.*`
- License / attribution notice: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

The project keeps explicit credit to `krrcream` and the original toolkit wherever this adapted logic is used, while also documenting TenRiff-side integration and modification work separately.

## What Works Right Now

The current codebase is already at the stage where the menu opens, songs can be selected, charts can be loaded and played, and results plus local records can be saved.

- BMS parsing / normalization / timeline processing
  - headers, dictionaries, and measure commands
  - fractional `#MEASURE`
  - compact lane mapping for `#4K / #6K / #8K` headers
  - `#LNOBJ` and LN channels (`51`-`55`, `61`-`65`)
  - legacy BMS text compatibility through CP932 (Shift-JIS) fallback
- BMS audio
  - native WAV decode
  - native OGG Vorbis decode (`stb_vorbis`) first, with Windows Media Foundation fallback when needed
  - MP3 via Windows Media Foundation fallback
  - optional `ffmpeg.exe` fallback
  - keysound modes: `follow / autoplay / ignore`
- Song Select
  - cache-first loading
  - forced reindex with `F5`
  - search, key-count filtering, and difficulty filtering
  - `LV ASC/DESC`, `TITLE A-Z/Z-A` sorting
  - external folder / BMS drag-and-drop
  - saved recent sources
  - `BMS / OSU / All` filtering
- Gameplay / HUD
  - real-time HUD
  - staged chart loading progress
  - `Esc` cancel during gameplay loading
  - display offset
  - performance overlay
  - note head/tail bitmap cache and static playfield command-list cache
- Options / skins
  - Hi-Speed, Rate, gauge, audio, input, and graphics settings
  - `Skins` screen for judgement-line position and note width/height
  - `5K~10K` lane-color editing with live preview
- Results / local records
  - dedicated result screen
  - replay/result JSON export
  - accumulated local chart history
  - best-record selection that prioritizes clear state

## Current Limitations

The project is usable, but it is not a fully finished product yet.

- Windows GUI is the main supported path.
- Linux GUI/audio/input backends are not finished yet.
- Some GUI flows are validated mostly through builds/tests and still need more manual runtime verification.
- Older design docs may not fully match the current implementation, so [docs/current-state.md](docs/current-state.md) should be treated as the first reference for actual current behavior.

## Quick Start

### 1. Repository Layout

These directories are the main ones to look at:

- `src/`: runtime and game code
- `tests/`: unit and smoke tests
- `docs/`: current-state and design documents
- `config/`: default global configuration
- `profiles/`: runtime profile config, keymaps, and local results
- `songs/`: chart root

### 2. Release Build

Typical Windows build commands:

```powershell
cmake -S . -B build-dist -G "Visual Studio 17 2022" -A x64
cmake --build build-dist --config Release --target tenriff
cmake --build build-dist --config Release --target bms_parser_tests
```

### 3. Run Tests

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 4. Launch

Direct launch:

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

Using the launcher script:

```powershell
.\launch_win.bat
```

## Configuration and Runtime Data

TenRiff separates global config from per-profile config.

- Global config: `config/config.json`
- Profile config: `profiles/<name>/config.json`
- Keymap: `profiles/<name>/keymap.json`
- Song-index cache: `profiles/<name>/.tenriff/song-index/<source-hash>.json`
- Replay export: `profiles/<name>/replays/*.json`
- Result export: `profiles/<name>/results/*.json`
- Runtime log: `logs/run.log`
- Crash logs: `logs/crash-*.log`

For the real config structure, [docs/config.md](docs/config.md) is the fastest place to start.

## Recommended Reading Order

This README is only the entry point. For details, read the docs in this order:

1. [docs/README.md](docs/README.md)
   - docs map
2. [docs/current-state.md](docs/current-state.md)
   - what actually works right now
3. [docs/baseline-0.8.0.md](docs/baseline-0.8.0.md)
   - the `0.8.0` baseline document future work is expected to preserve
4. [docs/gameplay-guide.md](docs/gameplay-guide.md)
   - how to start, basic controls, HUD, judgement, and results
5. [docs/config.md](docs/config.md)
   - config, profile, and keymap structure
6. [docs/menu.md](docs/menu.md)
   - menu, state-machine, and song-selection flow
7. [docs/core-loop.md](docs/core-loop.md)
   - play-loop and data-flow details
8. [docs/roadmap.md](docs/roadmap.md)
   - medium- and long-term direction

## How To Interpret The Docs

Design docs and current code may disagree. When that happens, use this order of precedence:

1. current code
2. [docs/current-state.md](docs/current-state.md)
3. [docs/config.md](docs/config.md)
4. older design docs

In short, for current behavior, prioritize the current-state docs over older design plans.

## Read Next

- For actual gameplay usage: [docs/gameplay-guide.md](docs/gameplay-guide.md)
- For configuration details: [docs/config.md](docs/config.md)
- For menu flow: [docs/menu.md](docs/menu.md)
- For play-loop details: [docs/core-loop.md](docs/core-loop.md)
- For a fast status snapshot: [docs/current-state.md](docs/current-state.md)
