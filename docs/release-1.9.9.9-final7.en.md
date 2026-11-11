# TenRiff 1.9.9.9-final7

[한국어](release-1.9.9.9-final7.ko.md) | [English](release-1.9.9.9-final7.en.md) | [日本語](release-1.9.9.9-final7.ja.md) | [简体中文](release-1.9.9.9-final7.zh-CN.md)

Fixes a missed note blanking the judgement shown on screen. A miss now shows its BAD/POOR when it is decided. Scores, gauge, judgement windows and replays are unchanged.

- An untouched note is decided as a miss 340 ms after its time. That miss used to be recorded in the display slot with the note time, so it was already expired. Its BAD/POOR never appeared, and the judgement just shown for another lane disappeared with it.
- A miss is now shown from the moment it is decided. A miss retired by a late key press is shown from that press.
- Only the display time changed. Judgement counts, combo, gauge, result graphs, rulesets, the replay format and verification are the same as before.

Verified by automated checks of the engine and HUD path. The live play screen, physical input, listening and extended play need further verification.

[Release verification](release-1.9.9.9-final7-gate.md)
