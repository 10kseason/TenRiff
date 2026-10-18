# Fixed judgement baseline — 2026-10-18

**Fixed judgement policy for 1.9.6.** These values are pinned by the user and must not change without a later explicit instruction. The general source baseline remains **1.9.0**; prior releases, installations and profiles are preserved. See [1.9.6 scope and publication status](release-1.9.6.en.md).

Normal retains PG22/19/15/10ms and its original GR/GD/BAD. Hard reuses the complete Normal NM row (PG/GR/GD/BAD), and EX the complete Normal VHD row, regardless of chart RANK. There is no separate multiplier table or integer-ms rounding. This explicit follow-up supersedes the earlier Hard/EX ratio and rounding request.

All values are **±ms**, the boundary on each side. Normal GR/GD are displayed to six decimal places; their original ratios remain exact internally. VEZ (#RANK 4) has a separate badge but uses EZ timing. Missing or unsupported RANK values also use EZ.

| Mode | Chart rank | PG | GR | GD | BAD |
| --- | --- | ---: | ---: | ---: | ---: |
| Normal | VEZ / EZ | 22 | 65 | 115 | 210 |
| Normal | NM | 19 | 55.714286 | 98.571429 | 180 |
| Normal | HD | 15 | 46.428571 | 82.142857 | 150 |
| Normal | VHD | 10 | 24.761905 | 43.809524 | 80 |
| Hard | All chart ranks / NM | 19 | 55.714286 | 98.571429 | 180 |
| EX | All chart ranks / VHD | 10 | 24.761905 | 43.809524 | 80 |

GR/GD/BAD retain the exact Normal EZ65/115/210ms values multiplied by1,18/21,15/21,8/21. Hard selects NM and EX selects VHD without further tightening or widening. Six decimal places are display-only; boundaries are converted to integer audio samples only at the final runtime step.

The stored `judge.pg=21ms` remains a legacy calibration value for profile compatibility. New runtime windows apply `stored PG / 21` to the pinned table, so the default stored 21ms produces Normal EZ PG 22ms. Saved profiles are not migrated or overwritten and distinct custom values are not replaced with defaults. Custom records remain noncanonical under custom 9/10 IDs. Peer protocol moves to v7 with the same packet layout, explicitly rejecting v6 peers that use different windows.

## Records and compatibility

New Normal/Hard uses R9; EX uses R10. Historical R1–R8 replays, scores and board identities retain their original timing and calculations and are not recalculated under the new table. Empty Poor accuracy exclusion, hold/mask policies, the 340ms automatic-miss deadline, score factors, gauges and the fixed grade ladder remain unchanged. For 1.9.6, Sites accepts only new standard Online R9/R10 submissions and rejects new submissions under older rules. Existing boards and records remain legacy read-only. Guest, unmarked historical, custom and LN Mix v2 exclusions and the `client_submitted` boundary remain. The separate Ranked verifier still blocks rules it does not support.

See [timing windows](judgement-windows.md) for boundaries, LN and historical rules, and [config](config.en.md) for settings.
