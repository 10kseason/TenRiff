# Main Menu Low-Latency Blueprint

main menu も gameplay と同じ低遅延思想に従う必要があります。audio は master clock として動き、入力は別スレッドで timestamp を付け、render は snapshot だけを消費します。この blueprint は、menu 作業が input lag を再導入しないようにするためのルールと実装順序をまとめたものです。

## Current Implementation State (Windows Menu UI)
- `MenuApp` は **InputThread (polling)** -> **SPSC queue** -> **menu state machine** -> **RenderThread (D3D11 window render)** で動く。
- `SongIndexerThread` はバックグラウンドで song index を構築し、`profiles/<name>/.tenriff/song-index/<source-hash>.json` にキャッシュする。
- menu で audio / graphics / input / mode settings を変えると profile config file に保存される。
- play 開始時、現行実装は menu thread を止めて `GameSession` を別実行する。
- **Windows menu UI は D3D11 + Direct2D / DirectWrite** ベースで、Title / Song Select と各種 settings screen を描画する。
- Input summary:
  - Title: `↑ / ↓` move, `Enter` select (`PLAY / EDIT / OPTIONS / EXIT`), `Esc` quit
  - Song Select: `↑ / ↓` song movement, `← / →` left menu focus 切り替え, `Enter` select / play, `Esc` back
  - Settings / Mode: `↑ / ↓` item 移動, `← / →` 値変更, `Enter / Esc` で戻る
  - Keymap: `↑ / ↓` select, `Enter` capture binding, `Esc` return
  - Result: `Enter` で Song Select に戻る
  - Shared utility keys: `F1` help, `F2` songs-folder browse, `F5` refresh / reindex, `F9` screenshot

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
- **SongIndexerThread** は path / title / artist / BPM / key count / mode / preview audio をスキャンする。進捗更新は UI に投稿し、操作性は維持する。
- **Cache index**（`song_index.json` または SQLite）を mtime / hash と合わせて使い、毎回 full rescan しない。初回は遅くてよいが、次回以降は即時性を目指す。
- **Preview audio** は audio engine 経由でスケジュールする。UI は preview request を enqueue し、AudioThread が mix して timing を合わせる。
- empty-state 画面には常設の `Add Songs Folder` action を置き、drag-and-drop は補助扱いにする。

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
