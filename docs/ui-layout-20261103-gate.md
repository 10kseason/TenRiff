> Historical candidate verification. See [1.9.9.9 validation](release-1.9.9.9-gate.md) for the combined release gate.

# Native UI layout verification · 2026-11-03

Unpublished local follow-up to the complete color/sound candidate. The released base remains `28393891aad339aa8f7c21b8244a4e83a8d64422`, with general fixed baseline 1.9.6. Existing installations, profiles, source candidates and supplied sounds are preserved. Use the separate `ui-layout-local` launcher/profile.

The supplied HTML is a visual reference. Its sample data, diagnostic prose, remote font links and scripts are not game logic. The confirmed masked distribution bar, middle difficulty column, row score subtitles and right-side clear heading are omitted. Native Song Select uses ten rows, while explicit authored song-layout overrides retain seven. Existing fonts, real detail metrics, semantic clear colors and verified FULL COMBO glow are reused. Results retain four tabs, existing timing distributions and lane/type/interval metrics, including stored mean/stddev fallback for old records without analysis data.

| Check | Result / scope |
| --- | --- |
| MSVC x64 Release | Client, replay verifier, unit registry and native preview/smoke tools built; final incremental build passed |
| CTest | 3/3, 1,489 unit cases, zero integration skips; optional external 10k-calc reference unavailable |
| Native menu flow | 633/633, including ten-row list, page/wheel wrapping and existing result/menu actions; isolated test profile with no real account or Sites connection |
| Visual cases | 19 unique cases, 247 hit regions, geometry and native dispatch verified. One foreground-limited case passed on a same-binary retry |
| Visual coverage | Song/record/source/empty/search/table, clear score colors/FULL COMBO, all four result tabs, 16-key lane banking, no samples/old analysis-unavailable records/failure, Korean/English/Japanese/Chinese and 960×540 with large UI font |
| Offline skin editor | 37/37; regenerated native catalog check passed. Existing fonts/metrics/colors remain; new result review rectangles are additive |
| Preservation | All 1,470 previous source files remain unchanged at their original location. Eleven gameplay/gauge/record/replay/peer/audio owner files, audio assets and default config are byte-identical; font factory expressions unchanged |
| Local archives | Packaging verifies ZIP CRC and every member's SHA-256; counts/hashes are retained in the outer evidence directory |

Initial compilation found an MSVC nested-lambda default-argument error; explicit arguments/parameter packs repaired it. Initial captures exposed a clipped second title line, crowded histogram labels and an oversized empty-avatar fallback; these were repaired and final captures inspected. Initial foreground-limited native dispatch attempts are retained rather than counted as successful; the final same-binary retry verified the affected song-list case. No final product check remains failed.

The screenshots contain synthetic fixture values. Physical key feel/listening, arbitrary user files/fonts/skins, long play, other PCs and actual two-PC/network-account use remain unverified. No version/baseline promotion, publication, deployment or user-profile migration was performed. Scoring, judgement, replay and peer formats were not changed.
