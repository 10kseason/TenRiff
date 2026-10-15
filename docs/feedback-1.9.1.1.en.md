# TenRiff 1.9.1.1 online BMS records

1.9.1.1 extends both the client and web conditions so that new BMS gauge, timing and RANDOM-branch records can be submitted to the **Sites community leaderboard**. Check the [release page](https://github.com/10kseason/TenRiff/releases/tag/1.9.1.1) for publication status, downloads and final verification results. Before deployment, the following describes the intended support boundary, not a claim that public-server verification has completed.

## Behavior

- Eligible BMS gauge v2, BPM/STOP timing v2 and control-flow v2 records, previously local-only in 1.9.1, can enter the web upload queue. **Both the 1.9.1.1 client and the corresponding web deployment are required.** The published 1.9.1 executable retains its upload gate and is not replaced or overwritten.
- Ranking conditions include the policy versions and the selected branch content. Different gauge/timing/control policies or branch content remain separate even for the same song. The original chart file identity is preserved.
- A branch is identified by the SHA-256 of active content after control-flow expansion. Different seeds selecting the same branch content share one condition. The seed is stored as per-score reproduction metadata, not as a ranking condition.
- Submissions without the optional version information retain historical behavior and existing board identities. The added information belongs to the web submission; existing replay files and the replay format do not change.

Use the existing connection under F10 → Web Leaderboard and play normally. Saving an eligible result triggers a submission attempt; the result screen and F10 show its status. This change does not add bulk resubmission of old local scores or restore failed queues automatically on the next launch.

## Boundaries

Sites records remain **`client_submitted` community records**, with authentication, shape, duplicate and supported-condition validation. This does not mean server-side C++ replay reexecution or anti-cheat verification. The separate Ranked path continues to reject the new BMS policies until its verifier backend supports them. Existing exclusions, including LN Mix v2, autoplay, practice, aborted/paused plays and courses, remain in force.

Master 10%, BMS Performance 20% and debounce 0ms defaults, saved settings, 1.9.1 gameplay/replay behavior, prior public assets and the fixed 1.9.0 baseline are preserved. Historical scores and gauges are not recalculated.

Verification covers the client submission contract, web acceptance/rejection, policy/branch ranking isolation, preservation of existing boards/records and post-deployment public readback. Real-account score submission, replay reexecution, physical devices and long play each require separate evidence. Final passes and unverified boundaries are recorded with the release results.

[Sites connection and submission guide](sites-leaderboard.md) · [1.9.1 changes and compatibility](feedback-1.9.1.en.md)
