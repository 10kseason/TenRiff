# TenRiff 1.9.9.5 verification gates

**This is the source-time release acceptance plan, not a completed-check claim.** Fresh outcomes and publication status are recorded in the release notes. Historical candidate results are not fresh release evidence. The general fixed baseline remains **1.9.6**; this release continues 1.9.9.3 and its subsequent presentation, BGA and render-wait changes.

[한국어 scope](release-1.9.9.5.ko.md) · [English scope](release-1.9.9.5.en.md) · [日本語 scope](release-1.9.9.5.ja.md) · [简体中文 scope](release-1.9.9.5.zh-CN.md)

| Gate | Acceptance requirement | Required coverage |
| --- | --- | --- |
| Build and automated checks | Required | Versioned Release build, focused changed-behavior checks, complete CTest and native menu/session smoke; distinguish optional unavailable integrations. |
| Simple LN rendering | Required | All eight non-default native shapes, player/ghost, active/released holds, original rect and imported art preservation, alpha/taper/width behavior. |
| ALL COMBO | Required | Whole-chart completion through the final LN tail, independent player/ghost state, excluded runs, result display, cached art and text fallback. |
| EX and rank presentation | Required | Exact seven RGB stops and positions, S+/S++/S+++ cached text effects, non-EX palette preservation, Classic/Studio/player/ghost, badges, Shift Start and fallback paths. No rank PNGs in runtime packages. |
| Key overlay | Required | No difficulty estimate; KPS/key viewer, supported modes, default OFF, saved positions and lifecycle retained. Chart/library difficulty remains. |
| BGA and render waiting | Required | Forward PTS never regresses during stall recovery; explicit rewind, sparse frames, EOF, failed/inaccurate seeks and long GOP progress remain. Bounded stale-oversleep recovery preserves normal wait behavior. |
| Supporting tools and compatibility | Required | Offline editor/catalog, scene music, FFmpeg and Safetensors checks as applicable. Preserve timing, grade thresholds, R1–R10, peer v7, LN Mix v1/v2/v3, NK2 E and existing online exclusions. |
| Source and package integrity | Required | Exact version/source correspondence, runtime/source/checksum ZIPs, CRC and extracted hashes, licenses/provenance, safe defaults, no private paths/diagnostics or BGA weights, and existing model allowlist. Independent final-source rebuild/checks remain separate. |
| CI and publication | Required | PR and merged-main checks for their exact revisions; immutable new tag/assets, fresh draft and anonymous public asset readback with matching hashes. Preserve every prior release, tag and asset. |

Synthetic rendering, simulated stalls and deterministic timer tests do not establish physical input, audible behavior, end-to-end Discord/OBS synchronization, sustained-play reliability or measured performance gains. Other PCs, arbitrary charts/skins/models, actual two-PC play and real-account production uploads remain unverified unless directly tested. Replay initialization alone does not prove scored playback.

No website, database or public skin-editor deployment is part of this release. Publication and downloads are determined by the [1.9.9.5 release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.5). Publish fresh outcomes only after the corresponding checks complete; do not copy candidate pass counts as release results.
