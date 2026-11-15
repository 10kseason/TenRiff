# Keep the selected solo skin in multiplayer

[한국어](multiplayer-skins.ko.md) | [English](multiplayer-skins.en.md) | [日本語](multiplayer-skins.ja.md) | [简体中文](multiplayer-skins.zh-CN.md)

Native and imported LR2 use the same selected HUD in solo and multiplayer. TenRiff image skins and menu-only skins retain their selected assets and style.

Remove the multiplayer-only Classic fallback for Studio Deck. Online standings fit into free side space around the field, gauge, song information, local score, riff map and performance panel, scaling with player count.

Open Options → Skins from the room, choose the skin/HUD, and return. Opening Options clears Ready; ready up again. Skin changes during an active match remain blocked.

The shared ghost layout, judgement, scores, network protocol and saved formats are unchanged. Arbitrary authored skin coordinates and real multi-PC play need separate verification.

If there is no free space at a readable size, only the auxiliary opponent cards are hidden; full standings remain on the results screen. The field and selected HUD stay in place.

## Hash checks and round synchronization

The lobby shows `Hash check complete.` only when this PC's verified chart and the room's acknowledged hash and size agree. Changed selections, pending acknowledgements and disconnection never show completion.

Hosted multiplayer reuses HTTPS connections on its worker thread and polls every 250ms instead of 500ms while waiting for loading, final results or round reset. Normal polling and the server's five-second start countdown remain unchanged. This reduces client-side delay; it does not guarantee halving total synchronization time.

Final result sample positions no longer regress behind the last live score and trigger server rejection. A rejected final result ends the connection instead of leaving a long wait. A peer leaving during loading still returns the remaining clients to the lobby. Real internet latency and extended multi-PC play need separate validation.
