TenRiff source package notes (`1.3.1`)

- This folder is a curated source-only staging area for public/open-source distribution.
- It intentionally excludes local build trees, packaged binaries, caches, user profiles, logs, and private working notes.
- Internal agent workflow files such as `AGENTS.md` are not part of the public source bundle.
- The included `SOURCE_PACKAGE_SCOPE.txt` file defines the exact include/exclude rules used for the staged bundle.
- The current source line is `1.3.1`; it carries the UI-r2 Song Select and animated Result flows, adds direct left/right quick-setting controls and record/layout fixes, and keeps optional local profile avatars, the TenRiff skin format, and ranked-clear exclusion for Autoplay.
- It supports BMS-family charts, 4K through 14K key modes, and native/LR2/TenRiff skins, keeps MPG/MPEG video BGA decoding with an FFmpeg fallback, and exposes an External ONNX Upscaler that remains off until the user enables it and acknowledges the high-spec warning.
- The repository license is MIT. Keep the top-level `LICENSE` file with any redistributed source bundle.
- The source bundle includes the code/docs/dependencies needed for a standalone Windows configure/build, but it does not ship the local `10k-calc/` reference checkout or `external/llama.cpp/`.
- The generic integration and compatibility smoke/quantization tools live under `tools/onnx_upscaler/`, but no ONNX model, checkpoint, training data, or model-specific verification metadata is distributed. Users must supply a rights-cleared model matching the documented 960x540 RGB residual x2 contract. FP32/FP16 boundaries and float-boundary INT8 QDQ metadata are detected automatically. Model selection only stores a path; there is no automatic benchmark gate. The default route requests a high-performance DirectX GPU. The experimental low-power option requests WinML `DirectXMinPower`; it is not an explicit or verified NPU selection and TenRiff falls back to the existing DirectX routes when needed.
- The staged docs/readmes now track Korean, English, Simplified Chinese, and Japanese entrypoints.
- Typical Windows build flow inside the extracted source-package root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

- Optional Python-reference checks can print `[skip]` when `10k-calc/` is absent; that is expected for the public source package.
- `launch_win.bat` can create missing `profiles/`, `songs/`, and `logs/` folders on first launch.
- When refreshing this public source bundle for a release, verify at least the standalone configure/build/test path above from the staged source-package root.
