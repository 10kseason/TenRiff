# Bundled TenRiff Skin Instructions

These instructions apply to every distributable skin under `skins/`.

## Source of truth

- Read [`../docs/skin-agent-guide.md`](../docs/skin-agent-guide.md) before creating or extending a skin.
- Use [`../docs/skin-format.md`](../docs/skin-format.md) for field behavior.
- Validate every `skin.json` against [`../docs/tenriff-skin.schema.json`](../docs/tenriff-skin.schema.json).
- Keep the official schema URL exactly as shown in [`../examples/skins/TenRiff-Example/skin.json`](../examples/skins/TenRiff-Example/skin.json).

## Distribution boundary

- `skins/` contains finished skins that are copied beside `TenRiff.exe` and shipped in release packages.
- `examples/skins/` contains only `TenRiff-Example`, the minimal authoring template.
- Never copy assets from commercial rhythm games or unlicensed third-party skins. Record asset provenance in the skin README.
- Preserve unknown existing assets and user edits. Do not replace another bundled skin for an unrelated request.

## Required workflow

1. Write a visual brief covering mood, palette, note shape, supported key modes, and readability constraints.
2. Start from `examples/skins/TenRiff-Example` or a structurally similar bundled skin.
3. Create `skin.json` first, then add only referenced or conventional assets.
4. Keep every asset path relative to `skin.json`; absolute paths and `..` are forbidden.
5. Use shallow `gameplay.modes.1k` through `16k` overrides for key-count arrays, and `gameplay.modes.7+1` for the dedicated BMS 7-key-plus-scratch layout.
6. Keep the gameplay center dark enough for notes, holds, receptors, and lane dividers.
7. Inspect every generated bitmap and keep project-used outputs inside the skin folder.
8. Document install steps, supported modes, provenance, and intentional Native fallbacks.

## Validation

From the repository root:

```powershell
Get-Content -Raw skins/<skin>/skin.json |
  Test-Json -SchemaFile docs/tenriff-skin.schema.json
```

For universal claims, inspect 4K, 7K, 10K, and 16K. Engine or packaging changes require a rebuild and the registered CTest suite; skin-only bitmap or manifest edits do not.
