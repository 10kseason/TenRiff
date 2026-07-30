# TenRiff Current State

この文書は、次のエージェントや新しい作業者が最初に読むべき current-state 文書です。目的は、「このプロジェクトは今どういう状態で、どこを見ればよく、何がまだ未検証か」を素早く把握できるようにすることです。

## Baseline
- 現在のプロジェクト版と公開 stable 版は `1.2.8 stable`
- direct-IP multiplayer と preview r5 の input-backend lifecycle 修正は `1.1.8 stable` に統合
- `1.1.8` は 1.1.7 の visual refresh に osu!mania OD8 補助スコア、最初の native `BAD` で終了する `Sudden Death (1 MISS)`、決定的な `LN Mix 10%～90%` を追加
- `1.2.0` は BMS channel `04/07` と osu!mania 背景を gameplay sample timeline に接続し、FHD 未満の画像背景を Windows ML 上の LunaSR で非同期補間
- `1.2.1` は LunaSR `basic_v2` へ切り替え、gameplay BGA と Song Select の選択 BGI を補間し、single-player の Esc pause menu、polygon note shape、LN tail cap toggle を追加します。
- `1.2.2` は procedural 円・多角形 skin を bar と同じ 100% 幅に補正し、FFmpeg fallback 付き MPG/MPEG 動画 BGA を追加し、LunaSR を必須 200 FPS benchmark gate 付き staged32 RGB FP16 model に切り替えます。
- `1.2.3` は当時の LunaSR 固定 RGB x2 performance gate を緩和。
- `1.2.4` は権利範囲が不明確な LunaSR ONNX と model 固有 metadata を公開配布から除外し、既定値 `off` の user-supplied model opt-in integration だけを残します。
- `1.2.5` は model 固有名の integration を generic External ONNX Upscaler に置換し、Graphics Settings の file 選択/.onnx drop、profile 別 model path、model 別 WinML session を追加。
- `1.2.6` は playable chart を BMS family のみにし、native/LR2 skin だけを維持。ONNX upscaler の手動有効化と実験的 NPU 優先、Song Select の Rate 調整、中央 indexing progress、local JSON 難易度表、BMS keysound late-input hotfix を追加。
- `1.2.7` は External ONNX Upscaler の FP16 binding、float boundary INT8 QDQ 検出、high-performance DirectX GPU default、動画 BGA one-in-flight backpressure を修正。
- `1.2.8` は LN 全体の一瞬の点滅修正、gameplay BGA toggle、BMSTable HTML/header link import、長い settings 画面の scrollbar UX 改善をまとめた配布前候補。
- 後続作業の基準文書は `docs/baseline-1.1.2.ja.md`
- Windows GUI ビルドが主対象
- Linux は `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview` レベルの preview のみ
- 対応 chart surface は BMS family（`.bms/.bme/.bml/.pms`）のみ
- `1.2.4 stable` の gameplay 入力は RawInput を優先しつつ、同じ `InputThread` で bound-key polling shadow を常時動作させる。起動失敗または message pump の予期しない終了時も queue / pressed state を reset せず、その producer を Polling に切り替える
- menu 入力は従来の foreground process/root-window 境界を維持する。RawInput の起動失敗、process-global 登録先の消失、hidden message window の終了を検知すると、ユーザー入力を待たず Polling に切り替える。
- 確認済み fallback は profile を書き換えず、そのアプリ実行中の menu と後続 gameplay に維持する。アプリ再起動または `Options -> Input Settings -> Backend` の明示変更で再試行する。

## Core Architecture
- `MenuApp`
  - menu state machine の中心
  - Song Select、Options、Keymap、Result、Gameplay 起動への遷移を管理
  - 最近の保守リファクタで Song Select の record/keymap/render/state 境界が専用 `.cpp` に分離された
  - open-source source package でもローカル `10k-calc` なしでコアテストが動くよう、optional な Python-reference チェックは skip 可能
- `SongIndexerThread`
  - 譜面インデックス専用のバックグラウンドスレッド
  - Song Select に進行状況を送る
- `AudioThread`
  - オーディオのマスタークロックとミキシングを担当
- `InputThread`
  - RawInput / polling 入力を収集し、キューへ渡す
  - gameplay は RawInput と bound-key polling shadow を単一の `InputThread` state tracker で dedupeし、`GameSession` では logical edge を source 単位で再フィルタリングしない
