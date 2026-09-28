# TenRiff UIの言語対応

Language: [한국어](localization.md) | [English](localization.en.md) | 日本語 | [简体中文](localization.zh-CN.md)

## 日本語への切り替え

Options → Profile Setup → Language、または初回のQuick Setupで「Japanese」を選びます。左右キーで英語 → 韓国語 → 日本語を切り替えられます。言語とメニュー文字サイズは選択時に適用され、現在のプロファイルに保存されます。

設定キーは`ui.language`です。`en`・`ko`・`ja`に対応し、初期値は`en`です。`jp`・`japanese`・`ja-jp`・`ja_JP`も大文字・小文字を区別せず`ja`として読み込みます。不明な値は英語になります。

## 実装

| ファイル | 役割 |
| --- | --- |
| `src/ui/Localization.h/.cpp` | 言語enum、設定トークン、言語切り替え、共通の翻訳処理 |
| `src/ui/JapaneseStrings.inc` | 英語のUI文をキーとする組み込み日本語辞書 |
| `src/config/Config.cpp` | 言語設定の保存と読み込み |
| `src/app/MenuApp.cpp` | `ui_language()`と`ui_text(英語, 韓国語)` |
| `src/app/menu/settings/*SettingsView` | 言語enumを受け取る設定画面 |
| `src/app/MenuAppTail.inl` | `MenuRenderData.ui_language`と読み込み状態の受け渡し |
| `src/render/MenuWindow_draw*.inl` | `loc`・`wloc`・`result_loc`による描画時の翻訳 |

従来の「韓国語かどうか」という真偽値を言語enumへ変更しました。既存の英語・韓国語は維持し、日本語を1つの辞書で管理します。辞書は一度だけ構築し、検索時には文字列の新規割り当てを行いません。未登録の文は英語を表示します。

BMS編集状態とプレイ準備の段階名は内部では元の値を維持し、画面へ渡すときに既知の文を翻訳します。判定・音声・ノート配置・保存形式は変更しません。

## UI文の追加

1. アプリでは`ui_text("English", "한국어")`、描画では`loc`・`wloc`・`result_loc`、設定画面では`localized(language, ...)`を使用します。
2. 英語と完全一致するキーを`JapaneseStrings.inc`へ追加します。前後の空白、改行、UTF-8の記号も合わせてください。
3. 設定トークン・曲名・アーティスト名・ファイル名・プレイヤー名・チャット本文は翻訳しません。
4. 動的な文を追加する場合は検査対象を明示します。部分一致でユーザーデータを書き換えないでください。

## 検査

```powershell
python tools/audit_japanese_localization.py
python tools/audit_japanese_localization.py --json
```

翻訳呼び出し、画面名、内蔵難易度表、既知のBMS編集状態・読み込み段階を検査します。不足・重複・空の翻訳・確認できない動的キーがあれば終了コード1を返します。件数は実行時の出力を確認してください。

C++テストでは言語トークン、両方向の切り替え、主要画面の文、英語・韓国語と未知の値の維持を確認します。設定テストでは3言語の保存・再読み込みと、日本語選択時の即時反映を確認します。

## 範囲と制限

ソース検査の成功は全画面の視覚確認を意味しません。サーバー・音声・パーサーの診断、過去の結果ファイル内の説明、外部スキン画像内の文字、ユーザーデータは原文のまま残る場合があります。FAST/SLOW・BGA・ASIO・Rate・キー名などの用語も一部維持します。OSファイル選択画面の種類名には英語が残ります。

日本語のタイトル・選曲・設定・ヘルプ・結果・読み込み画面を720p/1080pで開き、文字切れ・代替フォント・改行を確認してください。単体テストだけでは実機プレイや通信対戦を検証できません。
