# TenRiff 1.9.3 release verification gate

This checklist is not evidence that checks have passed. Publication and downloads are determined by the [1.9.3 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.3). Final evidence must identify exact tested commits, package hashes and readback results. Keep prior candidates and initial failed runs as separate evidence.

Scope: [한국어](release-1.9.3.ko.md) · [English](release-1.9.3.en.md) · [日本語](release-1.9.3.ja.md) · [简体中文](release-1.9.3.zh-CN.md).

## Required checks

1. Build Release and run registered CTest, editor tests and native menu/session checks. Check the authored RANK badge, reserved level including editor practice/direct launch, BMS measure timing/scroll positions, retained non-PG FAST/SLOW, independent visibility switches and Safe default with saved Fast preserved. Retain Guest/Online and historical-upload exclusion coverage.
2. Verify Empty Poor gauge/POOR/raw-score/combo behavior and its exclusion only from ordinary/detailed/segment accuracy. Exercise new R7/R8 and historical R1–R6 engine-to-replay/verifier behavior, actual native Sites payloads, distinct boards and unchanged old identities. The approved web version 19 is already deployed; do not insert production test scores as a substitute for local contract checks.
3. Exercise ONNX fixed/dynamic, residual/image, FP32/FP16 and uniform x1–x4 boundaries, invalid contract rejection, legacy Luna equivalence and FFmpeg/alpha/cancellation fallbacks. Check optional Safetensors conversion, metadata/output mode, bounded isolated-process behavior, cache integrity and native fallback without dependencies. Keep trained-model test inputs/outputs and weights outside packages. State when real Spandrel models are not exercised.
4. Inspect both runtime and source archives: CRC, safe unique paths, all extracted hashes, version, Safe/OFF/empty-model/Auto defaults, converter script inclusion, only the existing NK3 model allowlist and **zero BGA weights**. Check privacy, licenses and FFmpeg provenance. Source includes code/tests/docs, not external Python packages or model conversion caches.
5. Independently build/test the final extracted source. Compare source members to exact committed Git blobs and explicitly included dependency extras, then match the tested source tree to the final merged commit. Distinguish working-tree equivalence from exact Git-blob byte identity.
6. Check final PR and merged-main ASan/OpenVINO runs for their exact commits. Create only a new 1.9.3 tag and release assets. Preserve every previous tag/release/asset identity, installation, profile and external model.
7. Download draft assets afresh, then repeat anonymously after publication. Verify names, sizes, SHA-256, ZIP CRC and remote asset digests against final local artifacts. Confirm intended tag/commit/latest-stable status and unchanged prior releases.

## Evidence boundaries

The fixed baseline remains 1.9.0; this release continues published 1.9.2. Fixed grades, Guest exclusion and Sites `client_submitted` status remain. No universal ONNX/Safetensors compatibility, new VEZ timing window, osu! measure generation, video AI or server anti-cheat is claimed.

Report physical input/audio, real production-account upload, long play, other PCs, real Spandrel compatibility and model quality/performance only when directly exercised. Earlier local unit/native/GPU results do not establish final-package or public-delivery results by themselves.
