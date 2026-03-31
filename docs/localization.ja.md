# TenRiff Localization Guide (current)

Language: [한국어](localization.md) | [English](localization.en.md) | [简体中文](localization.zh-CN.md) | 日本語

この文書は、TenRiff の現在の UI localization 構造を整理し、今後さらに言語を追加するときに関連ファイルと境界を探し直さずに済むよう、実務的な参照を提供します。

## Current Model
- 現在の公式 UI 言語は `en` と `ko` です。
- config key は `ui.language` で、既定値は `en` です。
- 無効な language token は config load 時に `en` へ正規化されます。
- language 変更は Graphics Settings で即時反映され、保存後も保持されます。
- 現在の localization 対象は主に menu、settings、help、song select、result、gameplay HUD テキストです。
- song title、artist、一部の replay/result status text などのデータ値は翻訳しません。表示安全化のみ行います。

## Main Boundaries
1. Config persistence と language-token normalization
   - `src/config/Config.h`
   - `src/config/Config.cpp`
2. App 側 string selection helpers
   - `src/app/MenuApp.h`
   - `src/app/MenuApp.cpp`
3. language state を render snapshot へ渡す経路
   - `src/app/MenuAppTail.inl`
   - `src/render/MenuWindow.h`
4. App 側 menu string generation
   - `src/app/MenuAppDeviceSettings.cpp`
   - `src/app/MenuAppSettings.cpp`
   - `src/app/MenuAppKeymap.cpp`
   - `src/app/MenuAppSkin.cpp`
   - `src/app/MenuAppSongSelectRender.cpp`
   - `src/app/MenuAppTail.inl`
5. Renderer 側 hardcoded UI strings
   - `src/render/MenuWindow_draw.inl`
   - `src/render/MenuWindow_draw_title_body.inl`
   - `src/render/MenuWindow_draw_generic_body.inl`
   - `src/render/MenuWindow_draw_songselect_body.inl`
   - `src/render/MenuWindow_draw_result_body.inl`
   - `src/render/MenuWindow_draw_gameplay_body.inl`

## Current Conventions
- app logic で作る string は `ui_text("English", "한국어")` を使う。
- 繰り返し使う token 風ラベルは専用 label helper を使う。
  - 例: `ui_on_off`, `ui_language_label`, `ui_gauge_label`, `ui_random_label`
- renderer 固定文言は `MenuWindow::draw(...)` 内の `loc(...)` と `wloc(...)` に従う。
- user data や chart metadata は翻訳しない。`sanitize_ui_text(...)` で安全化のみ行う。
- 保存される config 値と表示ラベルは分離する。
  - 例: 保存値 `hard`、表示ラベル `Hard` または `하드`

## When Adding Or Editing A UI String
1. string が app state / settings / help logic にしか存在しないなら、`MenuApp*` ファイルで `ui_text(...)` を使って追加する。
2. string が rendering code 内だけにあるなら、`MenuWindow_draw*.inl` の `loc(...)` または `wloc(...)` で追加する。
3. `on/off`、gauge、random、display mode のような繰り返し値なら、各所に複製せず label helper を追加/拡張する。
4. 保存 config 値そのものは localize しない。保存 token を表示ラベルへ変換する関数だけを追加する。
5. chart metadata、filename、replay/result path は翻訳しない。

## Recommended Workflow For A New Language
- 現在の構造は `English/Korean` のペアベースです。第三言語を本格追加するなら、ペア helper を積み増すより一般化が安全です。

1. config normalization に新しい language token を追加する。
2. `ui.language` の save/load と migration の挙動は維持したまま、新 token を受け入れる。
3. `render.ui_korean` のような bool-only handoff は language enum または token に置き換える。
4. ペア helper を実際の多言語 lookup module に昇格させる。
   - 推奨配置: `src/ui/Localization.h`, `src/ui/Localization.cpp`
5. 繰り返し label を token-based API へ移す。
6. `MenuApp*` と `MenuWindow_draw*` を走査し、残る hardcoded text を整理する。
7. docs と tests を同時更新する。

## Recommended Future Token Layout
- 保存値は短く安定した token のまま保つ。
  - 例: `en`, `ko`, `hard`, `normal`, `easy`
- 表示文字列は token table から解決する。
- 1 つの token の言語別文字列は 1 か所にまとめる。
- 画面側は literal phrase を持たず token を要求する形を優先する。

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
- Graphics Settings の Language 行が即時反映されることを確認
- Help overlay、Song Select、Result、Gameplay loading/countdown で言語混在しないことを確認
- keymap save/fail status、result restart/replay hints、song-browser hints が一緒に切り替わることを確認
- `docs/ui-audit-checklist.md` の `720p`, `1080p`, `Performance HUD on/off` 手動確認を行う

## Known Limitations
- 現在の実装は一般的な i18n system ではなく、`en/ko` 分岐ベースの layer です。
- 一部の保存済み result/replay metadata status string は英語のまま残ることがあります。
- 一部の fallback placeholder string も英語のまま残ることがあります。
- 本格的な第三言語拡張は、まず bool handoff と pair helper の一般化から始めるべきです。
