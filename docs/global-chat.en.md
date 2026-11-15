# Global chat

[한국어](global-chat.ko.md) | [English](global-chat.en.md) | [日本語](global-chat.ja.md) | [简体中文](global-chat.zh-CN.md)

Choose online mode, connect your account with F10, then open global chat with F11. Home, song selection, options, gameplay, results and multiplayer rooms share one channel. Neither joining a room nor uploading a score is required.

Enter sends; Esc/F11 closes. UTF-8 input and /np now-playing messages are supported. Messages are limited to256 bytes; queued sends are paced at1second. Leaving a room or switching to solo preserves global chat and the current draft.

Guest mode, disconnecting or changing accounts clears local history and pending sends. Authentication or invalid-response failures close the send gate; reconnect through F10. Logout does not delete messages already delivered to the server.

The server retains the latest100 messages, displaying authenticated account names. The new client exposes global chat only. The old room-chat API remains for older clients.

Chat authentication is isolated from room authentication so their renewals do not disconnect each other. The updated server and additive chat tables are required. An older server rejects the dedicated chat endpoint without replacing a room session. Multiplayer server v4 and four chat tables are deployed for 2.0.2. Real-account messages, internet latency and long sessions require separate validation.
