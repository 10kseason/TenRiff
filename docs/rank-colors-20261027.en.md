> **1.9.9.3:** Includes accelerated controls, LR2CRS courses, BMS/KPS overlay, clear cards, grade colors and EX rainbow fixes. General fixed baseline: **1.9.6**. Older release and local-candidate entries below are historical; this scope supersedes them. [Release](release-1.9.9.3.en.md).

# Rank/shift colors and EX rainbow correction

Local 1.9.9.1 follow-up including prior local changes. Grade thresholds, judgement, gauge math, score and replay remain unchanged.

- S+/S++/S+++ use EX rainbow; S uses HD #FF3838; A+/A use NM #FFF000; B/C/D use EZ #00ED57; E/F use lavender #C2A0FF. AUTO/-- retain neutral #B6C0D0. Historical top-grade aliases retain the same top-grade family.
- Song selection, record and result grades share the palette. EX's seven colors span actual glyph bounds using the existing cached glyph/bloom rendering, without rebuilding per frame.
- All four clear badges retain EX's shared font, weight, size and logo geometry; authored font settings remain. Shift Start values use matching clear colors, including rainbow EX-HARD.
- EX gameplay gauges now stay rainbow even if a skin saved a solid EX color, in Classic/Studio and ghost views. This supersedes the previous explicit-EX-color exception. Other authored gauge colors remain.
- Legacy lobby rendering also displays clear badges. Imported logo alpha/shape is preserved and painted with clear colors inside badges. Other uses of that logo retain their original pixels. Original images and saved skins are not rewritten.

Native visual checks cover default/solid-EX skins, imported alpha marks and legacy lobby. Arbitrary user skins, physical input and long play remain unverified. Prior packages/candidates and fixed baseline1.9.6 are preserved. Local client UI scope.
