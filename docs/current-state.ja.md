# TenRiff Current State

この文書は、次のエージェントや新しい作業者が最初に読むべき current-state 文書です。目的は、「このプロジェクトは今どういう状態で、どこを見ればよく、何がまだ未検証か」を素早く把握できるようにすることです。

## Baseline
- 現在のプロジェクト版は `1.1.4 stable`
- direct-IP multiplayer と preview r5 の input-backend lifecycle 修正は `1.1.4 stable` に統合
- 後続作業の基準文書は `docs/baseline-1.1.2.ja.md`
- Windows GUI ビルドが主対象
- Linux は `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview` レベルの preview のみ
- 既定サーフェスは BMS-first
- `.osu` はオプションで再有効化でき、4K-10K をサポート
- `1.1.4 stable` の gameplay 入力は RawInput を優先しつつ、同じ `InputThread` で bound-key polling shadow を常時動作させる。起動失敗または message pump の予期しない終了時も queue / pressed state を reset せず、その producer を Polling に切り替える
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
  - BMS / OSU / All filtering
  - difficulty / title sorting
- osu!mania:
  - 4K-10K の load / launch 対応
  - キーモードごとの separate keymaps
  - 4K-10K chart difficulty calculation
  - `mode.key_mode` は N2NC スタイルの lane remap でキー数を変換
  - `mode.key_mode=none` は元のキー数と基本パターンレイアウトを維持
- Skins / gameplay feel:
  - `rect` / `circle` note shape
  - note border on/off
  - combo Y adjustment
  - judge line / lane width / lane spacing / note width / divider width / 16K center gap / note height / LN body width adjustment
  - キーモードごとの lane-width 配列と inter-lane spacing 配列が保存され、preview / live gameplay / ghost field の同じレイアウト計算に適用される
  - osu!mania `ColumnLineWidth` を読み込んで lane divider 幅へ反映
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
  - gauge mode は `EX-Hard / Hard / Normal / Easy` をサポートし、すべて `100%` で開始して `0%` で即失敗する
  - live gameplay の `ClockSync` は大きな Windows QPC 絶対値ではなく centered anchor regression を使い、継続する clock discontinuity 後に自動 rebase する
  - stale backlog は QPC event age と `BAD` window で判定し、fresh input の sample mapping が現在の playback anchor から大きくずれた場合は anchor に fallback する
  - tail release timing は osu hold と BMS `#LNMODE 2` charge note のみに適用
  - 2 台のキーボードが同じキーを押しても、最後の入力ソースが離すまで論理 `Pressed` は維持される
- Graphics:
  - resolution presets (`720p`, `1080p`, `qhd`, `native`)
  - `refresh_hz` (`60..1050`, default `300`)
  - VSync off: menu effective cap `300`, gameplay は設定値を `1050` まで使用可能
  - VSync on: present refresh は active monitor Hz に追従し、render pacing は `monitor_hz * 2` を目標にする（`1050` clamp）
  - `visual_offset_ms`
  - `performance_overlay`
- Gameplay performance:
  - static playfield command-list cache
  - note head / tail bitmap cache
  - fixed-size HUD note transport
- Loading UX:
  - Song Select indexing progress 表示
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
  - `version = 8`
  - `include_osu` を含む
  - optional `layout_label`

## Runtime / Packaging Rules
- 新しい user profile は自動生成される
- 現在の正式 P2P 配布ラインは `TenRiff 1.1.4 stable`
- distribution package には `Songs` を含めない
- distribution package には menu BGM 用の `Mainmusic/` runtime asset を含める
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
  - BMS-first default
  - keysound policy
  - osu key-mode mismatch など

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
- 4K-10K `.osu` の separate keymap 確認
- Linux はまだ実行可能ビルドではない

## Best Next Read
- runtime / config なら `docs/config.ja.md`
- menu / indexing / state machine なら `docs/menu.ja.md`
- play loop / audio / judgement なら `docs/core-loop.ja.md`
