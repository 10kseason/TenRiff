# TenRiff Localization Guide (current)

Language: [한국어](localization.md) | English | [简体中文](localization.zh-CN.md) | [日本語](localization.ja.md)

This document summarizes TenRiff's current UI localization structure and gives a practical reference for adding more languages later without having to rediscover the relevant files and boundaries.

## Current Model
- The current official UI languages are `en` and `ko`.
- The config key is `ui.language`, and the default is `en`.
- Invalid language tokens are normalized to `en` when loading config.
- Language changes apply immediately in Graphics Settings and persist after save.
- The current localization scope is mainly menu, settings, help, song select, result, and gameplay HUD text.
- Data values such as song titles, artists, and some persisted replay/result status text are not translated. They are sanitized for display instead.

## Main Boundaries
1. Config persistence and language-token normalization
   - `src/config/Config.h`
   - `src/config/Config.cpp`
2. App-side string selection helpers
   - `src/app/MenuApp.h`
   - `src/app/MenuApp.cpp`
3. Passing language state into the render snapshot
   - `src/app/MenuAppTail.inl`
   - `src/render/MenuWindow.h`
4. App-side menu string generation
   - `src/app/MenuAppDeviceSettings.cpp`
   - `src/app/MenuAppSettings.cpp`
   - `src/app/MenuAppKeymap.cpp`
   - `src/app/MenuAppSkin.cpp`
   - `src/app/MenuAppSongSelectRender.cpp`
   - `src/app/MenuAppTail.inl`
5. Renderer-side hardcoded UI strings
   - `src/render/MenuWindow_draw.inl`
   - `src/render/MenuWindow_draw_title_body.inl`
   - `src/render/MenuWindow_draw_generic_body.inl`
   - `src/render/MenuWindow_draw_songselect_body.inl`
   - `src/render/MenuWindow_draw_result_body.inl`
   - `src/render/MenuWindow_draw_gameplay_body.inl`

## Current Conventions
- For strings created in app logic, use `ui_text("English", "한국어")`.
- For repeated token-like labels, use dedicated label helpers.
  - Examples: `ui_on_off`, `ui_language_label`, `ui_gauge_label`, `ui_random_label`
- For fixed renderer-only strings, follow `loc(...)` and `wloc(...)` inside `MenuWindow::draw(...)`.
- Do not translate user data or chart metadata. Only make it safe with `sanitize_ui_text(...)`.
- Keep persisted config values separate from display labels.
  - Example: persisted value `hard`, display label `Hard` or `하드`

## When Adding Or Editing A UI String
1. If the string only exists in app state, settings, or help logic, add it in a `MenuApp*` file with `ui_text(...)`.
2. If the string only exists inside rendering code, add it through `loc(...)` or `wloc(...)` in `MenuWindow_draw*.inl`.
3. If it is a repeated value such as `on/off`, gauge, random, or display mode, add or extend a label helper instead of copying the pair everywhere.
4. Do not localize persisted config values themselves. Only add a function that maps the stored token to a display label.
5. Do not translate chart metadata, filenames, or replay/result paths.

## Recommended Workflow For A New Language
- The current structure is pair-based around `English/Korean`. Once a third language is added, it is safer to generalize the structure instead of stacking more pair helpers.

1. Add the new language token to config normalization.
2. Keep `ui.language` save/load and migration behavior stable while accepting the new token.
3. Replace bool-only render handoff such as `render.ui_korean` with a language enum or language token.
4. Promote the pair helper into a real multi-language lookup module.
   - Suggested location: `src/ui/Localization.h`, `src/ui/Localization.cpp`
5. Move repeated labels into that token-based API.
6. Sweep `MenuApp*` and `MenuWindow_draw*` files for remaining hardcoded text.
7. Update docs and tests together.

## Recommended Future Token Layout
- Keep persisted values as short stable tokens.
  - Example: `en`, `ko`, `hard`, `normal`, `easy`
- Resolve display strings from a token table.
- Keep all language variants of one token in a single place.
- Prefer screen files requesting tokens instead of carrying literal phrases.

Example:

```cpp
enum class UiTextId {
    Back,
    Save,
    Language,
    GaugeHard,
};

std::string localized_text(Language lang, UiTextId id);
```

## Minimum Regression Checklist
- `config save and load preserve ui language setting`
- `config load normalizes invalid ui language to english`
- Verify that the Language row in Graphics Settings updates the UI immediately
- Verify that Help overlay, Song Select, Result, and Gameplay loading/countdown do not mix languages
- Verify that keymap save/fail status, result restart/replay hints, and song-browser hints switch together
- Run the manual `720p`, `1080p`, `Performance HUD on/off` sweep from `docs/ui-audit-checklist.md`

## Known Limitations
- The current implementation is not a general i18n system. It is an `en/ko` branch-based layer.
- Some persisted result/replay metadata status strings can still remain in English.
- Some fallback placeholder strings may still be English.
- A proper third-language expansion should generalize the bool handoff and pair helpers first.
