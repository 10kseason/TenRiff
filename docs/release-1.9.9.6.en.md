# TenRiff 1.9.9.6 — Indicator/result feedback and library loading

1.9.9.6 continues released 1.9.9.5 with indicator/result feedback and song-library, folder and difficulty-table loading changes. The fixed baseline remains **1.9.6**.

- Historical/latest barcode markers share 2×18 base dimensions. Latest-hit opacity, dots, colors, lifetime, placement and scaling remain unchanged.
- Results display `EX CLEAR` instead of `GAUGE SHIFT EX CLEAR`; other gauges lose only the same prefix. ASSIST identity and stored status remain.
- Show `NEW Record` for the first eligible local BEST or a meaningful improvement under the existing clear-first ordering. Timestamp-only ties, replay playback, historical results and failed saves do not qualify. Existing autoplay/practice/added-note/LN Mix exclusions remain.
- Switching a difficulty table reapplies it to loaded chart hashes instead of rereading song caches and records/replay verification. Same-URL content is still fetched. Single folders and ALL SONG share this path; Fast-to-Safe changes, empty libraries and running/unpublished scan results retain normal refresh.
- Candidate counting skips duplicate path/stat work; parsed caches move and selection reads visible indices instead of copying entire metadata vectors. F5 validation, cancellation, permission handling and bounded batches remain. Switching a table is not a refresh of externally changed files.

Controlled local-candidate medians for this implementation were 1,774→1,321ms (25.6%) for 2,000 charts/200 folders and 383→103ms (73.1%) for table application over 10,000 entries. Cached-library opening234→231ms is within variation; no noticeable improvement is claimed there. Three serial pairs reversed the second pair's order; HTTP was a local fixture, records were empty and OS caches were not cleared. This does not measure a real user library, cold-disk access, server latency or other PCs. Candidate measurements are not presented as fresh release benchmarks.

Post-selection gameplay loading is outside this change. Input/audio/judgement/score/gauge calculation, fixed grade boundaries, save/replay formats, R1–R10, peer v7 and existing online exclusions remain. The new-record flag is session-only. No website, database or public skin-editor deployment is included.

[Verification gates](release-1.9.9.6-gate.md) · [Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.6). Fresh outcomes are reported only after checks finish; older local and release notes remain historical.
