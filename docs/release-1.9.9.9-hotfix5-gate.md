# TenRiff 1.9.9.9-hotfix5 validation

- Build Windows x64 Release and run CTest, native menu smoke and offline editor tests. Build and test the extracted standalone source archive, and require PR/main sanitizer and NK3 CPU CI.
- Check original Auto/None/Native modes with remembered KRRCREAM/NK2/NK3 settings and 4/7/8/10/16K output. Confirm explicit same-key targets and unknown/missing modes do not emit the native marker.
- Feed exact native C++ payloads through the actual local web upload/ranking handlers. Check native inclusion, conversion exclusion, best-score selection, detailed precision, ties, totals and historical native policy isolation.
- Confirm website v25 remains compatible through public GET only. Retain the user's exclusion of ambiguous historical records; do not relabel, delete or resubmit stored scores. No production test account/key/score writes.
- Verify both ZIPs for CRC, all-file hashes, privacy/defaults and exact Git source blobs. Read back all three assets after draft upload and public publication, preserving prior releases/tags/assets.

All hotfix4 gameplay/skin/loading behavior and fixed baseline 1.9.6 remain. Scoring, judgement, replay format and R13/R14 policies are unchanged. Physical play, account score upload, long sessions and other PCs require separate verification.
