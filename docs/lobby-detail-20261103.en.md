> Historical local-candidate design/evidence. The included release behavior and launch instructions are in [1.9.9.9](release-1.9.9.9.en.md); local-only/profile notices below describe that earlier candidate. The final Result layout is the restored layout.

# Song Select clear badges, chart details and motion background

Local follow-up, 2026-11-03. Continue the result-rollback candidate and preserve its restored Result layout.

Replace the list's numeric scores and unplayed dashes with existing EZ/NM/HD/EX CLEAR badges. Reuse their mark, typography and semantic colors, including EX prism. Do not promote failed/assisted records to clears. Preserve folder chart counts and the selected MY BEST score.

Arrange chart details as BPM / keys / level / note count / judgement difficulty, followed by an NPS panel containing MIN / AVG / MAX. Read real note heads from the selected source chart; AVG is an actual mean, not renamed MED. Unavailable or pending data stays unavailable. Hide detailed accuracy only in lobby presentation, keeping detailed score, ordinary accuracy, Result and stored values.

NPS covers one-second bins from chart time zero through the last playable head's bin, including silent bins. Count TAP/LN heads once; exclude tails/mines. MIN/arithmetic AVG/MAX use all original bins; only the graph is reduced to at most 96 chronological peak bars. Preview uses rate 1.0 and a deterministic branch, which may differ from a random play. Missing, pending, over-8MB or six-hour-plus charts stay unavailable. Existing MED caches and record formats remain intact.

Use the previously generated silent 12-second 1080p30 video as the default native Song Select background. Reduced motion freezes the image; authored backgrounds retain priority. Release its dedicated decoder on leaving Song Select. Gameplay audio, timing, scoring, replays and storage formats remain unchanged.

Separate `lobby-detail-local` profile. Preserve previous candidates/installations/profiles; no publication or promotion of fixed baseline 1.9.6. See the accompanying verification report for exact density definitions and runtime limitations.
