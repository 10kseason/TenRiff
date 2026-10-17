> **1.9.5:** [Scene audio routing and timing indicators](release-1.9.5.en.md). Menu/result music follows the selected sound output. Indicator ranges and markers use blue FAST / green maximum judgement / red LATE. FAST/SLOW updates on maximum hits too. Lobby order is OFF → Mirror → Random → S-Random → Random (Scratch Fixed) → OFF.
> Earlier 1.9.4 behavior below is historical where superseded by this update.

> **1.9.4:** [Memory reclamation and play controls](release-1.9.4.en.md). Retain chart PCM and static BGA during play, defer forced trims, and add indicator shape/0.7-second markers, output selection, profile shortcuts, ×0.5/×2 and aligned measure lines.

# Indicators, output devices and profile shortcuts

Follow-up dated 2026-10-17, included in 1.9.4, to user-selected 1.9.3 (`bf544e17f22879011a04a7fd157e40b57f6598bb`). Fixed baseline 1.9.0, published assets and installations remain unchanged.

- BMS measure lines attach to the lower edge of notes instead of their center, using configured/imported note height in both player and ghost fields. Measure timestamps, STOP/BPM and judgement timing stay unchanged.

- Skin options add Indicator Shape: Dot (default) or Barcode (straight vertical strokes). Always keeps the track/center visible from the start; each judgement marker expires independently after 0.7 seconds. Retained FAST/SLOW text, judgement, scoring and replay semantics stay unchanged. Player, ghost and previews use the same style. Pausing freezes marker time with the game clock.
- Sound Output Device lists active WASAPI outputs plus System Default. Refresh with the configured refresh key (default F5); Back saves and applies. ASIO keeps its separate driver selector. Missing endpoints retain their saved identity and never silently fall back to another output; reconnect the device or select System Default. Output selection applies to gameplay and BMS previews. Built-in TenRiff menu/result music keeps its separate MCI path on the Windows default output.
- Profile settings reassign or clear (Delete) the former F1–F11 actions. A binding follows all contexts of that function key, including help/judge-line, refresh/half-speed and preview/double-speed. Help/footer labels show the current key. Conflicts with other actions, restart, lanes and reserved menu controls are rejected.
- The previous -10/+10 actions (default F5/F6) now multiply visual note speed by 0.5/2, once per press, clamped to 0.5–50. Holding a key or duplicate press events cannot compound the multiplier. Play, countdown and pause support them. Song Rate, audio, judgement timing and F3/F4 ±0.25 are unchanged.

Configuration: `skin.timing_marker_style=dot|barcode`, `audio.device_id` (empty=OS default), `ui.function_keys` object. F5/F6 also round-trip the legacy `hispeed_decrease_key`/`hispeed_increase_key`. Portable profile exports include shortcuts but exclude endpoint IDs; import preserves the current machine's selected endpoint. Skin presets preserve marker shape.

See the release's `evidence/VERIFICATION.ko.md` for results. This document does not establish audible hardware, other-PC or long-session validation or publication.
