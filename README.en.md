# TenRiff

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

TenRiff is a Windows GUI BMS rhythm-game runtime/launcher. The current stable version is `1.2.7`, and chart input is limited to the BMS family (`.bms/.bme/.bml/.pms`). Graphics Settings lets users select a rights-cleared external ONNX model, then explicitly enable `BGA Upscaler` and accept a high-spec warning. Public packages contain no model and the feature defaults to `off`; there is no performance benchmark gate, and load, contract, or inference failure keeps native scaling. The default accelerator route is a high-performance DirectX GPU, with automatic FP32/FP16 boundary and float-boundary INT8 QDQ metadata detection. Experimental `Prefer NPU` is an opt-in low-power device request; Windows and the driver choose the actual device. The project uses the MIT License, and bundled third-party notices are collected in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

This README is an introduction that explains "what to look at first when you open the project." For current behavior, the `1.2.7` project state, the `1.1.2 final stable` baseline, configuration, and design documents, continue from [`docs/README.en.md`](docs/README.en.md).

TenRiff should also be read as a `vibe coding` work: it was shaped through fast iteration and experimentation rather than only through a traditional long-form design-first process.

## Project At a Glance

- Primary target platform: Windows
- Supported charts: BMS-family only (`.bms/.bme/.bml/.pms`)
- Graphics path: D3D11 + Direct2D/DirectWrite
- Audio path: WASAPI
- Input path: RawInput or high-rate polling
- Direct-IP multiplayer: one host plus one joiner over TCP (default `27300/TCP`; see [usage](docs/multiplayer.md))
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

### Thanks

Thanks to OpenAI Codex, ChatGPT, Claude Code, Gemini, and the guest testers who helped validate the project.

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
  - Late real-time inputs keep their judgement timestamp while only the audible trigger is pinned to the current writable buffer boundary, preventing short keysounds from being skipped completely
- Song Select
  - Cache-first loading
  - `F5` forced reindexing with centered stage / percent / ETA progress
  - Search, key-count filtering, difficulty filtering
  - `LV ASC/DESC`, `TITLE A-Z/Z-A` sorting
  - External folder / BMS drag-and-drop
  - Recent source persistence / reopening
  - `-` / `+` adjusts the next-play Rate immediately
  - Browse can select a local BMS difficulty-table JSON and apply table levels to MD5/SHA-256 matches
- Gameplay / HUD
  - Real-time HUD
  - Staged chart-loading progress
  - `Esc` cancel during gameplay loading
  - Display offset
  - Performance overlay
  - Note head/tail bitmap cache + static playfield command-list cache
  - Ghost Battle defaults to `OFF` for new/missing-key settings while preserving an existing explicit opt-in
- Options / skins
  - Hi-Speed, Rate, gauge, audio, input, and graphics settings
  - Judgement-line position, note size, and lane color editing in the `Skins` screen
  - `5K`-`10K` lane color editing with a live preview
  - Native vector and LR2 playskins only
  - LR2 skin-folder selection or drag-and-drop copies into the active profile and imports note, LN, lane-gap, and destination-size data
- Results / local records
  - Result screen
  - Replay / result JSON export
  - Per-song local record accumulation
  - Best-record selection with clear-status priority

## What Is Still Limited

The project is usable, but it is not yet a fully finished product.

- Windows GUI is the main path.
- Linux GUI/audio/input backends are still incomplete.
- LR2 playskin import ports supported gameplay elements; it does not claim pixel-perfect reproduction of the full LR2 UI.
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

If Windows Defender or another antivirus briefly locks `TenRiff.exe`, rerun the same `cmake --build` command after the lock is released.

### 3. Public source packages can be built directly too

The versioned public source bundles (packages such as `TenRiff-1.2.1-source.zip`) include `external/` except `external/llama.cpp/`, `src/`, `tests/`, `config/`, `docs/`, and `Mainmusic/`, so they can be configured and built directly after extraction.

- The public source bundle does not depend on local build wrappers; use the plain `cmake --build` flow shown above.
- `10k-calc/` is intentionally excluded from the public source bundle, so optional Python-reference checks may print `[skip]` and still be considered normal.
- `external/llama.cpp/` is also intentionally excluded, so any local LLM/tooling checkout must be restored separately.
- `profiles/`, `songs/`, and `logs/` are also excluded from the bundle, but `launch_win.bat` creates the needed folders on first launch.

Inside the extracted source-package folder, a typical flow looks like this:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

### 4. Run the tests

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 5. Run the app

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
3. [`docs/baseline-1.1.2.en.md`](docs/baseline-1.1.2.en.md)
   - The `1.1.2 final stable` baseline document that follow-up work should use as a reference
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
