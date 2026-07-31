# BMS Key Converter

Standalone build entrypoint for TenRiff's BMS NK-to-NK converter.

This tool builds only the BMS parser/timeline, BMS gameplay-note builder, selectable Krrcream/nK2 key-mode converters, and CLI/Win32 GUI frontends. It intentionally avoids the TenRiff menu, renderer, input thread, WASAPI audio runtime, song indexer, profiles, and gameplay session.

## Build

From the TenRiff source root:

```powershell
cmake -S tools/bms-key-converter -B build-bms-key-converter -G "Visual Studio 17 2022" -A x64
cmake --build build-bms-key-converter --config Release --target bms_key_converter
cmake --build build-bms-key-converter --config Release --target bms_key_converter_gui
```

The CLI output is:

```text
build-bms-key-converter/Release/bms_key_converter.exe
```

On Windows, the optional GUI output is:

```text
build-bms-key-converter/Release/bms_key_converter_gui.exe
```

## CLI Usage

```powershell
.\build-bms-key-converter\Release\bms_key_converter.exe `
  --input "song\chart.bms" `
  --output "song\chart_6k.bms" `
  --target-keys 6
```

Useful preset examples:

```powershell
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_10k.bms" --preset 10k
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_8k_nk2.bms" --target-keys 8 --algorithm nk2
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_dt4.bms" --preset dt4
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_a9k.bms" --preset a9k --seed 1234
```

The `10k` preset follows the krrcream NtoN 10K preset shape: target keys `10`, max keys `10`, min keys `1`, transform speed slot `5` (`2 bars`), and fixed seed `0`.

Sample rate defaults to `auto`. In auto mode the converter probes referenced BMS note keysounds first, then BGM cues, and falls back to `44100 Hz` only when no referenced audio rate can be detected. Use `--sample-rate 48000` only when you need a manual override.

Supported output key counts are `4`, `5`, `6`, `8`, `9`, `10`, and `16`. The historical `7k` preset is exposed for compatibility with the original toolkit preset table, but this standalone BMS writer currently rejects direct `7K` output.

## Conversion Notes

- The converter parses BMS text with the same UTF-8/UTF-16/CP932 fallback path as TenRiff.
- Long notes are emitted through `LNOBJ` output when needed.
- Non-note channels and dictionaries such as `WAV`, `BMP`, `BPM`, and `STOP` are preserved.
- `krrcream` is the default and preserves the adapted N2NC lane transformation core in `src/gameplay/KeyModeConverter.cpp`.
- `nk2` selects the deterministic native 50/50 profile through `src/gameplay/Nk2KeyModeAdapter.cpp` and the self-contained `nk2/` module.
- nK2 ignores the Krrcream-only Max Keys, Min Keys, Speed Slot, and Seed controls; the GUI disables those fields while nK2 is selected.
- The nK2 build has no dependency on another key-converter source tree.
