TenRiff source package notes (`1.2.2 stable`)

- This folder is a curated source-only staging area for public/open-source distribution.
- It intentionally excludes local build trees, packaged binaries, caches, user profiles, logs, and private working notes.
- Internal agent workflow files such as `AGENTS.md` are not part of the public source bundle.
- The included `SOURCE_PACKAGE_SCOPE.txt` file defines the exact include/exclude rules used for the staged bundle.
- The current source line is `1.2.2 stable`, which adds MPG/MPEG video BGA decoding with an FFmpeg fallback and places the staged32 RGB FP16 LunaSR model behind a mandatory 200 FPS safety gate.
- The repository license is MIT. Keep the top-level `LICENSE` file with any redistributed source bundle.
- The source bundle includes the code/docs/dependencies needed for a standalone Windows configure/build, but it does not ship the local `10k-calc/` reference checkout or `external/llama.cpp/`.
- The optional LunaSR source/model package is included under `tools/lunasr/` with its dedicated MIT license and WinML-compatible ONNX artifact.
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
