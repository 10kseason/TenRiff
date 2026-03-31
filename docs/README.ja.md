# TenRiff Docs Map

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | 日本語

ルートの [`README.ja.md`](../README.ja.md) を読んだ後に参照する、詳細文書への次段インデックスです。設計文書と current-state 文書を一緒に持っているため、素早く文脈をつかむには次の順で読むのが最も効率的です。

このコードベースもまた、急速な反復と実験を通じて育った `vibe coding` 作品として読むのが適切です。

## Recommended Reading Order
1. `docs/current-state.ja.md`
   - 現在の製品状態、主要サブシステム、確認済みコマンド、残る手動検証項目
2. `docs/baseline-1.1.2.ja.md`
   - 現在の作業をどの基準の上に積み重ねるべきかを定義する `1.1.2 final stable` ベースライン文書
3. `docs/gameplay-guide.ja.md`
   - プレイ開始方法、曲選択、操作、HUD、判定、結果画面を実際の利用者視点で説明する文書
4. `docs/config.ja.md`
   - 実際の config / profile / keymap 構造
5. `docs/localization.ja.md`
   - 現在の英語/韓国語 UI 構造と、今後言語を増やすときに触るべきファイル境界
6. `docs/menu.ja.md`
   - menu / state machine / song-select フロー
7. `docs/core-loop.ja.md`
   - プレイループとデータフロー
8. `docs/roadmap.ja.md`
   - 今後の中長期方向
9. `docs/developer-extension-guide.ja.md`
   - mode/mod 拡張、runtime migration、replay/result、保守作業の追加位置を説明する開発者向けガイド

## Which Docs Are Source Of Truth
- `docs/current-state.ja.md`
  - 現在の実装状態の要約
- `docs/baseline-1.1.2.ja.md`
  - 後続作業が維持すべき `1.1.2 final stable` ベースライン文書
- `docs/config.ja.md`
  - 実際の `config/config.json`、`profiles/<name>/config.json`、`keymap.json` に基づく文書

## Historical / Design Docs
- `docs/menu.ja.md`
  - menu / state machine / 低遅延方針の設計文書
- `docs/core-loop.ja.md`
  - プレイループ初期設計とデータフロー説明
- `docs/localization.ja.md`
  - 現在の UI ローカライズ構造と将来の多言語拡張の参考
- `docs/latency.ja.md`, `docs/modes.ja.md`, `docs/gap-analysis.ja.md`, `docs/roadmap.ja.md`
  - 機能別の設計/分析/中長期方向文書
- `docs/developer-extension-guide.ja.md`
  - 現在コード基準の保守/拡張作業手順文書

## Translation Coverage
- ルート `README.md` には [`README.en.md`](../README.en.md)、[`README.zh-CN.md`](../README.zh-CN.md)、[`README.ja.md`](../README.ja.md) があります。
- `docs/` 内の主要文書は `.en.md`、`.zh-CN.md`、`.ja.md` のサフィックスで並行翻訳を持ちます。
- 翻訳文書と韓国語原文が食い違う場合は、現在のコード、その次に `docs/current-state.md`、その次に `docs/config.md` を優先します。

## Acknowledgements
- OpenAI Codex、ChatGPT、Claude Code、Gemini、そして検証に協力してくださったゲストテスターの皆さんに感謝します。

## Practical Rule
- 現在の挙動を確認するときは `docs/current-state.ja.md` から見始めます。
- どの基準の上に作業を積み上げるか判断するときは `docs/baseline-1.1.2.ja.md` を一緒に読みます。
- 古い設計文書と現在のコードは食い違うことがあるため、衝突したら現在のコード、`docs/current-state.ja.md`、`docs/config.ja.md` の順で解釈します。
