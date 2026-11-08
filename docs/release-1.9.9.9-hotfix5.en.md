# TenRiff 1.9.9.9-hotfix5

This release includes all hotfix4 hit-motion, burst customization, multicore scan and cache-loading changes. The runtime fix only corrects the native/conversion key-mode marker sent to the web leaderboard.

New Online plays in native key mode (`auto`/`none`/`native`) send `key_conversion: none` regardless of the remembered KRRCREAM/NK2/NK3 setting. Explicit target-key modes retain a conversion marker even when the output key count matches. Missing or unknown modes are not inferred to be native.

Production website v25 counts only native-key plays in song lists, rankings, personal bests and board/player/play totals. Previous clients also sent converter names for native mode, so existing server metadata cannot prove whether conversion occurred. The user selected exclusion of these ambiguous records from rankings. Stored data remains intact, with no relabelling, deletion or resubmission under a new identity. Previous clients may continue to send excluded markers; use hotfix5 for newly ranked native plays.

Extract the runnable ZIP into a new folder and run `launch_win.bat`. Select original/None as the converter target key mode, configure Online/web connection through F10, then start a new play. Preserve existing installations and profiles. Local scores and replays for converter plays remain available.

Scoring, judgement, replay format and R13/R14 policies remain unchanged. Fixed baseline: 1.9.6.

[Validation](release-1.9.9.9-hotfix5-gate.md)
