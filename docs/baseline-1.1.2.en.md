# TenRiff 1.1.2 Final Stable Baseline

This document defines the `1.1.2 final stable` baseline that future TenRiff work should build on. The goal is to lock down what counts as the current stable contract, what must be preserved, and what boundaries new work should respect.

## Release Identity
- Baseline release line: `1.1.2`
- Release label: `final stable`
- Current main target: Windows GUI build
- Default product surface: BMS-first
- Optional extension surface: `.osu` osu!mania 4K-10K
- Packaging paths:
  - Windows package: `Baepoks/TenRiff-1.1.2`
  - Public source package: `opensource-Tenriff-source/TenRiff-1.1.2-source`
- Packaging build source of truth: `build-dist/Release`

## Stable Contract
- Keep the gameplay input-timing correction that was anchored to the real playback head in `1.0.9`.
- Prefer saved RawInput for live gameplay input capture, while keeping bound-key polling shadow and Polling fallback on RawInput startup failure so input recognition stays alive.
- Keep the gameplay session on an always-allow input gate regardless of foreground state.
- Keep menu input on the foreground process/root-window boundary.
- Preserve saved `input.backend` / `input.rawinput` values instead of rewriting them from runtime fallback behavior.
- Keep shipping `Mainmusic/` in Windows distribution packages so menu BGM works out of the box.
- Keep excluding `external/llama.cpp/` from the public source package.

## Product Base
- The menu entry point is the `MenuApp` + `MenuWindow` pair.
- The default user flow is `Title -> Song Select -> Gameplay -> Result`.
- Song Select keeps cache-first loading, search/sort/filter flows, external folder drag-and-drop, and recent-source reopening.
- `GameSession` owns chart loading, gameplay audio prep, HUD snapshots, and gameplay shutdown boundaries.
- Rendering is built on D3D11 + Direct2D/DirectWrite, audio on WASAPI, and input on RawInput or high-rate polling.

## Baseline Capabilities
- The BMS parser/normalizer/timeline pipeline should be treated as the current real-world-compatible baseline.
- BMS explicit compact layouts (`#4K`, `#6K`, `#8K`) and SP compact layouts (`5+1 SP`, `7+1 SP`) are baseline features.
- BMS long-note support includes LN channels, `#LNOBJ`, and `#LNMODE 2` charge-tail judgement.
- BMS audio support includes native WAV, OGG/MP3 fallback paths, `ffmpeg.exe` fallback, and `follow/autoplay/ignore` keysound modes.
- Song indexing keeps both the default `safe` profile and optional `fast` profile, but large-library stability is judged against `safe`.
- osu!mania is off by default but remains a supported optional surface from 4K to 10K in the menu/runtime.
- Result screens, replay/result JSON export, local records, and ghost-comparison flows are baseline functionality.

## Baseline Defaults
- Default judgement windows:
  - `GOOD = 75ms`
  - `BAD = 340ms`
  - `indirect_miss` is folded into the same value as `BAD` in the current runtime.
- Long-note tail defaults:
  - `hold_grace = 80ms`
  - `hold_break = 200ms`
- The `3 / 2 / 1` countdown before gameplay is part of the baseline.
- The result-screen tail uses `ui.result_tail_ms = 3000ms`.
- All three gauges (`Hard`, `Normal`, `Easy`) start at `100%` and fail immediately at `0%`.
- Automatic gauge shifting is not part of the baseline.
- The default song-index profile is `safe`.

## Baseline UX And Packaging
- Song Select's left navigation, search, sorting, key/chart filtering, paging, and mouse-wheel navigation are part of the current UX contract.
- Shared shortcuts such as `F5` reindex, `F1` help, and `F9` screenshot remain part of the baseline.
- The current `Skins` workflow for judge line, note size, lane spacing, and lane-color editing should be preserved.
- Distribution packages remain no-songs packages.
- New user profiles continue to be created automatically on first launch.

## Baseline Constraints
- Future changes should prefer additive work that does not break the `1.1.2 final stable` contract.
- Linux remains preview-level; judge the baseline against the Windows GUI path.
- Current code and `docs/current-state.md` take precedence over older design documents.
- If release/docs/packaging rules change, update the baseline and current-state docs together.

## What To Preserve In Follow-Up Work
- The playback-head-based gameplay input timing fix
- The split policy between RawInput-first live capture, polling shadow/fallback, and preserved saved backend preferences
- The BMS-first surface
- Large-library `safe` indexing stability
- Cache-first menu loading
- No-songs Windows packaging with bundled `Mainmusic/`
- The `external/llama.cpp/` exclusion rule for public source bundles

## Recommended Companion Docs
- Current implementation state: `docs/current-state.en.md`
- Config/profile structure: `docs/config.en.md`
- Practical play flow: `docs/gameplay-guide.en.md`
- Maintenance/extension map: `docs/developer-extension-guide.en.md`
- Long-term direction: `docs/roadmap.en.md`
