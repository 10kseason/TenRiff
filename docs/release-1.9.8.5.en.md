# TenRiff 1.9.8.5 — BGA playback, input and song selection

2026-10-21

[한국어](release-1.9.8.5.ko.md) | [English](release-1.9.8.5.en.md) | [日本語](release-1.9.8.5.ja.md) | [简体中文](release-1.9.8.5.zh-CN.md)

Fixes BGA paths that flicker or show a scene too early, together with secondary-key input on the same lane. This release continues the user-selected published 1.9.8 and the preserved follow-up candidate. This document defines scope; publication and downloads follow the version-specific release page.

- Separate base/overlay playback state and image caches. Normalize MF PTS from the first decoded frame; handle sparse gaps, old PTS, restart and EOF while retaining the last completed frame during preparation. Correct the observed Save Yourself 533.3333ms offset and premature sparse EVE frames.
- Key exact black `RGB 0,0,0` in image overlays, including BMP. Preserve base-image black and authored alpha; external ONNX image processing retains the transparency mask. Video AI upscaling remains disabled.
- Wrap song navigation within the current filtered list. Rename Source to Library and remove `GAUGE SHIFT` from clear labels, using larger bold type, gauge colors and a subtle card tint.
- Show FAST/SLOW magnitude rounded outward to 0.1ms: ±22.020833ms displays ±22.1ms. Fixed Normal PG 22/19/15/10ms, Hard's complete Normal NM row and EX's complete Normal VHD row remain unchanged.
- Reject Esc for primary and secondary lane bindings. Pressing B while A is held on the same lane creates a new strike and transfers ownership to B. Releasing B does not revive held A; A must be released and pressed again. Pause/resume preserves this ownership.
- Classify formerly 7+1 SP charts without playable scratch as native 7K in new ordinary Original play. Actual scratch taps, LN, LNOBJ and mines retain eight lanes. Explicit eight-key exports declare `#8K`, preserving unused slots.

New native seven-lane and historical native eight-lane replays reconstruct their respective layouts. Explicit runtime key conversion and peer v7 retain legacy source mapping; sparse charts already native 7K without channels 18/19 retain their historical mapping too. Old eight-lane ghosts are not overlaid on new seven-lane play. No replay fields or ruleset IDs are added, but secondary takeover traces `Pressed → Pressed → Released` require the 1.9.8.5 verifier; published 1.9.8 may reject them. Only the derived song-index cache rebuilds from version 20 to 21.

Inherited evidence for the identical candidate implementation includes CTest 3/3 (1,399 cases), native 571/571, focused BGA pixel/PTS 102/102, hold/advance/rewind 18/18 and GPU composition 78/78. The opening 0–3 seconds of 358 videos covered 29,356 requests and 358/358 restarts, with no timestamp regressions or timeouts. UI evidence covers 14 captures, 102 regions and 20 color-pixel checks. These results do not certify the new 1.9.8.5 build, package, independent rebuild, CI or downloaded assets. The added 32 replay combinations use R9, not a separate run of those combinations under R10.

Full-length play, physical input/IME/listening, other PCs, real two-PC play, arbitrary external skins and the exact referenced out of sense video remain unverified. Preserve 1.9.8 X/Y and divider brightness, all 1.9.7 behavior, fixed general baseline 1.9.6, grades/timing/historical replay rules and Guest/client_submitted boundaries. No BGA weights ship; the existing 18 NK3 models and OFF/empty/Auto defaults remain. Website, database and public Skin Editor changes are outside this release.

[Behavior / compatibility](bga-input-feedback-20261020.en.md) · [Baseline 1.9.6](baseline-1.9.6.en.md) · [Normal / Hard / EX](judgement-baseline-20261018.en.md) · [Verification](release-1.9.8.5-gate.md) · [Release status](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5)
