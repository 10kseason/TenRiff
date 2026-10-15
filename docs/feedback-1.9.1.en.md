# TenRiff 1.9.1 changes

**2026-10-15 · 1.9.1:** The user-authorized 1.9.1 includes the earlier volume/input/outro fixes, BMS TOTAL/RANDOM compatibility, Result colors and the 10 mark. A Home language button cycles EN → KO → JA → ZH and saves the choice. New BMS gauge/branch records remain local until server verification supports them. The fixed baseline stays 1.9.0; earlier releases, installations and profiles are preserved. Dated local notes below retain their historical no-version-bump/unpublished status; this 1.9.1 preparation supersedes that status.

## Changes

- The Home language button cycles **EN → KO → JA → ZH → EN**. It saves through the existing `ui.language` setting and persists across restarts. Strings without a translation can fall back to English.
- Audio exposes only Master, BMS Performance and TenRiff Music volumes. New defaults are 10%, 20% and 100%, adjusted in 1 percentage-point steps. Saved whole-BMS volume is retained; legacy background/keysound fields no longer multiply playback. Lobby preview keeps ×0.75 attenuation.
- Debounce applies in the actual input filter, with 1ms steps over 0–20ms and a 0ms default. Retire the old automatic 5→8ms rewrite and preserve saved values. The Korean skin label becomes `LN 끝 줄임`.
- Show loading status while importing or refreshing difficulty tables. In ordinary single-player with Listen to End enabled, Esc skips the outro without Pause after all notes and LN tails have resolved. Course and multiplayer controls stay unchanged.
- New ordinary BMS uses LR2-derived recovery/damage with TOTAL and note count. Preserve TenRiff 100% start, zero failure, gauge shifts, judgement classification, LN weights and mine damage. Authored RANDOM branches use an independent seed and control-flow v2, while historical replays retain their original policies.
- Result levels share the lobby’s red level text and registered key-mode symbol colors. Show green ⑤ only for Aery5, never for plain native Lv. Replace the native menu mark with the supplied 10 motif.

[Technical BMS formulas and compatibility](bms-total-random-followup.en.md).

## Publication and verification

See the release page and verification record below for final validation results and downloads. Physical Xbox/input/audio latency, long play, other PCs and full equivalence to the original LR2 executable remain unverified.

[Release verification gate](release-1.9.1-gate.md) · [1.9.1](https://github.com/10kseason/TenRiff/releases/tag/1.9.1).
