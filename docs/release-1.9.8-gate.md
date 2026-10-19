# TenRiff 1.9.8 verification gates

This document defines required checks, not a claim that they have completed. Exact results, failed attempts, limitations and hashes belong in the release evidence and published assets. The release starts from published 1.9.7 (`948518dbc343e78acb1d7a7e47f7551b05fda159`); the general fixed baseline remains 1.9.6. No website, database or public Skin Editor deployment is part of 1.9.8.

- Build Release and run focused configuration/controller/preview tests, full CTest, native menu/session checks, skin-editor, scene-music, FFmpeg and Safetensors process checks. Preserve 1.9.7 common binding persistence/collisions, colored tabs, percentage increments, fresh play/Retry seeds, recorded replay seeds, seed-dependent ghost checks and scene gain.
- Verify all four restored X rows are adjacent to their corresponding Y rows for Native/TenRiff/LR2. Exercise localized search, keyboard and pointer changes, bounds of −600..600px in 10px steps, retained saved X/Y and profile/portable-skin round trips. Changing X must leave Y unchanged.
- Verify optional `skin.lane_divider_brightness` defaults to 1, accepts finite 0..1 with safe fallback/clamping, and uses 1% UI steps. Exercise 0%, intermediate and 100% preview/Classic/Studio/ghost rendering and same-window cache refresh. Compare default 100% with 1.9.7; separate divider pixels from unrelated animated content. RGB changes must preserve alpha, visibility, width, measure lines and shared brush color.
- Capture changed settings in four languages. Record native input dispatch separately from geometry/pixel checks; synthetic fixtures do not prove physical keyboard, IME or device behavior.
- Run PR and merged-main ASan/OpenVINO CI for their exact commits. Preserve failing attempts and fixes.
- Build runtime/source/checksum assets in a fresh directory, check ZIP CRC and extracted hashes, clean defaults, privacy, exactly the existing 18 NK3 ONNX files and no BGA model weights. Official binary archives continue to exclude standalone BMS key-converter executables.
- Create the final source archive from the merged Git blobs. Extract it independently and build/test it again. Optional external 10k-calc reference tests may be unavailable in the public package; report that separately from the CTest runner status.
- Read back all three draft assets, publish only after verification, then anonymously download all three public assets into another fresh directory and compare hashes. Preserve every prior release/tag/asset identity and the original 1.9.6 skin-X/Y local candidate.

Compatibility: fixed Normal/Hard/EX timing, R9/R10 and R1–R8 historical replay rules, peer v7, grades, Guest/custom/LN Mix v2 exclusions, client_submitted and saved settings remain. The existing X coordinate fields are exposed again without migration; the optional brightness field defaults to existing rendering. 1.9.7 fresh seeds and recorded replay behavior remain intact.

Physical input/IME/listening, long play, other PCs, arbitrary imported skins, actual two-PC play and real-account production uploads require separate validation. No claim of these checks follows from the automated gates.
