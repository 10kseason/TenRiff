# TenRiff 1.9.5.3 — Skin settings cleanup and search/input fixes

- Skin Settings adds **In-field Judgement Counts** (default Off): cumulative PG / GR / G / BAD / PR counts appear immediately below the timing indicator, follow its position and fit inside the note field. Counts stay visible between hits and use separate player/ghost totals. Judgement names and calculations remain unchanged.

- Select the edit lane with numbered preview buttons. UI font, font file and Gameplay Cursor live in Profile. Scratch Position appears only in Original runtime key mode and is adjustable for a 7+1 layout.

- Fresh defaults: note/field 60%, maximum key beam height 50%, 5K white/blue/white/blue/white, simple rectangle. F1 speed +0.25 / F2 -0.25; F3 judgement line up 1% / F4 down 1%. F5/F6 x0.5/x2 remain.

- Keep fresh defaults of opaque playfield ON, LN tail OFF, LN body width/opacity 100%, lane spacing 0%, key labels bottom; hide these controls and LN tail taper from Skin Settings. Explicit saved values remain intact.

- Also remove target-lane, single-color, note-border, overall/outline alpha, lane-background density, 16K center-gap and divider-width rows. New 16K center gap is 100%; effective divider width is fixed at 100%. Judgement line accepts 1–100%.

- Automatic image fit uses the configured destination note width/height: contain for 1:1, stretch otherwise. Fix F6 fullscreen skin preview and portable .trskin preset file saving.

- Holding Backspace repeatedly deletes complete UTF-8 characters and never navigates back. Free-text search uses title/artist only. Lv:16 and Lv:16+ each match only that exact level.

- Show every declared difficulty-table level folder in numeric order, including empty levels; entering a folder shows only its level. Only Enter skips the post-note/LN outro.

- Reuse serialized settings/preset formats and input action slots to preserve saved values. Judgement, score, gauge, audio and replay rules are unchanged. Retain fixed baseline 1.9.0, Guest exclusion, fixed grades, client_submitted and no bundled BGA weights.

[Release and downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.3) · [Verification scope](release-1.9.5.3-gate.md).

Physical long play, other PCs and arbitrary imported skins remain unverified. Browser preview is illustrative; use in-game preview for actual rendering.
