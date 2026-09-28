# TenRiff 1.7.8 Release Gate

## Scope

Windows x64 client and public source with modern native menus, Japanese UI and the editable Luma Keys gameplay skin. Every key count from 4 through 16 has a skin layout; this does not add chart-conversion modes. Judgement, input and audio behavior are unchanged by the skin work.

## Authoring

Use the [web skin editor](https://tenriff-skin-editor.lastestarcorp.chatgpt.site/) or bundled `launch_skin_editor.bat`. Select **Digital keyboard & gameplay**, edit shared or per-mode settings, and save a copy. Extract an exported ZIP before importing its skin folder in **Options > Skins > Import Skin**. Further edits can be loaded with **F5 / Reload Skin**. The new native fields require this release; old clients may ignore them.

Native settings cover 21 metrics, 28 colors, 11 motion controls, 11 HUD rectangles, 7 font roles and 5 vector sprite slots. Missing fields use defaults; per-mode categories merge by key and sprite arrays replace whole slots. Empty arrays hide a sprite. PNG replacements take priority.

## Validation scope

- Windows Release build and all three CTest entries; unit runner contains 831 cases.
- Editor suite: 21 tests for schema, history, per-mode merge, sprite validation, ZIP bytes/CRC and key response.
- Synthetic actual-renderer captures for 4K–16K, compact 16K, motion and skin settings. A browser-edited document was loaded at 4K/16K to check isolated note overrides, shared colors and HUD movement. PNG override priority was also checked.
- Final archive privacy/inventory scan, ZIP CRC, extracted-file hashes, standalone source build and remote download SHA-256 are required before completion of publication.

Synthetic captures do not establish real-player input/audio latency, long-session FPS or multi-PC compatibility. The in-app browser showed the export-start message, but a downloaded browser ZIP could not be read back there; ZIP serialization passed independently.

## Privacy boundary

No profiles, account connection files, upload credentials, replay/result exports, local logs, agent instructions or private model checkpoints are included. Documentation screenshots use synthetic chart names and the generic PLAYER profile. Public client protocol/validation code and documented endpoint names remain included; ranking-server implementation and operational settings are excluded. No real account upload was performed for validation.
