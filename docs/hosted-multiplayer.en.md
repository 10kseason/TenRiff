# Hosted multiplayer

This guide describes hosted multiplayer in TenRiff 2.0.0. The final4 compatibility reference, judgement, replays and legacy PeerProtocol v9 are unchanged. Online rooms use separate hosted protocol v2 and `score_version="ex-v1"`. Scores and standings use [EX Score](ex-score.en.md); older hosted v1 and direct-IP clients cannot join these rooms.

Choose Online with F10 and import your existing leaderboard connection. Open Multiplayer to create or join an official room. The server verifies the account; no leaderboard score is required. Guest, expired and revoked connections cannot join. The game UI has no direct-IP or LAN entry path.

A separate TenRiff Multiplayer Site manages rooms over HTTPS with D1. Up to eight players can join. Leadership transfers when the leader leaves. The leader selects a local BMS and other PCs match that file locally; all players must match and be ready before starting. No complete library, chart or audio files are transferred. Selected-chart matching replaces the direct-IP mode's precomputed shared-library list.

Audio and input judgement stay local. The server schedules a future start consumed by every client; late schedules or excessively slow responses fail closed. State and opponent scores exchange every 500ms, so opponent display may lag. Internet latency, eight-player load, long play and D1 usage need separate verification. Hosting does not provide replay verification or anticheat.

F11 opens room chat. Switching to Guest or disconnecting/replacing the account ends multiplayer. The official multiplayer Site receives the connection key over HTTPS only to verify it at the leaderboard, never to persist it. Subsequent requests use short-lived session tokens stored only as hashes. Revalidation occurs every 20 seconds; revocation has a maximum 30-second session window.

2026-11-14: The approved new Site has four D1 tables and MULTIPLAYER_ENABLED=1. Public API ingress requires account sessions; live checks reject anonymous requests, forged identity headers and invalid keys. Real-account matches, internet latency, sustained load and cost remain unverified. A fresh installation starts as Guest; connect an account in F10.
