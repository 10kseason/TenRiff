# TenRiff 0.9.92 Baseline

This document is the `0.9.92` baseline that should be used as the reference point for future TenRiff work. Its purpose is to quickly pin down "what should be treated as the current default, what must be preserved, and what scope future work should be stacked within."

## Release Identity
- Baseline release line: `0.9.92`
- Current main target: Windows GUI build
- Default product surface: BMS-first
- Optional extension surface: `.osu` osu!mania 4K-10K
- Distribution paths:
  - Windows package: `Baepoks/TenRiff-0.9.92`
  - Public source package: `opensource-Tenriff-source/TenRiff-0.9.92-source`
- Distribution build source of truth: `build-dist/Release`

## Product Base
- The menu entry point is the `MenuApp` + `MenuWindow` combination.
- The default user flow is `Title -> Song Select -> Gameplay -> Result`.
- Song Select provides cache-first loading, mixed mouse/keyboard control, external folder drag-and-drop, and recent-source reopening by default.
- Gameplay uses `GameSession` to handle chart loading, input, HUD snapshots, chart-audio preparation, and the boundaries around session end.
- Rendering uses D3D11 + Direct2D/DirectWrite, audio uses WASAPI, and input is built on RawInput or high-rate polling.

## Baseline Capabilities
- The BMS parser / normalizer / timeline pipeline should be treated in its current hardened state for real-world compatibility.
- BMS explicit compact layouts (`#4K`, `#6K`, `#8K`) and SP compact layouts (`5+1 SP`, `7+1 SP`) are already supported as baseline behavior.
- BMS long notes should be preserved in the current implementation state, including LN channel handling, `#LNOBJ`, and `#LNMODE 2` charge-tail judgement.
- Song indexing should be treated as operating on the `safe` default profile for large libraries.
- osu!mania is disabled by default, but when enabled it is part of the baseline that 4K-10K are supported in both the menu and runtime.
- The result screen, replay/result JSON export, and per-song local record accumulation are already included baseline functionality.

## Baseline Defaults
- Default judgement windows:
  - `GOOD = 75ms`
  - `BAD = 340ms`
  - `indirect_miss = 340ms`
- The gameplay pre-start `3 / 2 / 1` countdown is part of the baseline.
- The result transition tail after gameplay is based on `ui.result_tail_ms = 3000ms`.
- In-game Hi-Speed keeps `F3/F4` for fine adjustment and `F5/F6` for coarse adjustment.
- The baseline expects `F3/F4` to repeat while held, and for any Hi-Speed change made during play to be saved back into the profile.
- Default gauge behavior:
  - `Hard`, `Normal`, and `Easy` all start at `100%`
  - auto-shift thresholds are `Hard -> Normal = 66` and `Normal -> Easy = 33`
  - `BAD/PR` damage is `Hard -4 / Normal -2 / Easy -2`
  - Easy gauge applies an extra `0.90x` softening to `BAD/PR` damage when already at `25%` or lower
  - `PG` recovery is `Hard 0.01 / Normal 0.01 / Easy 0.032`
  - `GR` recovery is `Hard 0.05 / Normal 0.05 / Easy 0.0016`
  - `GD` recovery is `Hard 1/65 / Normal 1/65 / Easy 0.00064`

## Baseline UX
- The Song Select left navigation should preserve the current quick-access structure for sort/filter, source management, browse, mode/mod, records, and options.
- The left navigation and main menu buttons should remain immediately selectable/executable with left mouse clicks.
- Right-click should keep the current secondary UX path that moves one step back to the previous button selection in the active button column.
- The current guidance UX such as `F1` help, `F5` reindex, and the Song Select command bar should be preserved.

## Baseline Constraints
- Prefer local modifications only; do not redesign paths that are already stable.
- Linux is still preview-level, and the Windows GUI path is the baseline.
- Current code and `docs/current-state.en.md` take precedence over older design documents.
- Future work should favor additive changes that do not break the `0.9.92` baseline.

## What To Preserve In Follow-Up Work
- BMS-first surface
- Large-library safe indexing stability
- Cache-first loading in the menu
- The separation of gameplay HUD / audio / input
- The current result export / local records path
- UTF-8 / Korean path support
- The `0.9.92` packaging rules and no-songs distribution layout

## Recommended Companion Docs
- Current implementation state: [`docs/current-state.en.md`](current-state.en.md)
- Config / profile structure: [`docs/config.en.md`](config.en.md)
- Menu / state flow: [`docs/menu.en.md`](menu.en.md)
- Play loop / audio / input boundaries: [`docs/core-loop.en.md`](core-loop.en.md)
