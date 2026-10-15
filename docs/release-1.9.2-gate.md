# TenRiff 1.9.2 release verification gate

This is the release checklist, not evidence that the checks have passed. Publication and asset availability are determined by the [1.9.2 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.2). Version-specific final evidence must identify the tested commit and package hashes. Earlier candidates and failed runs remain separate evidence.

Scope: [한국어](release-1.9.2.ko.md) · [English](release-1.9.2.en.md) · [日本語](release-1.9.2.ja.md) · [简体中文](release-1.9.2.zh-CN.md).

## Required checks

1. Run the Release build, registered CTest suite, editor tests and native menu/session checks. Verify the startup F10 choice, Guest on every launch/Esc, Online fresh-play eligibility, irreversible active-play revocation, pending/in-flight upload boundaries, historical/guest refusal in Sites and Ranked, and unchanged local replay playback.
2. Verify the visible skin-row order, absence of skin export, preserved preset import/profile export, fixed grade boundaries, brightness, shared loading and language selection.
3. Exercise FFmpeg deblock with alpha, concurrent workers, cancellation, timeout/missing-executable fallback. Compare fixed and dynamic ONNX paths with independent full-image references at square, portrait, odd and FHD sizes; reject invalid tensor contracts. Keep externally supplied trained-model tests outside public packages. These checks do not establish universal visual quality or real-time performance.
4. Inspect the runtime and source ZIPs: CRC, safe unique paths, exact extracted hashes, clean defaults, empty model path/OFF, existing NK3 allowlist, **no BGA upscaler weights**, and no private paths, profiles, credentials, logs or test output. Verify the FFmpeg runtime and its corresponding provenance/notices separately.
5. Build and test the independently extracted final source package. Verify source files against exact committed Git blobs and any explicitly included dependency extras. Match the tested source tree to the final merged commit.
6. Check the final PR and merged-main ASan/OpenVINO CI runs for their exact commits. Create only a new 1.9.2 tag and assets; preserve all previous release/tag/asset identities.
7. Download the new draft assets afresh, then repeat anonymously after publication. Check names, sizes, SHA-256, ZIP CRC and remote asset digests against the final local artifacts. Confirm the release points to the intended commit and that earlier releases are unchanged.

## Evidence boundaries

The source baseline remains 1.9.0; this release builds on user-selected 1.9.1.1 and does not repin that baseline. No server/DB migration is included. Sites remains `client_submitted`, and client Guest/Online checks are not server-side replay verification or anti-cheat.

Report physical input/audio, actual production-account submissions, long sessions, other PCs and trained-model quality/performance only when directly exercised. Successful unit tests, synthetic native dispatch and small inference fixtures do not establish those outcomes.
