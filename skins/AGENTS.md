# TenRiff skin authoring rules

This file guides skin assets and manifests in this directory. Project instructions
and explicit user direction take precedence. It grants no permission to publish.

1. Read `docs/skin-agent-guide.md`, `docs/skin-format.md`, and the accompanying local
   `docs/tenriff-skin.schema.json`. The local candidate can be newer than the public URL.
2. Start from `examples/skins/TenRiff-Example` in a new folder. For the complete
   Modern Common style, copy a bundled manifest and its `modern/background.png`.
3. Keep `format: "tenriff-skin"`, `version: 1`. Menu and gameplay `renderer` opt in
   independently. Use lower-case `1k` through `16k`; `7+1` is a separate mode.
4. Preserve originals and unrelated work. Bundled `skin.legacy.json`, original images,
   and `README.legacy.md` are historical artifacts; do not regenerate or replace them.
5. Note/head/tail vector layers 0 and 1 respond to border opacity. Put the solid
   body at index 2 or later. Never make visibility depend only on the first two layers.
6. Omitted image slots auto-detect conventional files; `""` disables that slot's
   direct auto-detection. Screen background fallbacks are separate; see the format guide.
7. Use relative paths within the skin. Include source/license notes for assets.
   Do not embed scripts, secrets, machine paths, or another game's unlicensed art.
8. Keep profile-only controls out of manifests. Judgement-line skin gain is 0–1
   and multiplies the user dimmer; it does not change alpha or judgement timing.
9. Validate schema, assets, mode merging, and an editor save/reopen. Check 4K, 7K,
   10K, 16K and any advertised scratch mode, with borders on and off.
10. Report what was actually verified. Engine/input/audio/scoring changes require
    a separate explicitly scoped task; a skin-only request edits skins and documentation.
