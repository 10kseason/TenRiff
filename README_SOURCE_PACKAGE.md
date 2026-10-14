TenRiff source package notes (`1.9.0.1`)

- Fixed baseline: [1.9.0](docs/baseline-1.9.0.en.md), explicitly selected on 2026-10-14. The immutable baseline tag identifies the starting source. The current release commit and delivered ZIP hashes are recorded in the version-specific release notes and checksum asset. Previous 1.8.7 baseline and published tags/assets are preserved.
- This source adds preview attenuation, assignable Hi-Speed -10/+10, Xbox/XInput buttons, owned single-instance file pickers and BMS BPM-before-STOP correction. Optional timing-version metadata preserves historical replay schedules; affected new version-2 records stay local until the online service supports them. Normal/Hard remain R5 and EX remains R6; numerical judgement rules are unchanged. See the [1.9.0.1 release gate](docs/release-1.9.0.1-gate.md) and [compatibility notes](docs/feedback-20261015.en.md).
- Precise waits, input MMCSS/power policy, audio timing fixes, Studio/Classic HUDs, ordered note batching and BMS conversion/hold preservation from previous releases remain. Physical latency, real authenticated uploads and long-play/other-PC behavior require separate validation.
- Plugin runtime/API/SDK and major GameSession extraction remain unimplemented follow-up work. Long LN Mix v2 stays local-history/replay only, excluded from legacy BEST and Sites uploads.
- It intentionally excludes local build trees, packaged binaries, caches, user profiles, logs, and private working notes.
- Workspace-local `AGENTS.md` files and private agent notes are not part of the public source bundle. Skin authoring guidance is kept in the regular public skin documentation.
- `SOURCE_PACKAGE_SCOPE.txt` defines the versioned source-bundle inclusion and privacy boundary; listing a file there does not replace archive verification or publication evidence.
- The current source line is `1.9.0.1`; it keeps P64 and the host beam safety solver authoritative and enables the generalized NK3 pattern MLP only for non-10K sources converted to 10K.
- It supports BMS-family charts, 4K through 10K, 12K, 14K and 16K key modes, and native/LR2/TenRiff skins, keeps MPG/MPEG video BGA decoding with an FFmpeg fallback, and exposes an External ONNX Upscaler that remains off until the user enables it and acknowledges the high-spec warning.
- The repository license is MIT. Keep the top-level `LICENSE` file with any redistributed source bundle.
- Xbox/XInput uses Windows system XInput 1.4 or 9.1.0; no gamepad driver or XInput DLL is bundled. Digital buttons and triggers are supported; analog stick axes and vibration are not included. Physical-controller verification remains separate from simulated input tests.
- Native ASIO output requires a separately installed 64-bit Windows driver. The source contains the project's own narrow driver-interface declarations and backend; no Steinberg SDK or driver installer is bundled. WASAPI remains the default.
- The source bundle includes the code/docs/dependencies needed for a standalone Windows configure/build, but it does not ship the local `10k-calc/` reference checkout or `external/llama.cpp/`.
- The generic integration and compatibility smoke/quantization tools live under `tools/onnx_upscaler/`, but no BGA upscaler model, checkpoint, training data, or model-specific verification metadata is distributed. Bundled ONNX files are limited to the deterministic NK3 P64 graph and the 2K-through-18K generalized pattern MLP inference exports. BGA upscaler users must still supply a rights-cleared model matching the documented 960x540 RGB residual x2 contract.
- NK3 defaults to the bundled ncnn 20260526 shared runtime and converted models. Windows release packages run P64 and the optional MLP through Vulkan on AMD/NVIDIA GPUs and ship `ncnn.dll` plus its BSD license. `TENRIFF_NK3_BACKEND=AUTO|VULKAN|OPENVINO` controls routing, `TENRIFF_NK3_VULKAN_DEVICE=<index>` selects among Vulkan devices, and a source-provided OpenVINO package remains an optional compatibility fallback.
- Official 1.9.0.1 builds and Windows archives do not build or ship the standalone BMS key-converter CLI/GUI. Its source remains development-only and the top-level CMake option defaults to `TENRIFF_BUILD_STANDALONE_BMS_KEY_CONVERTER=OFF`.
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

The 1.9.0.1 release package set comprises the runnable client ZIP, the source ZIP, and SHA-256 checksums. The source bundle contains client code and verifier/build material only; it does not contain ranking-server code, deployment secrets, accounts, profiles, logs, replay/result exports, or upload keys.