- `RenderThread` + `MenuWindow`
  - D3D11 + Direct2D/DirectWrite ベースの menu / gameplay HUD レンダリング
  - 最近の保守リファクタでは大きな実装ファイルを細分化している
- `GameSession`
  - 譜面ロード、gameplay audio prep、HUD snapshot、gameplay 実行境界を担当

## What Works Now
- BMS parser / normalizer / timeline パイプラインは実パック互換性重視で強化済み
- BMS explicit key headers:
  - `#4K`
  - `#6K`
  - `#8K`
  - `5+1 SP`
  - `7+1 SP`
  - header があるか SP パターンが検出されると、そのキー数に合わせて compact lane mapping を適用
- BMS keysound:
  - `follow`
  - `autoplay`
  - `ignore`
  - 遅れて届いた real-time input も元の sample timestamp で判定し、可聴 keysound の開始だけを現在の writable-buffer 境界に固定
- BMS long notes:
  - LN channels (`51`-`55`, `61`-`65`)
  - `#LNOBJ`
  - `#LNMODE 2` charge notes は tail release timing 判定を使用
  - 通常 BMS LN は最後まで保持すると自動クリアされる
- BMS audio decode:
  - WAV native first
  - OGG / MP3 は Windows Media Foundation fallback
  - MF に失敗した場合は `ffmpeg.exe` fallback
- Song Select:
  - cache-first loading
  - `F5` forced reindexing
  - mouse-wheel navigation
  - 左側 `KEY` quick filter toggle
  - external folder / BMS drag-and-drop
  - recent source の保存と再オープン
  - difficulty / title sorting
  - search 入力中でなければ `-`/`+` で現在の play Rate を調整
  - indexing 中は stage、percentage、processed/total、ETA、song count、progress bar を header 下部中央に表示
  - Browse で local header JSON を選択するか clipboard の http(s) BMSTable HTML/header link を profile cache へ import し、MD5/SHA-256 一致から level/symbol を表示。変更時は再インデックス
- BMS key mode:
  - キーモードごとの separate keymaps
  - 対応 key count の chart difficulty calculation
  - `mode.key_mode` は N2NC スタイルの lane remap でキー数を変換
  - `mode.key_mode=none` は元のキー数と基本パターンレイアウトを維持
- Native difficulty:
  - BMS の LV/CR 計算では long-note Head/Tail の miss-ms だけを 0.5倍で評価し、`300ms`を`150ms`として緩和する。実際の gameplay 判定 window は変更しない
- Lane transform:
  - Random は `Off / Mirror / FR / SR` に対応。Mirror は key-mode 変換後の最終 lane を反転し、10K/16K は各 player half 内で独立して反転
  - Mod Manager の `LN Mix 10%～90%` は既存 hold を維持し、同じ lane の既存 span と重なる head を除外する。base BPM 基準の 1/8-note hold が次の同一 lane note より 50ms 以上前に終わる tap から設定割合を `Random Seed` で選び、すべての Mix 段階で長さを 1/16-note 70% / 1/8-note 20% / 1/24・1/32-note 10% に配分する
- Skins / gameplay feel:
  - `rect / triangle / pentagon / hexagon / circle` note shape。procedural 円・多角形は 100% で rect bar と同じ全幅を使用
  - note border on/off
  - combo Y adjustment
  - judge line / lane width / lane spacing / note width / divider width / 16K center gap / note height / LN body width adjustment
  - キーモードごとの lane-width 配列と inter-lane spacing 配列が保存され、preview / live gameplay / ghost field の同じレイアウト計算に適用される
  - 対応 skin route は `native` と LR2 playskin のみ。Skins で LR2 folder を選択または drop すると active profile に取り込む
  - LR2 note/LN image、lane gap、destination size を gameplay layout に反映
  - `skin.lr2_resolution_mode` は `auto / sd / hd / fhd` を保持
  - LR2 auto-detect は asset 名ではなく playskin `#DST_NOTE` の座標範囲を使う
  - フィールド上端からの future-note entry easing
  - 最後の判定ノート処理直後に gameplay が終了
