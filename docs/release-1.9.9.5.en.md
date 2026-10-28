# TenRiff 1.9.9.5

[한국어](release-1.9.9.5.ko.md) · [日本語](release-1.9.9.5.ja.md) · [简体中文](release-1.9.9.5.zh-CN.md)

This release integrates skin, presentation, BGA and render-wait changes after 1.9.9.3. The general fixed baseline remains **1.9.6**. Publication and downloads follow the [release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.5); fresh versioned verification follows the [release gates](release-1.9.9.5-gate.md).

## Changes

- Native simple shapes other than the original `rect` use solid lane colors for LN heads, bodies and tails. Ordinary taps, imported images/authored sprites, outlines, width, alpha and taper remain. Brighter overlaps between translucent caps and bodies retain existing alpha compositing.
- Completing the whole chart with an unbroken combo, including the last LN tail, displays **ALL COMBO** artwork with a rounded border and subtle yellow glow. Player and ghost qualify independently; fresh completed results also display it. Failed, aborted, automated, Auto Scratch, No Fail and partial-practice runs are excluded. Historical results do not infer the new badge.
- Media Foundation BGA catches up after a large time jump using bounded seeking and sequential decoding. Forward recovery does not regress already presented PTS. Explicit rewind, normal cadence, sparse frames, EOF and last-valid-image retention remain. Long GOPs may require multiple requests.
- If a stale oversleep estimate alone repeatedly prevents render timer sleeps and no new sleep sample exists, reduce that estimate by 7/8 at intervals of at least 100ms. FPS targets, VSync, unlimited mode and normal sleep measurements remain unchanged.
- EX uses a static cyan → blue → lavender → pink → gold → mint → cyan prism palette, superseding the gold-only and no-purple concepts. S+, S++ and S+++ are text with that gradient, glow and faceted light effects for depth. Effects use the existing cached glyph bitmap pipeline, without new per-frame blur or hue animation. Rank PNGs are neither shipped nor loaded.
- Remove the difficulty estimate from the key overlay while retaining KPS and key/note viewing. Default OFF, supported modes and saved positions remain. Chart and song-library difficulty ratings are unchanged.

EX stops at `0/0.18/0.36/0.54/0.70/0.85/1` are `#4AF3F7/#6391FC/#C75FFB/#FC94DE/#F4E289/#7CF69F/#4AF3F7`. Interpolation is static sRGB RGB; the base/fallback is `#4AF3F7`. EZ/NM/HD, other rank colors and E/F lavender `#C2A0FF` remain unchanged.

## Compatibility and limits

Judgement, input, audio, scoring, replay and grade thresholds are unchanged. Unrounded accuracy remains S+++≥99%, S++≥98%, S+≥97%, S≥95%, A+≥93%, A≥90%, B≥85%, C≥80%, D≥70%, surviving E/gauge-failed F. Preserve Normal PG22/19/15/10ms, Hard=the complete Normal NM row, EX=the complete Normal VHD row, R1–R10, peer v7, LN Mix v1/v2/v3, NK2 E and existing online exclusions.

No measured Discord, FPS or latency improvement is claimed. There is no website, DB or public skin-editor deployment. Prior candidate checks do not replace fresh 1.9.9.5 verification. Physical input/listening, end-to-end OBS/Discord behavior, long sessions, other PCs, arbitrary skins, actual two-PC play and real-account uploads are not guaranteed beyond directly tested scope. Extract into a new directory and preserve existing installations, profiles and charts.
