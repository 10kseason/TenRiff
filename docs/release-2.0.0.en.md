# TenRiff 2.0.0

[한국어](release-2.0.0.ko.md) | [English](release-2.0.0.en.md) | [日本語](release-2.0.0.ja.md) | [简体中文](release-2.0.0.zh-CN.md)

Integrates EX Score (2×PG + GR), account-linked hosted multiplayer, constellation Home/Song Select/Options, verified ALL COMBO presentation, and room-chat, Library and Session Mix fixes.

- **Scores:** EX Score is 2×P-Great + Great; Good, Bad and Poor award zero. Display, personal bests, ghosts, Session Mix, multiplayer and web ranks share this measure without mode/Rate score multipliers. Accuracy, grades, gauge and judgement rules remain separate. Historical replay-verification scores are preserved. Missing judgement counts are never guessed; affected web records are retained but excluded from EX ranks.
- **Online rooms:** Connect an existing leaderboard account through F10 to create or join official hosted rooms. No posted leaderboard score is required. Up to eight players share selection, readiness, scheduled starts, scores and room chat; leadership transfers on departure. Each PC supplies the matching chart locally. Charts, audio and complete libraries are not uploaded.
- **Presentation:** Integrates constellation Home, Song Select and Options with static galaxy backgrounds. The exact home slogan is **Tenriff BMS Rhythm Game Client**; only the subtitle below the hero is removed, while the guide remains. Options retain eight categories in a 4×2 grid and existing controls. The original options galaxy is unchanged; display-time saturation 0.92 and brightness 0.98 preserve alpha.
- **Records:** Only verified full combos display **ALL COMBO**. Best-clear lamps remain separate from best-EX records. The update includes long-title/artist presentation, static thumbnails, pointer lighting and reduced-motion paths. EX galaxy badges and clear gradients are separate assets from the options background.
- **Fixes:** Room text checks the connected PeerSession instead of an unconfigured global-chat service. F11, Enter and the 256-byte limit remain. Add Folder is Library-only; Session Mix generation, remix, save, import and cancellation are repaired.

New online rooms require hosted protocol 2 and `ex-v1`; web uploads require schema 2. Historical rulesets/replays and the direct TCP PeerProtocol v9 implementation remain intact, but old clients cannot mix with new rooms. Records remain **client_submitted**, without a claim of server replay verification or anticheat. Guest and unmarked historical records do not become upload eligible.

Actual results for final 2.0.0 builds, independent source-ZIP builds, package checks, CI and public assets are recorded in the verification gate. Earlier candidate counts are not reused as release evidence. Real IME, physical input, listening, real-account internet matches, long play, other PCs and performance/cost require separate verification.

The compatibility reference remains **1.9.9.9-final4**. **After 2.0.0, development is bug-fix-only; no new features.** See the [verification gate](release-2.0.0-gate.md) for release checks and publication status. Earlier version notices below are historical.

[EX Score](ex-score.en.md) · [Multiplayer](hosted-multiplayer.en.md) · [ALL COMBO / Song Select](menu-starlight.en.md) · [Options / Chat](options-chat.en.md) · [Library / Session Mix](library-session-mix-fix-20261114.en.md)
