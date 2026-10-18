> Superseded by [1.9.5.3](release-1.9.5.3.en.md). The following dated local-candidate record is historical; current controls and publication status are in the release notes.

# Skin settings overhaul (local candidate)

Includes every change from the preceding 1.9.5.2 feedback candidate. Published releases, installations and player profiles remain untouched.

- Numbered preview buttons select the edited lane, replacing Target Lane. They retain the current setting row and select its color, width and following gap (the preceding gap for the last lane). Existing number shortcuts and 7+1 scratch positioning remain supported.
- Skin Settings hides Single Color, Note Border, Image Aspect, Visual Opacity, Outline Alpha, Lane Tint Opacity, 16K Center Gap and Divider Width. Stored values remain readable and preserved. Effective divider width is fixed at 100%.
- Fresh settings use Rect (Simple) and a 100% 16K center gap. Explicit saved shapes and gaps remain unchanged.
- Automatic image fitting uses the configured destination note width and height, not the source image dimensions. A square destination contains the image with its aspect preserved; a rectangular destination stretches it. Legacy aspect tokens remain roundtrippable but live player, ghost and preview rendering use the automatic rule.
- UI Font and Choose Font File move to Profile immediately after language/text size, retaining the existing storage fields and validated TTF/OTF picker. Changes save immediately.
- Judgement Line accepts 1–100%; old 0% values clamp to 1% when loaded. Client, preview, skin parser and editor validation agree.

Hiding controls does not reset their stored values: an existing Single Color selection still applies. Judgement timing, scoring, replay and audio contracts are unchanged. The web editor was deployed with user approval as Sites version 8 at the [existing public URL](https://tenriff-skin-editor.lastestarcorp.chatgpt.site). The game candidate remains local.
