# メニュー構造と操作

現在の説明はクライアント 1.7.1 が基準です。下の折りたたまれた初期設計案は実装済み機能一覧ではありません。

## 現在の画面と操作

- ホーム: `Play / Multiplayer / Options / Exit`。譜面がなければ先頭は `Add Songs Folder`。
- 選曲: 上部 `Songs / Sources / Records / Session Mix / Options`、中央下部 `Search / Sort·Filter / Difficulty Table`。旧左側 KEY/メニューレールは現在の標準配置ではありません。
- 難易度表カード: 名前・URL 部分で編集、File でローカル JSON、Reset で標準 LV。Enter で適用、Esc で取消。不正な URL では現在の表を保持します。Filters の行も同じ取込経路を使用。
- Options: 5列×2行の10カード。Key Mode、Keymap、Skins、Graphics、Audio、Input、Calibration、Profile Setup、Mods、Key Test。
- 共通設定: 方向キーで選択・調整、Enter で項目の操作、Esc/Backspace で戻る。長い案内はページボタンを使用し、スキン設定ではライブプレビューを維持。
- キーマップ: 4K–10K、12K、14K、16K。割当成功時に即保存。
- 結果: 単人の表示演出は Space でスキップ。操作可能後は R/Left で再試行、F1 でリプレイ、Enter/Esc/Backspace で戻る。Session Mix は Enter で次曲、Esc/Backspace で終了。マルチ結果はロビーへ戻ります。
- F1/F2/F5 などは画面ごとに役割が異なります。[プレイ案内](gameplay-guide.ja.md)を参照してください。

## 所有権とデータフロー

- [MenuApp](../src/app/MenuApp.cpp) は InputThread の RawInput/ポーリングキーイベントとウィンドウのポインターイベントを処理。[MenuNavigator](../src/app/menu/MenuNavigator.h) が画面と Back 履歴を所有。
- 型付き設定 controller が選択・変更状態を所有し、MenuApp が保存・ファイル選択・再索引・装置再起動を実行。[MenuScreenDescriptor](../src/app/menu/MenuScreenDescriptor.h) が画面メタデータと view 経路を定義。
- 描画は不変 snapshot を参照。メニュー音楽は [MenuMusicController](../src/app/MenuMusicController.cpp)、選曲プレビュー状態は [SongSelectScreen](../src/app/SongSelectScreen.h) が所有。
- [launch_gameplay](../src/app/MenuAppTail.inl) はメニュー入力・プレビューを止めて GameSession を開始。ゲームのオーディオ・入力ライフサイクルは GameSession が管理します。メニューから同じ音声装置を開き続ける案は現在の契約ではありません。
- SongIndexerThread のキャッシュは `profiles/<name>/.tenriff/song-index/<source-hash>.json`。[現在の状態](current-state.ja.md)と[設定](config.ja.md)を参照。

## 保守

[メニューリファクタ](menu-refactor-plan.md)の Phase 0–6 は完了。既存 controller と明示的な画面経路を拡張し、対象テストと [UI チェックリスト](ui-audit-checklist.md)で確認します。描画の所有権は[メニュー表示](menu-visual-polish.md)と [1.7.1 変更](gameplay-polish-followup.md)を参照。

<details>
<summary>初期設計案 — 現在の実装契約ではありません</summary>

## Non-Negotiable Rules
- **menu から audio device を閉じない。** menu 進入時に audio backend を初期化し、silent callback（zero buffer）を走らせて、gameplay 前から `playhead_samples` / `buffer_start_samples` を有効に保つ。曲開始時の device reopen は warm-up jitter の原因になるので避ける。
- **menu input は InputThread + SPSC のみを使う。** UI action も同じ RawInput / evdev ingest 経路から消費する。render / UI event loop が直接 timestamp を付けてはいけない。
- **audio thread は allocation / I/O / lock free。** menu preview のために audio callback 内へ file I/O、heap allocation、lock を入れない。
- **Render は read-only。** snapshot を消費するだけで、authoritative timing を変えたり input に timestamp を付けたりしない。
- **重い仕事は別スレッドへ逃がす。** folder scan、metadata parse、replay / result save は background job に送って UI thread を止めない。

