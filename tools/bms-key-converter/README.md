# BMS Key Converter

Standalone build entrypoint for TenRiff's BMS NK-to-NK converter.

Development-only: TenRiff 1.4.5 official builds and Windows archives do not
build or ship this CLI/GUI. The top-level CMake option defaults to
`TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER=OFF`.

This tool builds only the BMS parser/timeline, BMS gameplay-note builder, selectable Krrcream/nK2/NK3 key-mode converters, and CLI/Win32 GUI frontends. It intentionally avoids the TenRiff menu, renderer, input thread, WASAPI audio runtime, song indexer, profiles, and gameplay session.

## Build

From the TenRiff source root:

```powershell
cmake -S tools/bms-key-converter -B build-bms-key-converter -G "Visual Studio 17 2022" -A x64 -DOpenVINO_DIR="C:/path/to/openvino/cmake"
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
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_10k_nk3.bms" --target-keys 10 --algorithm nk3
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_dt4.bms" --preset dt4
.\build-bms-key-converter\Release\bms_key_converter.exe --input "chart.bms" --output "chart_a9k.bms" --preset a9k --seed 1234
```

The `10k` preset follows the krrcream NtoN 10K preset shape: target keys `10`, max keys `10`, min keys `1`, transform speed slot `5` (`2 bars`), and fixed seed `0`.

Sample rate defaults to `auto`. In auto mode the converter probes referenced BMS note keysounds first, then BGM cues, and falls back to `44100 Hz` only when no referenced audio rate can be detected. Use `--sample-rate 48000` only when you need a manual override.

The standalone writer accepts every output key count from `1` through `18`. Existing standardized layouts retain their established BMS channel maps; other counts use an explicit `PLAYMODE nK` header and deterministic visible-lane channel order.

## Conversion Notes

- The converter parses BMS text with the same UTF-8/UTF-16/CP932 fallback path as TenRiff.
- Long notes are emitted through `LNOBJ` output when needed.
- Non-note channels and dictionaries such as `WAV`, `BMP`, `BPM`, and `STOP` are preserved.
- `krrcream` is the default and preserves the adapted N2NC lane transformation core in `src/gameplay/KeyModeConverter.cpp`.
- `nk2` selects the deterministic native 50/50 profile through `src/gameplay/Nk2KeyModeAdapter.cpp` and the self-contained `nk2/` module.
- `nk3` selects P64 evaluation, a fixed-target generalized pattern MLP for 2K through 18K, and the authoritative host beam safety solver. P64 defaults to strict OpenVINO GPU and accepts `TENRIFF_NK3_DEVICE=CPU`; the MLP verifies and tries NPU, GPU, then CPU. A 1K target uses P64 alone.
- nK2 and NK3 ignore the Krrcream-only Max Keys, Min Keys, Speed Slot, and Seed controls; the GUI disables those fields.
- The build has no dependency on another key-converter source tree. NK3 requires the bundled model and OpenVINO runtime.
