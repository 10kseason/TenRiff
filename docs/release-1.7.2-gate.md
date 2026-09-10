# TenRiff 1.7.2 Release Gate

This release combines the verified local 1.7.1 follow-ups with native Windows ASIO output and an explicit Native LV choice in the difficulty-table picker. The source is based on the current main branch, retaining its configuration and menu guidance.

## User-visible changes

- Native Windows ASIO output talks directly to an installed 64-bit driver. WASAPI remains the default. Driver installation is separate from TenRiff; see [ASIO setup](asio-audio.md).
- The difficulty-table picker offers Aery 5K, Aery 7K, Revive 10K and **F4 Native LV**. Native LV clears the external table and restores native display, sorting, grouping and numeric filters. A valid song cache avoids a full rescan. The normal default is BMS `#PLAYLEVEL`; profiles with calculated difficulty enabled retain their cached calculated LV.
- Portable `.trskin` files include skin configuration and required assets for transfer to another PC. See [skin preset format and limits](skin-presets.md).
- Sources supports folder addition and Del removal from the saved list. Chart, audio and folder files are retained, including after removal of the final source.
- Unicode paths work in the NK3 model, profile and replay-verifier validation paths. Multiplayer retains the finished round's participants and scores when opponents leave.
- Single-player pause is immediate; Continue or Esc resumes after a three-second countdown. Audio, chart time and scoring remain frozen during the countdown. Esc cancels a resume countdown back to pause.
- Session Mix ignores Esc during loading, the initial countdown and play. Existing between-stage result exits and multiplayer abort controls are retained.
- Reference BPM is the tempo with the longest cumulative running time. STOP waiting is excluded; repeated tempo sections add together. Initial BPM remains separate for deterministic replay and key/LN conversion. Cache v15 rebuilds older metadata. See [reference BPM](reference-bpm.md).
- Grade/Session Mix uses the LR2-reference gauge with indirect misses, low-HP damage scaling, carry between songs and failure below 2%. Ordinary Easy/Normal/Hard tuning remains unchanged. See [comparison and compatibility limits](lr2-gauge-audit.ko.md).

## Verification status

The following results were obtained from the 1.7.2 source on Windows x64. Both Release and ASan used a Korean installation path and a Korean TEMP/TMP directory. They include the new ASIO, Native LV, pointer mapping, and final-buffer error/export regressions.

| Check | 1.7.2 status |
| --- | --- |
| Windows x64 Release client and replay-verifier build | PASS; client, verifier, unit tests, NK3 smoke, judgement benchmark and native ASIO probe built |
| Release CTest and targeted ASIO/Native LV regressions | PASS: 3/3 CTest targets; 769 unit cases, 0 integration exclusions; NK3 324 routes passed |
| MSVC AddressSanitizer suite | PASS: 2/2 CTest targets; 759 unit cases, 10 existing integration exclusions |
| Native ASIO device initialization/callback smoke | FAILED on the registered Realtek driver: rate readback 0 and setter `-1000 (NotPresent)` at 44.1/48 kHz; playback did not start. Native protocol mock tests: 10/10 PASS. |
| Extracted source configure/build/test | PASS: 868 public source files archived/extracted with matching hashes; standalone verifier/test/NK3/benchmark build and CTest 3/3 (769 cases). Final source-code hashes remain identical; only this verification document was completed afterward. |

The ASan exclusions are the existing Windows runtime and NK3 integration cases. No new exclusions were added. The optional external `10k-calc` Python reference is absent from the public source package and prints its existing skip notice. Final compiler logs contain no compiler warnings or errors.

Validation logs are retained outside the source package: `ctest-release-final.log`, `ctest-asan-final.log`, `asio-focused-tests.log`, and `asio-realtek-final.stderr.log`. The Realtek failure remains recorded; exact current-rate drivers now avoid redundant setters, while invalid/mismatched rate readback is still rejected. ASIO device compatibility and audible playback are not release PASS claims.

Historical evidence remains in [local 1.7.1](local-1.7.1-update.ko.md) and [local 1.7.1 r2](local-1.7.1-r2.ko.md). The r2 Release CTest 3/3 with 749 checks and ASan 2/2 with 739 checks/10 integration exclusions are historical counts only.

## Manual coverage and limits

An ASIO callback smoke does not establish audible output quality, hardware latency, stable long-session playback or compatibility with every vendor driver. Actual keyboard-to-audio gameplay and device changes require manual checks on the selected device. Original LR2 executable A/B remains unverified, and LR2-unsupported charge-LN behavior retains the documented TenRiff handling.

## Distribution

Archive integrity is checked after packaging by CRC, extraction, complete file inventory and SHA-256 comparison against the tested source and binaries. Published tag and downloaded-asset hashes are checked after upload. These post-packaging results and final hashes are recorded in the GitHub release notes and its checksum asset, so the archives do not need to contain a self-referential hash of themselves.

- `TenRiff-1.7.2.zip`: Windows x64 client, replay verifier and runtime assets.
- `TenRiff-1.7.2-source.zip`: curated standalone source and build dependencies.
- `TenRiff-1.7.2-SHA256SUMS.txt`: hashes of the distributed ZIPs.

No songs, user profiles, private local paths, driver installer, standalone BMS converter executables or BGA upscaler model are included. Source package scope is defined by [SOURCE_PACKAGE_SCOPE.txt](../SOURCE_PACKAGE_SCOPE.txt).
