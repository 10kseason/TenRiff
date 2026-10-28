# Local indicator and result feedback

This isolated candidate continues released 1.9.9.5; the fixed 1.9.6 baseline and public version remain unchanged.

- Historical and latest barcode markers now share 2×18 base dimensions. Opacity identifies the latest hit. Dots, colors, 0.7-second lifetime, placement/scaling and player/ghost timing remain unchanged.
- Result summary/analysis displays `EX CLEAR` instead of `GAUGE SHIFT EX CLEAR`. Other gauges lose only the same prefix; ASSIST stays visible. Stored status and gauge colors remain intact.
- `NEW Record` appears above the score for a first eligible local BEST or a meaningful improvement under the existing ordering: clear strength, score, detail score/accuracy, combo and judged count. A timestamp-only tie does not qualify. A newer judgement policy starts a fresh BEST as before.
- Existing exclusions for autoplay, practice, added notes and current LN Mix remain. Replay playback, historical results and failed exports cannot show a new record. Multiplayer labels only the local player's record and sends no new peer data.

The session-only flag preserves file formats, scoring, judgement and replay contracts. Reusing the existing score-label slot preserves skin layout. Use the separate result-feedback-local profile. Physical input, real two-PC play, long sessions and arbitrary skins are unverified.
