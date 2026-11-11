# TenRiff 1.9.9.9-final6

[한국어](release-1.9.9.9-final6.ko.md) | [English](release-1.9.9.9-final6.en.md) | [日本語](release-1.9.9.9-final6.ja.md) | [简体中文](release-1.9.9.9-final6.zh-CN.md)

Separates 7+1/8K bindings, defaults scratch to red and clips notes at 16-lane gaps. Adds Unlimited VSync handling, timing-marker caching, LR2oraja score import, authored BMS levels and native high-resolution BGA frames.

- Options → Keymap and the NKRO test offer separate 7+1 and 8K layouts. Set primary and secondary bindings for the scratch and each of the seven keys.
- Profiles without a 7+1 layout start from their current 8K bindings. Saving the keymap in an older build drops the 7+1 section.
- Scratch defaults to red. Saved profile and skin colors take precedence.
- The 14+2 DP and 16K center gap stays clear of notes. Taps and holds draw inside their own block, including on the ghost field.
- `Unlimited` disables VSync regardless of its saved setting, with limits of 1500 FPS for gameplay and 300 FPS for menus. `Match Display` applies the saved VSync setting.
- Dot and Barcode timing markers cache a bitmap when their history stays stable across consecutive frames. Changed histories draw immediately.
- Open Options → Profile → Import Score Database and select LR2oraja's `player/<profile>/score.db`. LN/CN/HCN records are stored separately. Song Select shows the source, clear type, EX, combo and BP, preferring LN, then CN and HCN.
- The database ghost stores per-note judgement categories. Restoring a millisecond distribution requires separate replay input logs. Clear type, combo and minimum BP may come from different personal-best plays.
- In-game base difficulty uses BMS `#PLAYLEVEL`, with the internal calculation option removed. Older calculated caches refresh during the next index. A selected external difficulty table supplies its own levels.
- BGA above 480 pixels high uses native frame dimensions. Image ONNX processing applies to sources at or below 480 pixels high.

The reporter's PC frame rate and tearing, real personal databases, physical input, listening and extended play need further verification. [Record import](lr2-clear-import.en.md)

- Remove account and per-track identifiers from eight bundled music files while preserving Suno AI attribution, compressed audio and embedded artwork.

[Release verification](release-1.9.9.9-final6-gate.md)
