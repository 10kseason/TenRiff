# TenRiff 1.9.4 verification procedure

Release scope: user-selected 1.9.3 plus the 2026-10-17 indicator, output, profile-key, speed, measure-line and deferred-reclamation changes. Fixed baseline remains 1.9.0. Earlier dated release documents describe their own historical verification.

Required gates before publication:

1. Release build and CTest, with real BMS timing/RANDOM fixtures enabled; focused retention tests verify PCM survives runtime maintenance and dies at shutdown, and concurrent/nested deferred trims coalesce until menus.
2. Native MenuApp checks, skin-editor checks, and native visual fixtures for both indicator shapes/expiry, remapped keys, four-language settings and measure-line pixels at multiple heights/player/ghost.
3. Native BGA cache probe: 257 static 512×512 images exceed both old LRU limits, remain through pause, and become zero at a menu transition. This proves cache lifetime, not an FPS/latency improvement.
4. Clean defaults, ZIP CRC, all extracted hashes, source identity, privacy scan and zero BGA weights (existing NK3 allowlist retained). Independently rebuild/test the delivered source; do not infer delivery from a packaging exit code alone.
5. PR and merged-main sanitizer/OpenVINO checks for their exact commits, immutable new tag, draft asset readback, publication and anonymous asset readback. Compare previous release/tag/asset identities and preserve the original local candidate.

Evidence lives beside the release source in `evidence/VERIFICATION.ko.md`; gate descriptions are not a claim that execution has passed. Physical keyboard/controller/audio routing, unplug/replug behavior, long sessions, other PCs, production-account uploads and arbitrary external model quality remain unverified unless separately recorded. This release does not deploy a website/database or change the `client_submitted` trust boundary.
