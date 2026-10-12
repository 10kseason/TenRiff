# TenRiff Development Roadmap (staged)

**固定基準: [1.8.7](baseline-1.8.7.ja.md)**（2026-10-10）。Git tag `1.8.7`、commit `6b060ad47b950ad9e0dbc516fbb40189e8e21347` を使い、`latest` や新しいリリースに合わせて自動変更しません。

**2026-10-13 · 1.8.9.1:** 連続キー設定、全変換器のキー音・ロングノート保持、FAST/SLOW・タイミング背景切替、ゲージ拡大、プレビューのギア移動と判定文字の枠・背景除去を追加しました。既存リプレイは元の変換を維持します。[動作・検証・互換性の制限（韓国語）](release-1.8.9.1-gate.md)。固定基準は1.8.7です。

> 文書の範囲: 初期設計・改善案・当時の分析を含みます。未完了の提案や過去の測定値は現在の挙動・既定値・性能保証ではありません。[現在の状態](current-state.ja.md)と[設定](config.ja.md)を参照してください。

この roadmap は、スコープ肥大化を避けながらゲームループを拡張していくための推奨高位順序をまとめたものです。各段階でまず方向を固定し、その上に追加機能を積み重ねます。

## Current Baseline
- Windows GUI / runtime が主なサポート経路です。
- 固定基準は `1.8.7`、このソースは `1.8.9.1`。公開 package は NK3 P64 と generalized pattern MLP inference model を同梱するが、BGA upscaler model は同梱しない。互換性と権利を確認済みの upscaler ONNX を選択しても path を保存するだけで、BGA Upscaler は user が有効化して high-spec warning を確認するまで off のまま。自動 benchmark gate はない。
- 現行 menu / runtime は BMS family（`.bms/.bme/.bml/.pms`）専用で、native、bundle/profile の TenRiff `skin.json`、LR2 skin をサポート。
- 現在出荷されている挙動は [`docs/current-state.ja.md`](current-state.ja.md) を先に見てください。この roadmap は方向と残作業を示す文書です。

## 1.8.8–1.8.9 分析と 1.9.0 安定化計画（2026-10-12）

2026-10-12時点のソースは **1.8.9のリリースライン**です。1.8.8の分析・解像度・LN Mix・キー音・開始操作を基に、ロビー・HUD・結果画面の可読性と、新しいセッションへのJudge Easy/Hard選択の受け渡しを実装しました。完了したローカル検査・性能測定の結果と制限、および残るパッケージ・CI・公開工程を区別します。確認済みの動作と新しい検証・公開状況は[1.8.9リリース検証](release-1.8.9-gate.md)に記録します。プラグイン実行環境・API・SDKは未実装で、`1.9.0`は将来の安定化目標です。固定基準`1.8.7`、既存タグ・配布ファイルと1.8.8の記録を保持します。下記の初期段階一覧は未完了の本体機能追加を自動的に承認しません。

| 段階 | 完了条件 |
| --- | --- |
| 1.8.8 | レーン別FAST/SLOW・時間帯精度・単ノート/LNイベント・分布と標本数、自由サイズ・結果PNG・長いLN Mix・表示・メニュー入力・キー音保全・ソロ開始/再試行を[リリース条件](release-1.8.8-gate.md)で検証 |
| 1.8.9 | S+を維持したロビー・Studio HUD・結果UIとJudge選択の受け渡しは実装済み。[新しいリリース条件](release-1.8.9-gate.md)の完了したローカル検査・性能の制限と残るパッケージ・CI・公開工程を区別。極端なフィールドと性能表示で上部余白がなくなる場合の情報非表示も確認 |
| 1.9.0 より前の試験版 | 限定的な読み取り専用 API で、別の結果分析画面と個人用選曲フィルターを本体ソース変更なしに実装・検証 |
| 1.9.0 安定化後 | 本体の機能範囲を維持し、バグ・セキュリティ・互換性修正とプラグイン拡張を中心に保守 |

- **分析原則:** 適用中の判定規則で意味のある LN 項目だけを表示し、分析のために判定・点数・コンボ・ゲージ規則を変更しません。通常 LN の自動完了は離鍵タイミングの標本ではありません。測定不能のミスは別集計とし、`0 ms` の標本に含めません。平均とともに分布・標本数・ばらつきを示し、`−20 ms / +20 ms` が相殺されても正確な打鍵と誤解されないようにします。[分析基準と詳細計画（韓国語）](result-analysis-1.8.8.ko.md)を参照してください。
- **本体とプラグインの境界（計画）:** 入力、オーディオ時計、判定、点数、公式記録の保存・検証、マルチプレイ規則・セッション状態は本体が所有します。最初の API は読み取り専用データ、個別の設定・分析結果保存、許可された結果画面・選曲順序/フィルター・HUD 領域とメニュー拡張に限定します。
- **実行の隔離（計画）:** 外部コードは別の実行環境で処理します。ゲームは計算完了を待たず、入力・オーディオ経路から直接呼び出しません。権限制限と実行量/時間制限をそれぞれ設け、異常終了・無限ループ・過大な出力を隔離します。実行技術と具体的な上限は試験版の検証後に決定します。
- **マルチプレイ（計画）:** 本体が外部プラグインの実行と結果適用を遮断し、影響を受けたプレイ状態を既定規則で再構成します。プラグイン自身の停止に依存せず、個人のキーマップ・オーディオオフセットは維持します。将来、シングルで判定・譜面・入力を変更する拡張を許可する場合、その記録を既定の記録から分離します。
- **API 固定条件（計画）:** 上記の二つのサンプルを作るたびにエンジン変更が必要なら、API はまだ固定しません。API 文書、すぐ実行できるサンプル、エラーログ、プラグインを読み込まない安全起動を小さな SDK にまとめ、利用者と AI が API とサンプルからモードを作れることを検証します。
- **バージョン方針（計画）:** `1.9.0` は機能範囲と互換性の安定化であり、数字やファイルの永久凍結ではありません。本体修正版、プラグイン、公開 API の互換性バージョンを区別し、既存モード・記録の互換性を検証します。将来の固定基準変更には別途承認と文書同期が必要です。

