TenRiff source package notes (`1.9.0`)

- Fixed baseline: [1.9.0](docs/baseline-1.9.0.en.md), explicitly selected on 2026-10-14. The immutable tag identifies the source; its resolved commit and delivered ZIP hashes are recorded in the version-specific release notes and checksum asset. Previous 1.8.7 baseline and published tags/assets are preserved.
- This source includes independent BMS/menu volume, compact lobby/filter/skin controls, accuracy grades and SUBARTIST/OBJ credits, localized mods, EX judgement and upload status. Normal/Hard use R5; EX uses R6 with rank-scaled PG14/GR44.571429/GD78.857143ms, fixed BAD250ms and automatic-miss340ms. Preserve R1–R5 replay rules and saved settings. See the [1.9.0 release gate](docs/release-1.9.0-gate.md).
- Precise waits, input MMCSS/power policy, audio timing fixes, Studio/Classic HUDs, ordered note batching and BMS conversion/hold preservation from previous releases remain. Physical latency, real authenticated uploads and long-play/other-PC behavior require separate validation.
- Plugin runtime/API/SDK and major GameSession extraction remain unimplemented follow-up work. Long LN Mix v2 stays local-history/replay only, excluded from legacy BEST and Sites uploads.
- It intentionally excludes local build trees, packaged binaries, caches, user profiles, logs, and private working notes.
- Workspace-local `AGENTS.md` files and private agent notes are not part of the public source bundle. Skin authoring guidance is kept in the regular public skin documentation.
- `SOURCE_PACKAGE_SCOPE.txt` defines the versioned source-bundle inclusion and privacy boundary; listing a file there does not replace archive verification or publication evidence.
- The current source line is `1.9.0`; it keeps P64 and the host beam safety solver authoritative and enables the generalized NK3 pattern MLP only for non-10K sources converted to 10K.
- It supports BMS-family charts, 4K through 10K, 12K, 14K and 16K key modes, and native/LR2/TenRiff skins, keeps MPG/MPEG video BGA decoding with an FFmpeg fallback, and exposes an External ONNX Upscaler that remains off until the user enables it and acknowledges the high-spec warning.
- The repository license is MIT. Keep the top-level `LICENSE` file with any redistributed source bundle.
- Native ASIO output requires a separately installed 64-bit Windows driver. The source contains the project's own narrow driver-interface declarations and backend; no Steinberg SDK or driver installer is bundled. WASAPI remains the default.
- The source bundle includes the code/docs/dependencies needed for a standalone Windows configure/build, but it does not ship the local `10k-calc/` reference checkout or `external/llama.cpp/`.
- The generic integration and compatibility smoke/quantization tools live under `tools/onnx_upscaler/`, but no BGA upscaler model, checkpoint, training data, or model-specific verification metadata is distributed. Bundled ONNX files are limited to the deterministic NK3 P64 graph and the 2K-through-18K generalized pattern MLP inference exports. BGA upscaler users must still supply a rights-cleared model matching the documented 960x540 RGB residual x2 contract.
- NK3 defaults to the bundled ncnn 20260526 shared runtime and converted models. Windows release packages run P64 and the optional MLP through Vulkan on AMD/NVIDIA GPUs and ship `ncnn.dll` plus its BSD license. `TENRIFF_NK3_BACKEND=AUTO|VULKAN|OPENVINO` controls routing, `TENRIFF_NK3_VULKAN_DEVICE=<index>` selects among Vulkan devices, and a source-provided OpenVINO package remains an optional compatibility fallback.
- Official 1.9.0 builds and Windows archives do not build or ship the standalone BMS key-converter CLI/GUI. Its source remains development-only and the top-level CMake option defaults to `TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER=OFF`.
- The staged docs/readmes now track Korean, English, Simplified Chinese, and Japanese entrypoints.
- Typical Windows build flow inside the extracted source-package root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

- AddressSanitizer uses the same local and CI contract: `cmake --preset asan`, `cmake --build --preset asan`, then `ctest --preset asan`. The deterministic unit core runs in one process so tests cannot collide on shared ports. The preset excludes eight exact Windows RawInput/localhost cases and two NK3 inference integration cases; those remain covered by the normal Release/inference checks. The exclusion lists are explicit in `tests/unit/test_bms_parser.cpp`.

- Optional Python-reference checks can print `[skip]` when `10k-calc/` is absent; that is expected for the public source package.
- `launch_win.bat` can create missing `profiles/`, `songs/`, and `logs/` folders on first launch.
- When refreshing this public source bundle for a release, verify at least the standalone configure/build/test path above from the staged source-package root.

The 1.9.0 public release ships the runnable client ZIP, the source ZIP, and SHA-256 checksums. The source bundle contains client code and verifier/build material only; it does not contain ranking-server code, deployment secrets, accounts, profiles, logs, replay/result exports, or upload keys.
