# TenRiff UI localization

Language: [한국어](localization.md) | English | [日本語](localization.ja.md) | [简体中文](localization.zh-CN.md)

## Choosing a language

The client supports English (`en`), Korean (`ko`), and Japanese (`ja`); the default is `en`. In Options → Graphics Settings → Language, Left/Right cycles all three choices in either direction. The UI changes immediately and the setting is saved when leaving the screen.

`jp`, `japanese`, `ja-jp`, and `ja_JP` are accepted case-insensitively and saved as `ja`. Unknown tokens retain the existing English fallback.

## Implementation

| File | Responsibility |
| --- | --- |
| `src/ui/Localization.h/.cpp` | `ui::Language`, token normalization, cycling, shared text lookup |
| `src/ui/JapaneseStrings.inc` | Built-in Japanese catalog keyed by exact English UI text |
| `src/config/Config.cpp` | `ui.language` persistence |
| `src/app/MenuApp.cpp` | `ui_language()` and the existing `ui_text(english, korean)` helper |
| `src/app/menu/settings/*SettingsView` | Settings views receiving the language enum |
| `src/app/MenuAppTail.inl` | `MenuRenderData.ui_language` and loading snapshots |
| `src/render/MenuWindow_draw*.inl` | `loc`, `wloc`, and `result_loc` forwarding to shared lookup |

The old Korean boolean is replaced by an enum. Existing English and Korean literals remain at their call sites; Japanese is maintained in one compiled catalog. The lookup map is built once, and lookups use static string views without allocating. Missing keys fall back to English.

Internal BMS editor status and loading-stage values remain stable; recognized phrases are translated only when preparing the UI snapshot. Localization does not change judgement, audio, note geometry, or persisted skin tokens.

## Adding text

1. Use `ui_text("English", "한국어")` in app code, `loc`/`wloc`/`result_loc` in rendering code, and `localized(language, ...)` in settings views.
2. Add the exact English key and its Japanese translation to `JapaneseStrings.inc`. Preserve intentional leading/trailing spaces, newlines, and UTF-8 symbols.
3. Keep persisted tokens, chart metadata, filenames, player names, and chat text unchanged.
4. Give new dynamic keys an explicit audit path. Do not translate user data by substring replacement.

## Verification

```powershell
python tools/audit_japanese_localization.py
python tools/audit_japanese_localization.py --json
```

The audit covers explicit localization calls, screen titles, built-in difficulty-table names, and known BMS editor/loading status phrases. Missing keys, duplicate keys, empty translations, and unresolved dynamic keys produce exit code 1. Use the printed counts rather than a frozen count in documentation.

`test_localization.cpp` checks token normalization, both cycling directions, representative screen text, original English/Korean behavior, and unknown-value preservation. Config tests round-trip `en`/`ko`/`ja`; graphics tests check immediate Japanese view updates and persistence effects.

## Limits

A passing source audit is not full-screen or runtime visual QA. Server/audio/parser diagnostics, free-form historical result descriptions, third-party skin artwork, and user content may remain in their original language. Technical and rhythm-game labels such as FAST/SLOW, BGA, ASIO, Rate, and key names may stay Latin. Some OS file-picker type descriptions remain English.

Check title, song select, settings, help, results, and loading in Japanese at 720p and 1080p for clipping, font fallback, and wrapping. Source checks and unit tests do not prove hardware gameplay or remote multiplayer behavior.
