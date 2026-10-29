> **Historical local candidate; superseded by [1.9.9.7](release-1.9.9.7.en.md).** Its local-only online restriction no longer describes the released client.

# Standard LN early-release fix

Standard holds previously accepted release up to the GOOD window (about115ms). Early release now succeeds only within the current PG window: Normal10/15/19/22ms by BMS RANK, Hard19ms, EX10ms. The exact boundary succeeds; one sample earlier immediately commits BAD, combo break and gauge loss. Holding through the tail still completes automatically, and regrabbing never refunds a drop.

Authored charge-note release timing keeps its PG/GR/GD windows. No LN Release converts those notes to standard holds and uses the new policy. Course standard-LN completion uses the same PG tolerance. Failed holds record an OD8 Miss so all-combo and Sudden Death follow the failure. Tap/head windows, audio, input offsets and display timing stay unchanged.

New runs use R11/R12 or custom11/12, without adding replay JSON fields. R1–R10 and historical custom replays/ghosts retain their original policies. BEST distinguishes the new generation without rewriting history. Peer v8 preserves the packet layout and rejects v7. The deployed Sites R9/R10 contract stays unchanged; new R11/R12 submissions remain local. Existing Ranked server-support exclusions remain. No server or website deployment.

This is an unpublished local follow-up to1.9.9.6. Existing releases, installations, profiles and the pinned1.9.6 baseline are preserved. Physical keyboard feel and long sessions remain unverified.
