# TenRiff 2.0.2

[한국어](release-2.0.2.ko.md) | [English](release-2.0.2.en.md) | [日本語](release-2.0.2.ja.md) | [简体中文](release-2.0.2.zh-CN.md)

Fix multiplayer stability and connect account-wide global chat.

- Keep the complete selected solo skin/HUD in multiplayer. Fit opponent information into free side space; hide only auxiliary cards if space is insufficient.
- Show hash-check completion only after the local chart agrees with the server acknowledgement. Poll at 250ms instead of 500ms during start/end barriers and reuse HTTPS connections.
- Fix final-score positions regressing behind the latest live update and blocking result synchronization.
- After connecting an online account, use F11 global chat from Home, song selection, options, gameplay, results and multiplayer. The new UI uses global chat only; no room or score upload is required.

Includes the 2.0.1 darker Options background, 15% smaller cards and one-line hint. Judgements, score calculations, replays, saved formats and the final4 compatibility reference are preserved. Global chat is an explicitly requested 2.0.2 exception; the post-2.0.0 maintenance policy remains in force.

[Global chat](global-chat.en.md) · [Multiplayer](multiplayer-skins.en.md) · [Verification](release-2.0.2-gate.md)
