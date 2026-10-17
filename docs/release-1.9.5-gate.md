# 1.9.5 verification scope

Base: published 1.9.4, 29404df1ce74b70a2391042b3c5d3371e182f6a2. Fixed baseline 1.9.0 is unchanged.

- Release and independent extracted-source builds; CTest, native menu/session smoke and editor regressions.
- Deterministic native ASIO protocol tests for scene PCM, gain/focus, same-track driver changes, failure recovery and ownership release; muted real WASAPI decoding/output progress, missing endpoint rejection and recovery. Bundled title/options/clear/failed MP3 decoding uses the mock ASIO output; scene/preview/game/editor ownership transitions also use a synthetic WAV. No physical listening or ASIO hardware claim.
- Boundary tests at 44.1/48kHz and multiple PG/rate settings; marker expiry, PG refresh and exact-hit text clearing. Native Classic/Studio/Dot/Barcode/player/ghost captures and pixel colors for FAST/maximum/LATE, awarded maximum and expired markers.
- UI random cycle in both directions and OFF/Random labels; saved tokens/algorithms remain unchanged. Four-language current docs and help.
- Package CRC, extracted file hashes, exact final Git source, clean defaults, privacy and no BGA weights; existing NK3 allowlist preserved.
- PR CI, merged-main CI, new immutable tag, draft upload/readback, publication and anonymous readback; preserve all previous release identities and 1.9.4 local sources/packages.

Physical device listening/reconnect, long sessions, other PCs, production account uploads and measured latency/FPS/model quality remain unverified. No website or database deployment. Results are in the release workspace evidence/VERIFICATION.ko.md, not inferred from this plan.
