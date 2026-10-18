# TenRiff 1.9.7 verification gates

This document defines required checks, not a claim that they have completed. Exact results and hashes belong in the release evidence and published assets. No website or database deployment is part of 1.9.7.

- Build Release and run CTest, native menu/session checks, skin-editor, scene-music, FFmpeg and Safetensors process checks. Exercise common binding persistence/collisions, skin tabs and percentage increments, labels, fresh first-play/Retry seeds, preserved replay seeds and seed-dependent ghost rejection.
- Capture the changed native settings screens. Record native input dispatch separately from geometry/pixel checks; synthetic fixtures do not prove physical keyboard, IME or device behavior.
- Run PR and merged-main ASan/OpenVINO CI for their exact commits. Preserve failing attempts and fixes.
- Build runtime/source/checksum assets in a fresh directory, check ZIP CRC and extracted hashes, clean defaults, privacy, exactly the existing 18 NK3 ONNX files and no BGA model weights.
- Create the final source archive from the merged Git blobs. Extract it independently and build/test it again. Optional external 10k-calc reference tests may be unavailable in the public package; report that separately from the CTest runner status.
- Read back all three draft assets, publish only after verification, then anonymously download all three public assets into another fresh directory and compare hashes. Preserve every prior release/tag/asset identity and the original local candidate.

Compatibility: fixed Normal/Hard/EX timing, R9/R10 and R1–R8 historical replay rules, peer v7, grades, Guest/custom/LN Mix v2 exclusions, client_submitted and saved settings remain. Fresh seeds can coincidentally produce the same layout. Seed-dependent ghosts sharing a chart still require compatible seeds; seed-independent Original OFF/Mirror does not.

Physical input/IME/listening, long play, other PCs, arbitrary imported skins, actual two-PC play and real-account production uploads require separate validation. No claim of these checks follows from the automated gates.
