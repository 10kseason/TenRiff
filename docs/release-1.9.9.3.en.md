# TenRiff 1.9.9.3

This release integrates the 2026-10-27 local follow-ups into published 1.9.9.1. The general fixed baseline remains **1.9.6**. The [release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.3) determines publication and downloads.

## Changes

- Fresh note-speed defaults are F1 decrease / F2 increase; saved bindings retain their meaning. Holding speed or visual-latency controls in the lobby/game accelerates adjustment. Release/focus transitions reset repeat state.
- Session Mix offers an LR2CRS course browser like Song Select. Choose a course and play available charts in authored order, including repeated stages; missing files block entry. Auto Mix reduces music-variant duplication, follows warm-up/challenge/cooldown heuristics, and reuses its preview at entry.
- The Skin Settings key-overlay plugin defaults OFF and saves enablement and X/Y. Only 4/5/7/8/10K show key/hold input and KPS. A bounded input queue feeds a separate worker; the HUD reads nonblocking snapshots. Whole-chart BMS analysis runs during loading and shows one large fixed representative difficulty. It is not live-input difficulty, skill or pass probability; latency/FPS gains are unmeasured.
- Song cards use EZ green, NM yellow, HD red and EX rainbow borders and upper-left jacket logo badges. Badges share EX typography; Shift Start uses the same colors. Imported alpha-logo shapes and legacy lobby badges receive the semantic colors.
- EX gameplay gauges remain rainbow even with a saved solid `gauge_ex_hard` override. Other explicit skin colors and original skin/logo files remain intact. Default TenRiff logos are larger.
- Grade colors: S+/S++/S+++ rainbow, S red, A+/A yellow, B/C/D green, E/F lavender `#C2A0FF`; AUTO/-- stay neutral.

## Compatibility and verification

Unrounded accuracy thresholds remain S+++≥99%, S++≥98%, S+≥97%, S≥95%, A+≥93%, A≥90%, B≥85%, C≥80%, D≥70%, surviving E/gauge-failed F. Fixed Normal PG22/19/15/10ms, Hard=complete Normal NM row, EX=complete Normal VHD row, R1–R10, peer v7, LN Mix v1/v2/v3 and NK2 E remain unchanged. Preserve existing online boundaries, Guest/custom exclusions and client_submitted. No website, DB or public skin-editor deployment.

Prior local evidence is separate from fresh versioned checks. Follow the [release gates](release-1.9.9.3-gate.md). Physical input, OBS, long play, other PCs, arbitrary skins, actual two-PC play and real-account uploads remain unverified. Extract into a new directory and run `TenRiff.exe`; preserve existing installations/profiles.

[Controls](controls-20261027.en.md) · [Session Mix](session-mix-20261027.en.md) · [Overlay](key-overlay-20261027.en.md) · [BMS/KPS](bms-overlay-20261027.en.md) · [Clear cards](clear-cards-20261027.en.md) · [Final color policy](rank-colors-20261027.en.md)