- Judge:
  - 既定 `GOOD` window は `75ms`
  - 既定 `BAD` window は `340ms`
  - 同一 lane の pending note がすでに `BAD` で、直後の note が明確に `GOOD` 以上なら、pending note を miss として記録し、現在の press を次の note に割り当てて連続 `BAD` lock を防ぐ
  - note-consuming failure（auto-miss、早すぎる消費、hold break / tail miss）は `BAD`
  - かなり早い non-consuming press は LR2 スタイル `POOR` として扱われ、result / replay / UI に再表示される
  - `POOR` は combo を切らず、score / accuracy には入らず、専用 `PR` gauge damage を使う
  - gauge mode は `EX-Hard / Hard / Normal / Easy / Gauge Shift` をサポートする。固定 gauge は `100%` で開始し、`0%` で即失敗して type は変化しない
  - `Gauge Shift` は EX-Hard / Hard / Normal / Easy をそれぞれ 100% から独立して並列計算し、現在の tier が 0% で脱落すると同じ判定履歴を累積した次の生存 tier を選び、終了時の最上位生存 tier で確定する
  - `Sudden Death (1 MISS)` は最初の OD8 換算 object `MISS` で即失敗する。native `BAD` timing だけでは発動せず、空打ちの `POOR` も無視し、Practice No-Fail とは排他的に動作する
  - Gameplay / Result に実入力 timing を osu!mania stable OD8 window と ScoreV1（最大 1,000,000）で換算した補助 `OSU OD8` score を表示し、TenRiff native score / ranking は変更しない
  - native score は judgement 90,000 点 + 累積 combo 10,000 点で正規化され、全 PG の full combo は正確に 100,000 点。LN head / tail は各 0.5 weight で 1 object を構成する
  - accuracy は PG/GR/GD/BD の基準 100/80/50/20% から各 judgement band 内 timing に応じて最大 0.5 percentage point を減算し、PG timing span が 8ms を超える全 PG run は 99.5% 上限となる
  - rank 境界は `<75 F / 75 B / 80.5 A / 86.5 A+ / 90 S / 95.5 S+ / 98 AA / 99 SS / 99.75 SSS`
  - live gameplay の `ClockSync` は大きな Windows QPC 絶対値ではなく centered anchor regression を使い、継続する clock discontinuity 後に自動 rebase する
  - stale backlog は QPC event age と `BAD` window で判定し、fresh input の sample mapping が現在の playback anchor から大きくずれた場合は anchor に fallback する
  - tail release timing は BMS `#LNMODE 2` charge note のみに適用
  - 2 台のキーボードが同じキーを押しても、最後の入力ソースが離すまで論理 `Pressed` は維持される
- Graphics:
  - resolution presets (`720p`, `1080p`, `qhd`, `native`)
  - `refresh_hz` (`60..1050`, default `300`)
  - VSync off: menu effective cap `300`, gameplay は設定値を `1050` まで使用可能
  - VSync on: present refresh は active monitor Hz に追従し、render pacing は `monitor_hz * 2` を目標にする（`1050` clamp）
  - `visual_offset_ms`
  - `performance_overlay`
  - `bga_enabled=false` は gameplay image/video BGA と decoder/upscaler 処理を無効化し、Song Select preview は維持
  - `background_upscale_model_path` は Graphics Settings で選択/drop した互換 ONNX の path だけを保存。公開 model は同梱しない
  - BGA Upscaler は既定 `off`。user が明示的に on にして high-spec warning を確認する必要があり、自動 benchmark gate はない
  - 現在の契約は 960x540 RGB residual x2 で FP32/FP16 boundary と float boundary INT8 QDQ metadata を自動検出。model の権利・品質・性能は user が確認し、load・contract・decode・inference 失敗時は native scaling
  - default は high-performance DirectX GPU。実験的 `background_upscale_prefer_npu=true` opt-in は upscaler on 時だけ WinML `DirectXMinPower` session を先に要求する。実際の NPU/GPU は Windows/driver が選択し、作成・評価失敗時は high-performance DirectX と通常 DirectX fallback を使用
- Gameplay performance:
  - static playfield command-list cache
  - note head / tail bitmap cache
  - fixed-size HUD note transport
- Loading UX:
  - Song Select header 下部中央に indexing stage/percentage/processed/total/ETA/song count と progress bar を表示
  - gameplay chart-loading progress 表示
  - gameplay loading 中の `Esc` cancel
- Profile UX:
  - `Options -> Profile Setup` から現在の profile の初回 setup 画面を開き直し、language / audio / input / graphics / keymap を即時保存できる
- Direct-IP multiplayer:
  - joiner は active source と `recent_song_sources` の既存 profile-local cache だけを対象に、host chart の hash + size を照合する
  - 全 disk scan や自動 rescan は行わず、source root 外を指す cache path は拒否する

