# TenRiff 1.5.1 Fixed Stable Baseline

This document locks the stable TenRiff contract at release `1.5.1`. Follow-up work must not break these rules without an explicit compatibility change and migration. Historical baseline documents remain available, but this document, the current code, and `docs/current-state.en.md` take precedence.

## Release Identity

- Fixed baseline line: `1.5.1`
- Release name: `fixed stable baseline`
- Baseline platform: Windows GUI
- Product surface: BMS family only (`.bms/.bme/.bml/.pms`)
- Compatible self-hosted server: `TenRiff Server v1.1.0`
- Official assets: runtime ZIP, public-source ZIP, and SHA-256 manifest

## Stable Contract

- The main flow remains `Title -> Song Select -> Gameplay -> Result`; local records survive online-service failures.
- BMS parsing/timing, long notes, landmines, keysounds, replay v3, and deterministic replay verification are baseline behavior.
- Gameplay input timing remains anchored to the audio playback head. RawInput stays preferred with a bound-key polling shadow and Polling fallback, without rewriting the saved backend setting.
- NK3 keeps P64 plus the host beam safety solver authoritative. The generalized MLP is used only for non-10K sources converted to 10K.
- Windows packages include `Mainmusic/`, bundled skins, and the NK3 runtime/models, but no Songs or standalone BMS key-converter executable.

## Account And Global Chat

- `F10` opens account setup and requires selecting the main server or a user-supplied private API server before login or registration.
- Passwords are never stored in plaintext. The server uses a unique salt and PBKDF2-HMAC-SHA256 with 600,000 iterations; bearer tokens are stored hashed.
- Persisted client sessions are protected by Windows DPAPI. Account/password recovery is not required by this baseline.
- The password field accepts `Ctrl+V`, preserves leading and trailing spaces, and always renders the pasted value masked.
- `F8` opens the same server-wide global chat overlay from menus, gameplay, and results without leaking chat input into gameplay or menu commands.
- `/np` posts the current title and artist. Chat URLs require an animated warning and explicit approval before opening.
- While signed in, multiplayer room discovery uses the authenticated room list from the selected main/private server. Signed-out users retain LAN discovery.

## Ranked BMS

- Bulk song indexing is local-only and never pre-populates the ranked catalog.
- A new BMS becomes a registration candidate on an actual result submission. Only a challenge-bound replay that reproduces successfully may enter the leaderboard.
- osu-derived, autoplay/assist, incomplete, and non-reproducible submissions fail closed.
- Administrators may exclude any BMS SHA-256 from registration and leaderboard visibility.
- Client score claims are not trusted; the server uses exact chart bytes and an external replay-verifier result.

## Network And Security

- Game server: `27301/TCP`
- Origin API: `127.0.0.1:27302/TCP`, never directly exposed to LAN/WAN
- Public HTTPS gateway: `27303/TCP`, plus `27303/UDP` when HTTP/3 is enabled
- `27304~27305` are reserved; default host mappings do not expose `27300`, `80`, or `443`.
- Remote account, chat, and ranking traffic requires a trusted HTTPS certificate. HTTP is allowed only for loopback development.

## Compatibility Rule

- Future work should be additive over the `1.5.1 fixed stable baseline`.
- Changes to replay, scoring, chart identity, account storage, APIs, or ports require versioned migration and cross-compatibility tests.
- Git tag `1.5.1` is the immutable baseline identifier; the tag and published assets are not replaced after release.

## Companion Docs

- `docs/current-state.en.md`
- `docs/config.en.md`
- `docs/gameplay-guide.en.md`
- `docs/multiplayer.md`
- `docs/ranked-integrity-plan.en.md`
- `docs/release-1.5.1-gate.md`
