# Final5 release gates

- Windows Release build; CTest 4/4, 1547 unit cases with zero integration skips; native 747/747; offline editor 41/41.
- Repeat CTest and native smoke using only the exact extracted source ZIP, without a Git directory or files from the working source.
- LR2 synthetic DB integrity and ghost/EX parity, overlapping roots, early completion, missing/unsupported authored RANK rejection, cancellation and atomic save; no personal DB or import output in either archive.
- Andro: 20 local chart hashes, note counts, scheduled-note hashes and duration preserved; reference BPM and Safe/Fast metadata all 165. Synthetic timing v1/v2 tests preserve note/measure/scroll schedules at 1/44.1/48/96 kHz.
- Exact Git source blobs, ZIP CRC, file hashes, privacy scan and unchanged shipped config; NK3 allowlist exactly 18, no BGA upscaler weights, Video AI disabled.
- PR and main ASan/OpenVINO CI success; draft and anonymous public three-asset readback; preserve all previous releases, tags and assets.

Fixed baseline remains 1.9.9.9-final4. No production account/key/score writes, DB migration or website deployment. Physical input/listening, long play, other PCs and complete personal-DB timings remain separate user checks.
