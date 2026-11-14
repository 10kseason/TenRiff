# Starlight clear indicators and pointer lighting

[한국어](menu-starlight.ko.md) | [English](menu-starlight.en.md) | [日本語](menu-starlight.ja.md) | [简体中文](menu-starlight.zh-CN.md)

The built-in Songs, Library and Records screens use the user's original static image unchanged as `assets/menu/song-select-constellation-background.png`. It fills the screen while preserving its aspect ratio, cropping only the excess edges. When the image loads, the previous background video and procedurally drawn background constellations are skipped. Missing or failed assets fall back to the previous background. Custom skin images/videos and other screens retain their existing behavior.

The native song list uses the supplied v2 transparent artwork: green for EZ, blue for NM, red for HD, the newly supplied transparent galaxy for EX, and gold for verified full combos. Each row spans x=800 to x=1034 at the 1920 reference width. The unchanged PNG is stretched using its full width and height into the short row, making the pattern more elongated horizontally. A cached Direct2D color matrix applies 1.2× saturation while preserving source alpha. The EX original is copied unchanged as `TENRIFF_RESULT_EX_galaxy.png`. Full combos still require verified records; folders, unplayed charts, failures and assisted plays never acquire clear artwork.

The current layout contract places a large TenRiff mark and wordmark at the upper left, with the text-and-underline navigation tabs centered as a group. Constellation decoration remains on the outlined controls. The selected-song panel places the actual accuracy grade in its center; the grade keeps the existing accuracy thresholds and is not calculated from EX Score. The authored chart level or the selected external difficulty table’s level appears below the clear badge. List rows retain the artist below the title.

The main personal-best area shows only **EX Score**, accuracy and maximum combo from the highest-EX record. Its detailed grade and judgement counts belong to that same record. The highest clear lamp is tracked separately, so a higher EX Score on a weaker clear cannot lower an earned lamp. Records view continues to show the selected record. Session controls use **Note Speed** and **Gauge Shift**. Historical judgement data is preserved; current scoring follows the [EX Score transition](ex-score.en.md).

Long titles and artist names in visible rows and the selected-song panel move at 14 logical pixels per second, back and forth with a two-second dwell at each end. Text that fits remains still; reduced motion uses static ellipsis without automatic scrolling. Each text area clips its own content. The small song thumbnail uses the indexed static BGI image: BMS `STAGEFILE`, then `BACKBMP`, then a referenced static BMP image. It does not play video. Missing or undecodable images use the TenRiff mark.

Hovering a button or option spreads a warm radial light around the pointer and onto that surface, using existing hit regions without selecting or clicking. It disappears outside the window, on focus loss, over empty space or disabled controls. Gameplay hides it except in the pause menu. Reduced motion keeps a static pointer-following light with no pulsing.

The earlier [background constellations](menu-constellations.en.md) remain in the previous fallback path when the static background cannot load. The Library-only Add Folder fix was coordinated with the parallel task and preserved. TenRiff 2.0.0 also includes account-linked multiplayer and the Session Mix create/save/import fixes.

A verified full-combo record displays **ALL COMBO** in the selected-song clear badge. The emblem and outline remain intact, with subtle badge-local bloom and a small lens flare. Reduced motion freezes the slow shimmer. Song Select uses the verified full-combo flag belonging to its best-clear badge; Records uses the selected record. Full combo is never inferred by simply comparing note count and maximum combo. Scoring, judgements, DB and servers are unchanged.

See [Constellation options and room chat](options-chat.en.md) for the options-card redesign and room-chat input fix.
