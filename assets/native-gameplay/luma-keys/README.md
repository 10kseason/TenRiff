# Luma Keys — native gameplay skin

Original TenRiff UI artwork, MIT, authored for this project. No assets were copied from other games; no image-generation service or external art was used.

## Visual brief

A digital instrument: satin key faces, inset indicator strips, fast press travel and a softer release. Pearl/blue keys retain the player's lane palette; odd layouts keep a readable central key. Notes are compact luminous bars; holds have darker centres and bright side rails. Mint contact lighting is concentrated near the judgement line, leaving the incoming-note area dark.

All thirteen key counts from 4K to 16K share scalable geometry. 11K, 13K and 15K have explicit palettes and settings. Scratch-aware imported skins keep their existing rendering path.

## Files and runtime

`KeyIdle`, `KeyLit`, `Note`, `HoldHead`, and `HoldTail`, each in pearl/ice/mint reference colors: 15 SVGs. The game tints the same geometry to the actual lane color and caches GPU sprites when the palette or skin changes. The SVGs are editable references; the compiled atlas means no loose asset lookup during play.

Source: `tools/generate_luma_keys_assets.py`; generated atlas: `src/render/LumaKeysAssets.h`. Run the generator with `--check` to detect stale exports. Rect notes use the new sprites, while other selected shapes retain their procedural geometry. Existing border, opacity, line visibility, note sizing, and hit-brightness settings still apply. Inherited menu colors do not replace the native gameplay palette.
