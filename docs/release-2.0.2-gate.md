# TenRiff 2.0.2 verification gate

Base: 2.0.1; compatibility reference: final4. This release includes the explicitly requested global chat alongside multiplayer stability and presentation fixes.

- Selected solo skin/HUD retained in multiplayer; auxiliary opponent cards use available side space.
- Hash completion requires verified local chart and current server acknowledgement.
- Loading/results/reset polling changes from 500ms to 250ms; persistent HTTPS transport. The five-second server countdown is preserved. No measured production latency improvement is claimed.
- Final results cannot regress behind the latest accepted local score sample; server rejection fails promptly.
- One authenticated global channel across all screens, separate room/chat sessions, bounded UTF-8 text/history, idempotent retry and account-switch fencing. Old room APIs remain compatible.
- Local candidate: CTest 4/4 (1605 cases, zero integration skips), native 774/774, server SQLite 51/51, actual Worker and two C++ clients including session renewal, final results/reset and logout. Eight chat renderer conditions and twenty inherited skin conditions were inspected. These use synthetic fixtures.
- Release checks: serial MSVC Release build, exact-binary checks, independent source-ZIP build, outgoing-file review, exact Git/archive comparison, PR/main CI and public download/hash/CRC verification. Completion is recorded in the release evidence.
- The separately deployed multiplayer server includes additive migration 0002 for four chat tables. Existing migrations and room data are preserved. The leaderboard and skin-editor deployments are outside this release.

Initial candidate failures and resolutions: the first chat session design could revoke room sessions, so authentication and storage use a dedicated endpoint; a JS fixture default was corrected to test the room route; a concurrent executable asset-copy failure was resolved with serial builds. Localhost integration must run with network access; sandbox socket failures are not product failures.

Real-account messages, internet latency/cost/load, physical keyboard/IME timing, listening, arbitrary skin layouts, other PCs and long play remain unverified. The optional 10k-calc Python reference comparison was unavailable. Judgements, score calculations, replay formats and configuration defaults are unchanged.
