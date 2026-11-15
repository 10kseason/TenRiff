# Constellation options and global-chat input

> **2.0.1** Dim the default Options galaxy to 40% of its 2.0.0 brightness (source RGB multiplier 0.392). Reduce all eight cards to 85% width and height, including text/icons, and match their clickable bounds. Show the selected option description on one line below the cards in Korean, English, Japanese and Simplified Chinese. [2.0.1](release-2.0.1.en.md)


[한국어](options-chat.ko.md) | [English](options-chat.en.md) | [日本語](options-chat.ja.md) | [简体中文](options-chat.zh-CN.md)

The built-in options page follows the supplied `TENRIFF_constellation_options_mockup.png`, arranging its eight categories as a 4×2 card grid. Cards have distinct constellation motifs, colored icons, current values and selection highlights. Category order, settings, keyboard navigation and click targets retain their existing meaning. The constellations are static decoration, not an astronomical map. F1 help remains available.

The supplied `TENRIFF_galaxy_background.png` is copied unchanged to `assets/menu/options-galaxy-background.png`. A display-time color matrix applies saturation 0.92 and brightness 0.392 while preserving alpha. The image covers the screen at its original aspect ratio, cropping excess edges. It applies to built-in options/settings without overriding custom skin backgrounds; failed image loading returns to the previous background path. Quiet border halos and constellation panels change presentation rather than setting behavior.

In 2.0.2, input and sends use the authenticated global-chat service. Open with F11 and send with Enter, regardless of room membership. Guest mode, disconnect or account change clears local history and queued sends.

[Global chat](global-chat.en.md) · [2.0.2 verification](release-2.0.2-gate.md)
