# TenRiff 1.9.9.9-final5

[한국어](release-1.9.9.9-final5.ko.md) | [English](release-1.9.9.9-final5.en.md) | [日本語](release-1.9.9.9-final5.ja.md) | [简体中文](release-1.9.9.9-final5.zh-CN.md)

Reduces LR2 import hashing and chart lookup work, rejects EX for absent or unsupported authored RANK, and fixes empty trailing measures making Andro display BPM 1. The fixed baseline remains final4.

- Compute only the MD5 needed by LR2; EX checking no longer resolves audio/background paths. Avoid overlapping folder traversals and stop once every target chart is found.
- A sequential same-PC test with 512 synthetic records and 15,569 local charts took 91.139s before and 9.852s after. Lamp/EX JSON matched exactly for that sample. This is not a timing guarantee for personal databases or other PCs.
- Missing/out-of-range authored RANK now keeps only the base LR2 lamp instead of incorrectly proving EX through gameplay's EASY fallback. The original DB remains read-only; cancel/failure preserves the previous import.
- Reference BPM excludes length-only/all-00 padding after the last nonempty event measure. All 20 checked Andro charts and Safe/Fast index entries report 165; note times, counts and duration stay unchanged. Index cache v22 rebuilds older derived metadata once.
- Imports remain display-only, with no changes to TenRiff results, BEST, upload eligibility, judgement/gauge or existing R1–R14 replay policies. No personal DB or original Andro chart ships. The source includes only two synthetic test databases.
- The local candidate passed CTest 4/4 (1,547 cases, zero skips) and native 747/747. Packaging, source-ZIP-only rebuild, CI and public downloads are release gates. Physical input, listening, long play, other PCs and complete personal-DB timings remain unverified.

[LR2 import](lr2-clear-import.en.md) · [Reference BPM](reference-bpm.md) · [Release gates](release-1.9.9.9-final5-gate.md)
