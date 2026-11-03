> Historical candidate verification. See [1.9.9.9 validation](release-1.9.9.9-gate.md) for the combined release gate.

# Local UI fixes verification gate · 2026-11-03

- Release client, replay verifier and verification tools built successfully.
- Final CTest: 3/3, 1,493 unit cases, 0 integration skips. Optional external 10k-calc Python reference unavailable.
- Native menu: 637/637 checks, synthetic separate profile, no account/Sites connection.
- Native audio picker: 6/6 real modal Show cases, MP3/OGG/WAV/FLAC Unicode/space path selections and cancellation/reentry from MTA.
- UI: 15 final cases / 161 hit regions. Long title and detail metrics fit in 11 result cases, including Consolas, large/extra-large UI, CJK and small window. Initial foreground-limited Song Select was retried with the same final binary and passed native dispatch.
- Live BEST: paused local BEST selection, pause/resume logical clock, independent score progression and rank/cache changes passed. Local pause acceptance does not alter online submission policy.
- Offline editor: 37/37. Existing catalog values, original 15 music/SFX assets, 11 protected gameplay/record/replay/audio owners and config defaults are unchanged.

The first full unit run exposed one new fixture state-restoration omission after an incompatible-BEST assertion. The fixture now restores compatibility before its independent pause case; all assertions remain, and the final complete run passed. Initial build tooling/foreground failures are retained in the outer evidence folder. Screenshots use synthetic data. Physical input/listening, long sessions, arbitrary fonts/skins and other PCs remain unverified. No upload, publication, baseline promotion, installation or profile overwrite.
