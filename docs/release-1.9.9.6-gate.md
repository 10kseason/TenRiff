# TenRiff 1.9.9.6 verification gates

This is the source-time acceptance plan, not a completed-check claim. Release notes record fresh outcomes. Candidate results are historical evidence of the same implementation; fixed baseline1.9.6 remains unchanged.

| Gate | Required evidence |
|---|---|
| Release build | Versioned build, focused library/index/table checks and full CTest; separate optional integrations from executed checks. |
| Native behavior | Menu/session smoke including source selection/import, difficulty-table loading, first/improved BEST, timestamp ties, replay/history exclusions and failed saves. Preserve existing identity/selection/record behavior. |
| Rendering | Fresh Classic/Studio/ghost/scaled barcode and dot fixtures; EX/other clear labels, ASSIST, failed results; NEW Record summary/analysis/local multiplayer and geometry. State exact comparison limits. |
| Gauge width | All three options persist through config/presets, default Normal; Classic/Studio/ghost, embedded/fullscreen preview and live cache changes. Horizontal-only geometry with unchanged heights and fill ratios. |
| Loading semantics | No new score/replay formats; normal refresh retained for forced/empty/running/unpublished scans. Table reapplication preserves live metadata and records; same-path content refresh and Native LV restoration. |
| Performance evidence | Label controlled candidate measurements, metadata parity, warm-cache/network assumptions and small cached-opening differences. Do not imply actual user-library or gameplay-start improvements. |
| Source/package | Exact Git-blob source correspondence, ZIP CRC/hash readback, safe defaults, privacy/no local runtime data, licenses and existing model allowlist. Independently rebuild and test the final source ZIP. |
| CI/publication | Exact PR and merged-main revisions pass required checks. New immutable tag, three release assets, fresh draft and anonymous-public hash/size/CRC verification. Preserve earlier releases/tags/assets. |

The local candidates recorded an initial CHECK macro compilation error, a missing benchmark runtime DLL, sandbox cache/GPU failures and a transient native pause-resume timeout. Keep initial failures and fresh rerun evidence; a later pass does not establish the earlier cause.

Physical input/listening, long play, arbitrary charts/skins/models, other PCs, actual two-PC play and real-account uploads remain unverified unless directly exercised. No website/database/public-editor deployment is included. Post-selection gameplay loading is outside this change.
