# TenRiff Ranked Integrity and Online Records Plan

Status: **policy planned; no public ranking server or submission endpoint exists yet**.

The server must never trust client-submitted score totals or judgement counts. A score becomes
publicly ranked only after the server resolves an approved BMS chart SHA-256 and deterministically
replays evidence v3 with the matching versioned TenRiff rules engine.

## Fixed policy

- Ranked charts are approved `.bms/.bme/.bml/.pms` hashes only.
- `.osu` charts, osu import/parser routes, osu!mania rules, ScoreV1/OD8 as the submitted authority,
  and future osu-derived conversions are rejected as `ranked_ineligible_osu`.
- Computing auxiliary OD8 statistics during a native BMS play does not itself invalidate the play;
  the authoritative submitted ruleset and score must be TenRiff native BMS.
- Purely visual skins and backgrounds do not affect eligibility.
- Autoplay, practice/no-fail, replay playback, aborted plays, and rules/mods/rates outside the
  versioned allowlist remain local with explicit ineligibility reasons.

## Stages

1. Define one shared `RankedEligibility` contract and stable reason codes; test the complete matrix,
   golden replays, tampered evidence, and legacy migrations before opening any upload endpoint.
2. Add a dedicated Records screen with Local and Online tabs. Local records expose verified,
   unverified, and ineligible states plus replay details; Online remains disabled initially.
3. Deploy a read-only TLS/API-versioned leaderboard for approved chart hashes. Server failure must
   disable online features without blocking local play.
4. Run a non-public shadow submission period. Submit chart/ruleset/replay hashes, mods/rate, and an
   idempotency key; the server replays the trace, rate-limits it, detects duplicates, and returns a
   stable rejection reason on mismatch.
5. Publish only `online_verified` results after authentication, recovery, retention, and ruleset
   versioning are defined. Client signing or attestation is only a supporting signal; deterministic
   server replay remains the trust boundary.
6. Keep append-only verification/status audit logs, reversible moderation history, and a user-visible
   appeal/reverification path. Scores the server cannot reproduce never become verified by override.

The detailed Korean plan in [`ranked-integrity-plan.md`](ranked-integrity-plan.md) is authoritative
for the current implementation sequence.