## State Machine Skeleton
- `TitleState`
- `SongSelectState`
- `GameplayState` (chart playback)
- `ResultState`
- 後で: `SettingsState`, `KeymapState`, `LatencyToolsState`

### Flow
`Title -> SongSelect -> Gameplay -> Result` が最小の playable loop。各遷移は live audio clock を再利用し、InputThread は動かし続けるべき。

## Song Select Without Hitching
- **SongIndexerThread** は BMS-family file の path / title / artist / BPM / key count / mode / preview audio をスキャンする。stage / percent / ETA と progress bar は Song Select header 下部の中央に表示し、操作性は維持する。
- **Cache index**（`song_index.json` または SQLite）を mtime / hash と合わせて使い、毎回 full rescan しない。初回は遅くてよいが、次回以降は即時性を目指す。
- **Preview audio** は audio engine 経由でスケジュールする。UI は preview request を enqueue し、AudioThread が mix して timing を合わせる。
- empty-state 画面には常設の `Add Songs Folder` action を置き、外部 folder と BMS file の drag-and-drop に対応する。
- Browse > Difficulty Table で http(s) BMSTable page/header link を clipboard にコピーして `Enter` を押すと profile cache へ import する。`Right` は local header JSON 選択、`Left` は解除で、変更時は MD5/SHA-256 一致 level を再適用する。
- Song Select の `-` / `+` は、検索文字入力中でない場合に `speed.rate` を即時変更・保存する。

## Settings: Latency-First Surface
- Audio backend (`wasapi / asio`, `alsa / jack`)
- Sample rate（48 kHz 推奨）
- Buffer size（`128 / 192 / 256`）+ optional adaptive step-up
- RawInput / evdev grab toggle（off 時は警告）
- VSYNC off / driver frame-queue guidance
- `input_offset_ms` と独立した `visual_offset_ms`
- HUD toggles（latency overlay / xrun / late counter）

## Key Remap and NKRO Test
- key binding 取得は **次の input event** を InputThread から受けて行い、render loop の polling でブロックしない。
- per-key state machine（UP / DOWN）を維持し、DOWN 中の duplicate DOWN、UP 中の duplicate UP を落とす。実際の down -> up -> down 遷移は保持し、fast tap や release を捨てない。
- key capture 成功時は即保存し、隠れた最終 save chord は設けない。
- NKRO test は見える画面として残すが、隠しショートカットにはしない。
- NKRO test は現在押されている集合を表示し、同じ input event を使って ghosting / missing key をリアルタイムに示す。

## Transition Into Gameplay Without Lag Spikes
1. **Preload stage (in menu):** 譜面を sample position に load / normalize し、keysound を pre-decode / preload する。
2. **Warm start (on entry):** audio がすでに動いている状態で、`buffer_start_samples` 基準の少し未来に `song_start_samples` を予約する。
3. **Start:** その sample time で render / judgement / keysound 経路を接続し、最初の note の体感を固定する。

## Result Screen Hygiene
- 結果は即表示し、replay / log save は background job にして "Saving..." 表示を出す。
- replay は `{lane, state, sample}` を保存し、遅延バグを決定的に再現できるようにする。

## Recommended Implementation Order
1. 既存 pipeline の input / timing を壊さない前提で UI framework（SDL + ImGui あるいは custom）を決める。
2. `Title / SongSelect / Gameplay / Result` の 4 画面と state machine を実装し、end-to-end ナビゲーションを通す。
3. SongIndexerThread + cached index + responsive SongSelect UI を追加する。
4. latency-first settings を前面に出し、可能なら live apply する。backend 変更に再起動が要る場合は明記する。
5. input pipeline ルールに従って key remap + NKRO test を追加する。

</details>
