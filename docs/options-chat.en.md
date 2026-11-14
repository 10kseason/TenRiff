# Constellation options and room-chat input

[한국어](options-chat.ko.md) | [English](options-chat.en.md) | [日本語](options-chat.ja.md) | [简体中文](options-chat.zh-CN.md)

The built-in options page follows the supplied `TENRIFF_constellation_options_mockup.png`, arranging its eight categories as a 4×2 card grid. Cards have distinct constellation motifs, colored icons, current values and selection highlights. Category order, settings, keyboard navigation and click targets retain their existing meaning. The constellations are static decoration, not an astronomical map. F1 help remains available.

The supplied `TENRIFF_galaxy_background.png` is copied unchanged to `assets/menu/options-galaxy-background.png`. A display-time color matrix applies saturation 0.92 and brightness 0.98 while preserving alpha. The image covers the screen at its original aspect ratio, cropping excess edges. It applies to built-in options/settings without overriding custom skin backgrounds; failed image loading returns to the previous background path. Quiet border halos and constellation panels change presentation rather than setting behavior.

Room-chat input previously depended on whether an unrelated global-chat service was configured. An open room-chat overlay now accepts committed text through the connected `PeerSession`, including hosted rooms. F11 open/close, Enter send and the 256-byte UTF-8 limit remain unchanged. Hidden/disconnected chat and account or URL-warning dialogs block input from reaching the wrong field. Account authentication, protocol 2 and server rate limits are unchanged.

Regression coverage is provided for committed Korean/ASCII input queues, local two-way chat, hosted mock responses and two-client Worker chat. This document does not claim passed tests or performance figures; consult the [2.0.0 verification gate](release-2.0.0-gate.md) for executed results. Real Windows IME composition/commit-key timing, production-account messages, internet latency, long sessions and other PCs require separate checks. Existing [ALL COMBO presentation](menu-starlight.en.md), scoring and judgement rules are outside this change.
