# TenRiff 1.9.5 — Scene audio routing and timing indicators

Menu/result music follows the selected sound output. Indicator ranges and markers use blue FAST / green maximum judgement / red LATE. FAST/SLOW updates on maximum hits too. Lobby order is OFF → Mirror → Random → S-Random → Random (Scratch Fixed) → OFF.

- Replace scene MCI playback with the existing AudioThread/decoder so menu/result music uses the selected WASAPI endpoint or ASIO driver. Menu WASAPI uses shared mode on that endpoint; gameplay retains its exclusive-mode preference. Missing selected devices never silently redirect. Scene rotation, gain and focus muting remain.
- Decode asynchronously outside menu/audio callbacks and poll idle menus. Release the scene device before previews, gameplay or editor playback. Reject old editor decode completion after practice/results. The short settings-adjustment click retains its Windows-default path.
- Blue left, green center and red right ranges represent FAST, the actual sample-quantized maximum window, and LATE. Player/ghost use their own effective judge policies. Markers within the maximum window or awarded maximum stay green, including the newest marker. Conditional indicators show maximum hits too; independent 0.7-second marker expiry and always-visible ranges remain.
- FAST/SLOW refreshes on maximum hits; rounding to zero milliseconds clears stale text. Conditional text lasts 0.75 seconds and Always retains the latest value. No scoring, timing-window or replay calculation changes.
- Lobby order: OFF → Mirror → Random → S-Random → Random (Scratch Fixed) → OFF, reversed for backward cycling. `rr` is renamed only for display; algorithms, saved tokens, seeds and replay meaning are unchanged. Korean disabled-value labels use OFF.
- Preserve 1.9.4 cache retention/menu reclamation, note-speed multipliers, key remapping and measure alignment; fixed baseline 1.9.0, prior artifacts/profiles, Guest/grades/R7–R8/legacy replays, no bundled BGA weights and OFF/empty/Auto defaults remain.

[Release](https://github.com/10kseason/TenRiff/releases/tag/1.9.5) · [Verification scope](release-1.9.5-gate.md)
