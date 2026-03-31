# TenRiff 1.1.2 Final Stable Baseline

この文書は、今後 TenRiff の作業を積み重ねるときの基準として扱う `1.1.2 final stable` ベースライン文書です。目的は、「何を現在の安定契約とみなし、何を維持し、どの範囲で次の作業を積み上げるべきか」を素早く固定することです。

## Release Identity
- 基準リリースライン: `1.1.2`
- リリース名称: `final stable`
- 現在の主対象: Windows GUI ビルド
- 既定の製品サーフェス: BMS-first
- 任意の拡張サーフェス: `.osu` osu!mania 4K-10K
- 配布基準パス:
  - Windows パッケージ: `Baepoks/TenRiff-1.1.2`
  - 公開ソースパッケージ: `opensource-Tenriff-source/TenRiff-1.1.2-source`
- 配布用ビルドの source of truth: `build-dist/Release`

## Stable Contract
- `1.0.9` で整理した gameplay playback-head 基準の入力タイミング補正は維持対象です。
- gameplay の live 入力キャプチャは安定性優先で `Polling` に固定します。
- gameplay セッションは foreground 状態に関係なく入力を受け続ける always-allow gate を維持します。
- menu 入力は従来どおり foreground process/root-window 境界を維持します。
- 保存済みの `input.backend` / `input.rawinput` は runtime fallback の結果で書き換えず、そのまま保持します。
- Windows 配布パッケージは menu BGM 用の `Mainmusic/` ランタイム資産を含みます。
- 公開ソースパッケージは引き続き `external/llama.cpp/` を除外します。

## Product Base
- menu の入口は `MenuApp` + `MenuWindow` の組み合わせです。
- 既定のユーザーフローは `Title -> Song Select -> Gameplay -> Result` です。
- Song Select は cache-first ロード、検索/ソート/フィルタ、外部フォルダ drag-and-drop、recent source 再オープンを標準で備えます。
- Gameplay では `GameSession` が譜面ロード、gameplay audio prep、HUD snapshot、終了境界を担当します。
- レンダリングは D3D11 + Direct2D/DirectWrite、オーディオは WASAPI、入力は RawInput または高頻度 polling を基本軸とします。

## Baseline Capabilities
- BMS parser/normalizer/timeline は、実用互換性を重視して強化された現行実装を基準とします。
- BMS explicit compact layout（`#4K`, `#6K`, `#8K`）と SP compact layout（`5+1 SP`, `7+1 SP`）は既定機能です。
- BMS long note は LN channel、`#LNOBJ`、`#LNMODE 2` charge tail 判定を含む現行実装を維持します。
- BMS オーディオは WAV native、OGG/MP3 fallback、`ffmpeg.exe` fallback、`follow/autoplay/ignore` keysound モードを基準機能とします。
- Song indexing は既定 `safe` profile と任意 `fast` profile を維持しますが、大規模ライブラリ安定性は `safe` 基準で評価します。
- osu!mania は既定では無効ですが、オプションで有効化した場合の 4K-10K menu/runtime 対応は基準機能です。
- result 画面、replay/result JSON export、local records、ghost 比較フローは既に含まれる基準機能です。

## Baseline Defaults
- 既定判定範囲:
  - `GOOD = 75ms`
  - `BAD = 340ms`
  - `indirect_miss` は現在のランタイムで `BAD` と同じ値に折りたたまれます。
- long note tail 関連の既定値:
  - `hold_grace = 80ms`
  - `hold_break = 200ms`
- gameplay 開始前の `3 / 2 / 1` カウントダウンは基準仕様です。
- result 画面の遷移 tail は `ui.result_tail_ms = 3000ms` 基準です。
- 3 種類の gauge（`Hard`, `Normal`, `Easy`）はすべて `100%` で開始し、`0%` で即失敗します。
- auto-shift gauge は使いません。
- song index の既定 profile は `safe` です。

## Baseline UX And Packaging
- Song Select 左ナビゲーション、検索、ソート、key/chart filter、ページ移動、mouse wheel 移動は維持対象です。
- `F5` 再インデックス、`F1` ヘルプ、`F9` スクリーンショットなどの共通ショートカットは現在 UX 契約の一部です。
- `Skins` で judge line、note サイズ、lane spacing、lane color を調整する現行フローは維持します。
- 配布パッケージは `Songs` を含まない no-songs 構成を維持します。
- 新規ユーザープロファイルは初回起動時に自動生成される方針を維持します。

## Baseline Constraints
- 今後の変更は基本的に `1.1.2 final stable` 契約を壊さない加算型変更を優先します。
- Linux は依然として preview レベルであり、基準判断は Windows GUI 経路を前提にします。
- 古い設計文書より現在のコードと `docs/current-state.md` を優先します。
- リリース/文書/パッケージ規則が変わった場合は baseline 文書と current-state 文書を同時に更新します。

## What To Preserve In Follow-Up Work
- playback-head 基準の gameplay 入力タイミング補正
- gameplay live capture の `Polling` 固定と、保存済み backend 設定保持の分離方針
- BMS-first サーフェス
- large-library `safe` indexing 安定性
- menu の cache-first ロード
- `Mainmusic/` 同梱の no-songs Windows パッケージング
- 公開ソースパッケージでの `external/llama.cpp/` 除外規則

## Recommended Companion Docs
- 現在の実装状態: `docs/current-state.ja.md`
- 設定/プロファイル構造: `docs/config.ja.md`
- 実際のプレイフロー: `docs/gameplay-guide.ja.md`
- 保守/拡張ポイント: `docs/developer-extension-guide.ja.md`
- 中長期方針: `docs/roadmap.ja.md`