## Song Indexing Model
- song source が変わると、まず `profiles/<name>/.tenriff/song-index/<source-hash>.json` の profile-local cache を読む
- cache が無いか無効ならバックグラウンドインデックスを開始
- Indexing profiles:
  - `safe` が既定
  - `fast` が任意選択
  - Mode Settings の `Indexing` 行と `config.mode.song_index_profile` で制御
- Indexing stages:
  - `SCANNING FILES`
  - `BUILDING METADATA`
  - `WRITING CACHE`
- 大規模ライブラリ向けメモリ強化:
  - two-pass enumerate + small batch metadata build
  - `safe` profile は 1-worker 寄りの budget と頻繁な heap trim で RAM high-water を抑える
  - indexing 用 BMS parse は asset map / 不要 header / 非必須 command を省く低メモリ経路を使用
  - cache save は giant JSON tree ではなく streaming write
- 実測:
  - 46k-chart Windows benchmark library の safe full-index で `46,636` candidates / `46,602` indexed entries
  - peak memory はおよそ `working set 453MB`, `private 524MB`
  - 同ライブラリの 1024-chart sample では fast profile throughput が safe 比で約 `2.05x`
- Cache schema:
  - `version = 11`
  - optional `layout_label`
  - `native_level`、`md5`、`sha256`、difficulty-table name/symbol/level/order metadata

## Runtime / Packaging Rules
- 新しい user profile は自動生成される
- 現在の正式 P2P 配布ラインは `TenRiff 1.2.8 stable`
- distribution package には `Songs` を含めない
- distribution package には `Main Menu / Options / Song Selecte / Multiplayer Lobby / Clear / Failed` の `Mainmusic/` scene slot を含め、各 `Name.mp3` と `Name 2.mp3`～`Name 64.mp3` を自動検出して scene 再入場ごとに循環する
- distribution 更新には built artifact と必要な runtime asset だけを含める
- source-only / public handoff 前に include/exclude リストを確定する
- preview source branch と tag は stable release と分離して管理する
- public source package 更新時は docs/files を揃えるだけで終えず、その staged folder 自体で standalone configure/build/test が通ることも確認する

## Config / Profile Reality
- 実際の既定値は `config/config.json` にある
- runtime profile は `profiles/<name>/config.json`
- keymap は `profiles/<name>/keymap.json`
- `keymap.json` は `modes.{4k..10k}` の per-mode binding 構造
- 古い profile は runtime migration により一部補正される
  - keysound policy
  - 削除済み osu field は保存しない

## High-Value Files
- `src/app/MenuApp.cpp`
- `src/app/GameSession.cpp`
- `src/app/SongIndex.cpp`
- `src/app/SongIndexerThread.cpp`
- `src/render/MenuWindow.cpp`
- `src/render/RenderThread.cpp`
- `src/app/ChartLoader.cpp`
- `src/chart/BmsParser.cpp`
- `src/config/Config.*`
- `src/config/Keymap.*`

## Validated Commands
- `cmake --build build --config Release --target tenriff`
- `cmake --build build --config Release --target bms_parser_tests`
- `cmake --build build --config Release --target bms_realworld_smoke`
- `ctest --test-dir build -C Release --output-on-failure -R bms_parser_tests`
- `cmake -S . -B build-check -G "Visual Studio 17 2022" -A x64`
- `cmake --build build-check --config Release --target bms_parser_tests`
- `.\build-check\Release\bms_parser_tests.exe`

## Still Manual-Validation Heavy
- 実際の CJK-heavy library での Song Select fast-scroll crash 再現
- fast profile の長時間 full-index に対する RAM / commit 再確認
- gameplay low-FPS / 0.1% / 0.01% low の確認
- graphics 設定 live-apply 中の OBS / Discord / Game Bar 共存確認
- drag-and-drop / 外部 Korean-path source の GUI 確認
- 実際の NPU 搭載 Windows PC で `NPU 優先（実験）` on/off と WinML device fallback を確認
- local 難易度表の変更後に hash matching、reindex、表示順を GUI 確認
- Linux はまだ実行可能ビルドではない

## Best Next Read
- runtime / config なら `docs/config.ja.md`
- menu / indexing / state machine なら `docs/menu.ja.md`
- play loop / audio / judgement なら `docs/core-loop.ja.md`
