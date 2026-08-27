TenRiff source package notes (`1.5.1`)

- This folder is a curated source-only staging area for public/open-source distribution.
- It intentionally excludes local build trees, packaged binaries, caches, user profiles, logs, and private working notes.
- Internal agent workflow files such as `AGENTS.md` are not part of the public source bundle.
- The included `SOURCE_PACKAGE_SCOPE.txt` file defines the exact include/exclude rules used for the staged bundle.
- The current source line is `1.5.1`; it keeps P64 and the host beam safety solver authoritative and enables the generalized NK3 pattern MLP only for non-10K sources converted to 10K.
- It supports BMS-family charts, 4K through 14K key modes, and native/LR2/TenRiff skins, keeps MPG/MPEG video BGA decoding with an FFmpeg fallback, and exposes an External ONNX Upscaler that remains off until the user enables it and acknowledges the high-spec warning.
- The repository license is MIT. Keep the top-level `LICENSE` file with any redistributed source bundle.
- The source bundle includes the code/docs/dependencies needed for a standalone Windows configure/build, but it does not ship the local `10k-calc/` reference checkout or `external/llama.cpp/`.
- The generic integration and compatibility smoke/quantization tools live under `tools/onnx_upscaler/`, but no BGA upscaler model, checkpoint, training data, or model-specific verification metadata is distributed. Bundled ONNX files are limited to the deterministic NK3 P64 graph and the 2K-through-18K generalized pattern MLP inference exports. BGA upscaler users must still supply a rights-cleared model matching the documented 960x540 RGB residual x2 contract.
- NK3 defaults to the bundled ncnn 20260526 shared runtime and converted models. Windows release packages run P64 and the optional MLP through Vulkan on AMD/NVIDIA GPUs and ship `ncnn.dll` plus its BSD license. `TENRIFF_NK3_BACKEND=AUTO|VULKAN|OPENVINO` controls routing, `TENRIFF_NK3_VULKAN_DEVICE=<index>` selects among Vulkan devices, and a source-provided OpenVINO package remains an optional compatibility fallback.
- Official 1.5.1 builds and Windows archives do not build or ship the standalone BMS key-converter CLI/GUI. Its source remains development-only and the top-level CMake option defaults to `TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER=OFF`.
- The staged docs/readmes now track Korean, English, Simplified Chinese, and Japanese entrypoints.
- Typical Windows build flow inside the extracted source-package root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

- AddressSanitizer uses the same local and CI contract: `cmake --preset asan`, `cmake --build --preset asan`, then `ctest --preset asan`. The deterministic unit core runs in one process so tests cannot collide on shared ports. Eight exact Windows RawInput/localhost integration cases remain in the normal Release suite because their worker shutdown is incompatible with MSVC ASan.

- Optional Python-reference checks can print `[skip]` when `10k-calc/` is absent; that is expected for the public source package.
- `launch_win.bat` can create missing `profiles/`, `songs/`, and `logs/` folders on first launch.
- When refreshing this public source bundle for a release, verify at least the standalone configure/build/test path above from the staged source-package root.