## 0) 骨格と master clock を固める
- すべての時間敏感処理で **AudioThread を master clock** として扱う。
- **InputThread** は RawInput / evdev から event に timestamp を付け、SPSC queue に積む。
- 譜面 timeline は **sample positions (int64)** へ正規化し、audio thread が決定的 timestamp を消費できるようにする。
- **Render** は snapshot を描くだけに限定し、judgement / score は audio 側で確定させる。

## 0.5) 低遅延ループを強化する
- 1 つの backend（WASAPI / ALSA）を end-to-end で仕上げ、device padding を露出させ、`buffer_start_samples` を明示計算して mixer が playback-buffer domain で動くようにする。
- ClockSync は外れ値除去、sliding-window EMA 更新、device 変更 / underrun reset hook、monotonic clamp で強化する。
- AudioThread では入力を取り出して sample time に変換し、late / normal / future に分岐して keysound を配置する。
- 判定 window を sample で表現し、callback budget / late inputs / xruns の HUD カウンタを出し、rate 可変時は window を適切にスケーリングする。
- ✅ replay を sample-time input trace（lane / state / sample）として保存し、決定的再現のための JSON export を書き出す。

## 1) 1 曲を end-to-end で遊べるようにする
- menu からも audio backend を silent callback 付きで動かし、gameplay 前から master clock を安定させる。
- ✅ UI state machine（console）として **Title -> Song Select -> Play -> Result** を InputThread / SPSC とともに立ち上げた。
- ✅ Windows D3D11 menu UI（text + background + focus styling）を追加した。
- ✅ async SongIndexerThread と cached index（mtime / hash）を追加し、Song Select がスキャン中も応答するようにした。
- 最小 BMS loader（essential channel のみ）-> note scheduling -> judgement -> result screen を通す。
- preview audio は audio engine で予約し、Song Select 中に keysound を preload する。

## 1.5) BMS-only chart support の強化
- 以前の multi-format 方針は現行 release line では superseded。loading、indexing、replay、result、difficulty-table は BMS family に集中する。
- archive や別 format の import path を再導入せず、real-pack の encoding、keysound、BGA、long note、lane layout 互換性を強化する。
- optional integration は user-supplied、明示的に有効化するまで disabled、失敗時は native behavior へ安全に復帰できる形を維持する。

## 2) Key remap と 8K / 10K modes
- ✅ 「リマップ UI フロー」仕様どおりの key remapping UI（NKRO test を含む）。
- ここまで来ると、プロジェクトは十分実用的な個人練習ツールになる。

## 3) Lane transform / Random mode
- ✅ **Mirror**、**Full Random (FR)**、**Super Random (SR)** は実装済み。**AR** は挙動を定義するまで後回し。

## 4) Launcher を付ける
- folder check、初回 config 生成、error code 整理を扱う。
- これが終わると、ゲームはローカル PC 上で自己完結する。

## 5) 公開 ranking より先に信頼できる記録を作る
- [`ranked-integrity-plan.en.md`](ranked-integrity-plan.en.md) に従い、共通 eligibility reason、Local Records 画面、read-only Online Records、shadow submission、公開 verified ranking の順で進める。
- server は承認済み BMS chart SHA-256 と replay evidence から結果を再計算し、client の score claim を信頼境界にしない。
- `.osu` chart と osu 派生 import / ruleset / scoring / conversion は ranked 登録不可。native BMS の補助 OD8 統計は local metadata のみ。

## 6) 規模拡大に合わせて menu architecture を refactor する
- `1.6.0` で Phase 0-6 を完了した。明示的 navigation、全 settings family の typed controller、exhaustive screen descriptor が現在の保守対象 architecture となる。
- [`menu-refactor-plan.md`](menu-refactor-plan.md) の段階設計と phase gate に従い、menu 全体の一括 rewrite から始めない。
- 新しい screen の追加で input dispatcher、render-data builder、複数の共有 cursor/return flag を同時に変更する必要が生じるか、screen behavior を focused unit test で覆えなくなった時点で着手する。
- 先に navigation、settings persistence、pointer/keyboard parity の characterization test を追加し、visible behavior を変えずに `MenuApp` から screen family ごとに local state/controller へ移す。
- 共有 return-screen flag は明示的な navigation stack/back policy に置き換え、slider を含む adjustable row は row index 条件ではなく typed value range と persistence callback で表現する。
- render DTO 構築と input/state mutation を分離する。各 extraction は focused test と通常の client build を通してから次へ進み、menu 全体の一括 rewrite は行わない。
