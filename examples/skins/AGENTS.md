# TenRiff Skin Agent Instructions

These instructions apply to the single authoring template under `examples/skins/`.

`examples/skins/` must keep only `TenRiff-Example`. Finished skins belong under
[`../../skins/`](../../skins/) and follow [`../../skins/AGENTS.md`](../../skins/AGENTS.md).

## Source of truth

- Read [`../../docs/skin-agent-guide.md`](../../docs/skin-agent-guide.md) before creating a skin.
- Use [`../../docs/skin-format.md`](../../docs/skin-format.md) for field behavior.
- Validate every `skin.json` against [`../../docs/tenriff-skin.schema.json`](../../docs/tenriff-skin.schema.json).
- Keep the official schema URL exactly as shown in `TenRiff-Example/skin.json`.

## Working boundary

- For a skin-only request, edit only that skin folder and its documentation.
- Do not change the renderer, schema, profile configuration, or another skin unless the user asks for an engine feature.
- Preserve unknown existing assets and user edits. Add new files with descriptive names instead of replacing unrelated art.
- Never copy assets from commercial rhythm games or other copyrighted skins. Use original, licensed, or user-provided material and record its source in the skin README.

## Required workflow

1. Write a short visual brief: mood, palette, note shape, supported key modes, and readability constraints.
2. Start from `TenRiff-Example`; inspect a finished bundled skin under `../../skins/` when a richer structure is needed.
3. Create `skin.json` first, then add only the assets referenced by it or supported standard filenames.
4. Keep all asset paths relative to `skin.json`; never use absolute paths or `..`.
5. Use `gameplay.modes` for key-count-specific arrays. Mode keys are lowercase `1k` through `16k` and overrides are shallow.
6. Prefer a dark, low-detail gameplay center. Notes, hold heads/tails, receptors, and lane dividers must remain readable over the background.
7. Inspect every generated bitmap before committing it to the skin. Project-used generated images must live inside the skin folder.
8. Add or update the skin README with install steps, supported modes, asset provenance, and any intentional Native fallbacks.

## Asset conventions

- Lobby and gameplay backgrounds: 16:9, preferably `1920x1080` or larger.
- Logo, notes, holds, and receptors: transparent PNG when supplied.
- Supported bitmap extensions: `.png`, `.jpg`, `.jpeg`, `.bmp`.
- Omitted slots intentionally fall back to TenRiff Native rendering; do not add empty placeholder images.
- Use `note_aspect: "width"` for square arrow art, `contain` to preserve art inside the note box, and `stretch` for bar notes.
- Check at least 4K, 7K, 10K, and 16K when claiming universal support.

## Validation

From the repository root, run:

```powershell
Get-Content -Raw examples/skins/TenRiff-Example/skin.json |
  Test-Json -SchemaFile docs/tenriff-skin.schema.json
```

Then import or select the folder in `Options > Skins`, press `F5`, and check:

- no warnings on the Skins screen;
- the lobby remains readable;
- gameplay notes and holds are visible;
- per-mode colors and geometry stay within the playfield;
- no image is stretched unexpectedly.

Skin-only changes do not require rebuilding TenRiff. Engine or schema changes do.
