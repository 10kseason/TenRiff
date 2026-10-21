# BGA, input and song-selection follow-up (2026-10-20)

[한국어](bga-input-feedback-20261020.ko.md) · [日本語](bga-input-feedback-20261020.ja.md) · [简体中文](bga-input-feedback-20261020.zh-CN.md)

This note defines behavior and compatibility for the user-requested 1.9.8.5 release based on published 1.9.8. The fixed general baseline remains 1.9.6; prior releases, installations, profiles, original charts and the local candidate are preserved. See the [1.9.8.5 release scope](release-1.9.8.5.en.md) for publication and downloads. New release verification is recorded separately.

## Requested behavior

| Request | Change |
| --- | --- |
| 1 | Previous from the first song wraps to the last; next from the last wraps to the first. Navigation uses the currently filtered visible list. |
| 2 | Exact black `RGB 0,0,0` becomes transparent in BGA image overlays. Base-image black and authored alpha remain intact. The optional external ONNX image path preserves the overlay transparency mask. |
| 3–5 | Investigate local Save Yourself and video BGA material. Separate base and overlay image caches; correct requested-clock tracking, cached frames, EOF and stale asynchronous results. A decoded frame ahead of the next request does not itself cause a rewind. |
| 6 | FAST/SLOW shows one decimal place. Magnitude rounds outward to 0.1ms: 22.0208ms displays `22.1 ms`, never `22.0 ms`. This changes presentation only. |
| 7 | Esc cannot be assigned to primary or secondary lane bindings. Its cancel/pause role remains. |
| 8 | Inferred SP or explicitly declared 7K without playable scratch objects becomes native 7K in new ordinary Original play and the library. Zero-only 16/56 rows do not count. Scratch taps, LN, LNOBJ and mines retain 7+1. |
| 9 | Song-selection “Source” wording becomes “Library”. Internal storage names and paths are unchanged. |
| 10 | Holding A and freshly pressing B on the same lane produces a new strike and transfers ownership to B. Releasing B releases the lane; held A does not reactivate until released and pressed again. Transfer does not inject a false release that would break an LN. |
| 11 | Hide the `GAUGE SHIFT` prefix in lobby clear labels; enlarge and embolden the text, use the gauge tier color and apply a subtle clear tint. Stored statuses and ASSIST distinctions remain. |

## Timing and compatibility

- Fixed Normal PG remains VEZ/EZ 22ms, NM 19ms, HD 15ms and VHD 10ms. Hard reuses the complete Normal NM row; EX reuses the complete Normal VHD row. Windows are quantized to audio samples. Timing, score, gauge and current R9/R10 or historical R1–R8 rules are unchanged.
- Charts formerly classified as 7+1 through channels 18/19 preserve the seven positions `11/12/13/14/15/18/19` when becoming native 7K. Sparse charts already classified as 7K without 18/19 keep their historical compact mapping because old seven-lane replay metadata cannot distinguish a remap. Explicit 8K and other authored layouts, PMS and DP retain their classifications. Eight-key conversion exports declare `#8K` to retain all eight slots even when some are unused.
- New native 7-lane replays reconstruct seven lanes. Historical native 8-lane replays reconstruct the old empty scratch slot. Explicit runtime key conversion and peer v7 retain the legacy source mapping. An old 8-lane ghost is not overlaid on a new 7-lane play.
- No replay fields, format or ruleset IDs are added. Secondary-key takeover can record `Pressed → Pressed → Released` using existing events. These traces require the updated verifier; the published 1.9.8 verifier may reject consecutive Pressed events.
- Pause/resume preserves input ownership. An owner released while paused is released on resume without reviving an older held key. Keys first pressed during pause require a release and fresh gameplay press; resynchronization itself creates no fresh strike judgement.
- Derived song-index cache version changes from 20 to 21 and rebuilds once. Original charts, records and replays are not migrated. Guest exclusion, `client_submitted`, fixed grades, no bundled BGA weights and OFF/empty defaults remain.

## Investigation and verification limits

The local inventory contains 358 videos, 1,142 charts referencing visual material and 135 charts with undefined BGA IDs. Inventory coverage is not evidence that all charts play without flicker. The U_E material includes the Save Yourself video. The exact referenced out of sense video was not located in the current search; reproducing that chart's reported flicker remains unverified.

The first frame decoded by Media Foundation establishes the presentation timestamp (PTS) origin; later frames are compared using elapsed time from that origin. This corrects the observed 533.3333ms start offset in Save Yourself. Initial requests within three seconds decode sequentially from the beginning; later initial requests seek with a one-second preroll. If seeking overshoots the target, the reader reopens the video and resumes sequential decoding.

When a frame gap exceeds two 30Hz request periods and the next frame leads by more than one period, the reader holds that one future frame until its PTS. Forward playback discards frames whose PTS precedes the last retained frame. Ordinary 24/25/30/60fps behavior remains. This fixes the EVE AVI frame at 1.6 seconds appearing about 1.3 seconds early after its frame at zero. ffprobe confirms F13 and CLOSING each contain one encoded frame, so retaining their PTS-zero frame at EOF is correct.

Inherited evidence from the identical candidate implementation: focused pixel/PTS checks passed 102/102 cases (62 initial positions, 20 sparse-frame gaps and 20 timestamp regressions), with 18/18 hold/advance/rewind checks. All 358 videos passed the opening 0–3-second sweep with 29,356 requests and 358/358 restart checks; timestamp regressions and timeouts were zero. This does not verify full-length playback. All 1,399 unit cases and 571 native checks also passed. The added replay regression's 32 source-layout, key-mode and random combinations use R9; this does not establish a separate run under R10. The 135 charts with unresolved references include conditional branches and do not represent 135 confirmed playback failures; unresolved-reference skipping remains unchanged.

Black-key layering references the [BMS command memo LAYER section](https://hitkey.nekokan.dyndns.info/cmds.htm#LAYER). The primary [beatoraja BGImageProcessor source](https://github.com/exch-bms2/beatoraja/blob/master/src/bms/player/beatoraja/play/bga/BGImageProcessor.java) is an image-texture caching reference, not proof of TenRiff behavior.

Record 1.9.8.5 build, package, independent rebuild, CI and download verification separately under the [release gates](release-1.9.8.5-gate.md); inherited candidate results do not replace them. Preserve failures and distinguish automated checks from physical input/listening, long play, other PCs, actual two-PC play and arbitrary external skins. Website, database and public Skin Editor deployment are outside scope.
