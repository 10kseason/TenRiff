> **Historical document — local candidate before 1.9.9.5:** Behavior, unpublished status and check counts below describe that candidate only. Candidate evidence is not fresh release evidence. Current scope/colors follow [1.9.9.5](release-1.9.9.5.en.md); fresh verification follows the [public release gates](release-1.9.9.5-gate.md).

# Prism rank text and EX palette (local candidate)

[한국어](rank-prism-20261028.ko.md) · [日本語](rank-prism-20261028.ja.md) · [简体中文](rank-prism-20261028.zh-CN.md)

This local candidate continues 1.9.9.3 with prism effects on S+, S++ and S+++ text. Release build, CTest 3/3 (1,445 cases), 26 final render fixtures and 37 editor checks passed. The published version and fixed baseline 1.9.6 remain unchanged.

- S+, S++ and S+++ text uses the EX prism gradient, glow and faceted light effects for depth. It uses the existing cached glyph bitmap pipeline instead of loading PNGs. Effects are cached when the text, font, size or device changes, so no blur or hue animation needs to run every frame. Cache creation failure falls back to existing text rendering.
- Generated rank images remain preserved concept outputs outside the source tree; the game neither loads nor ships them. Other ranks and EZ/NM/HD/E/F colors remain unchanged.
- EX uses the selected artwork's cyan → blue → lavender → pink → gold → mint → cyan palette with static sRGB RGB interpolation. This selection supersedes both the former EX gold-only appearance and the earlier no-purple instruction.
- Only the difficulty estimate display is removed from the key overlay. KPS and the key/note viewer remain, as do chart and song-library difficulty ratings.

| Position | 0 | 0.18 | 0.36 | 0.54 | 0.70 | 0.85 | 1 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| EX color | `#4AF3F7` | `#6391FC` | `#C75FFB` | `#FC94DE` | `#F4E289` | `#7CF69F` | `#4AF3F7` |

The EX base/fallback color is `#4AF3F7`.

The candidate retains [render-wait recovery](render-wait-20261028.en.md), [simple LNs and all-combo artwork](skin-finish-20261028.en.md), and the [screen-sharing/Discord investigation and BGA recovery](share-sync-20261028.en.md). The scope above supersedes older descriptions of EX colors and the overlay difficulty estimate. This change does not alter judgement, input, audio, replay, grade thresholds or inherited render-wait behavior. No measured Discord or performance improvement is claimed.

Pass results mentioned above belong to historical candidate verification. Fresh versioned coverage follows the [1.9.9.5 release gates](release-1.9.9.5-gate.md). Physical input, long sessions, other PCs and arbitrary skins remain distinct from synthetic checks.
