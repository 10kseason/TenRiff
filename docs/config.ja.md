# TenRiff Config Schema (current)

確認元: [Config.h](../src/config/Config.h)、[Config.cpp](../src/config/Config.cpp)、[既定 JSON](../config/config.json)、[キーマップ](../src/config/Keymap.cpp)。省略値にはコード既定値を使い、以下の範囲は読込・保存時の正規化規則です。

この文書は、`config/config.json`、`profiles/<name>/config.json`、`profiles/<name>/keymap.json` を基準に、現在実際に有効な設定構造をまとめたものです。

## Load Order
1. code defaults
2. global config: `config/config.json`
3. profile config: `profiles/<name>/config.json`
4. CLI
5. menu/runtime save

profile が存在しない場合は初回起動時に自動生成されます。

## `config.json`

### `audio`

- `play_to_end` (bool; default `true`): 最後の判定後、残りの音声を最後まで再生します。falseでは `ui.result_tail_ms` 後に結果へ進みます。判定・スコア・リプレイと手動スキップは変わりません。

- `backend` (string)
  - `wasapi | asio`、既定は `wasapi`。プレイと曲プレビューの出力を選択。メニュー・結果の BGM は独立した Windows MCI 経路を維持。
- `asio_driver` (string)
  - インストール済み64ビット ASIO ドライバーの CLSID。空は Auto（名前順の最初）。Audio 設定で選び、F5 で一覧を更新。
- `rate` (int)
  - 既定 sample rate
- `frames` (int)
  - buffer frames
- `periods` (int)
  - period 数
- `exclusive` (bool)
  - WASAPI exclusive mode を試すか
- `use_mmcss` (bool)
- `affinity` (int)
  - `-1` は既定
- `preset` (string)
  - `basic | high`
- `bms_keysound_policy` (string)
  - `follow | autoplay | ignore`
- `background_sound_enabled` (bool)
  - メニュー・リザルト・曲プレビュー音楽のオン／オフ。プレイ中の譜面 BGM は維持します。
- `title_music` (string)
  - `none | default | random_bms | last_played`、既定は `default`。タイトル・初期設定の音楽を「なし・標準・ランダム BMS・最後にプレイした曲」から選択します。曲の音声を読めない場合は標準に戻ります。BMS はキー音を含め最大 5 分を非同期合成し、繰り返し再生します。
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `normalize_audio` (bool)
  - ゲーム内RMS音量調整。既定false。ONはRMS→ソフトリミッター→マスター、OFFは線形マスター音量→最終出力範囲制限のみ。メニュー音楽と選曲プレビューは変更しません。
- `keysound_volume` (double)

[ASIO 設定](asio-audio.md)参照。選択サンプルレートを固定し譜面音声をリサンプルします。`frames` は要求値でありドライバーの許容サイズに調整します。ASIO では preset は frames を上書きせず、`exclusive` と `periods` は WASAPI 用です。ASIO エラー時の自動 WASAPI 切替はありません。

### `input`

- `backend` (string)
  - `polling | rawinput`
  - 現行 `1.7.2` リリースラインの既定値は `rawinput`
  - `Options -> Input Settings -> Backend` または `Options -> Profile Setup -> Input Backend` で profile ごとに選択可能
  - runtime fallback は保存済みの値を `polling` に書き換えない
  - RawInput の起動失敗、登録先の消失、message window の終了を確認すると、そのアプリ実行中は menu と後続 gameplay の両方で Polling を維持する
  - アプリ再起動または Input Settings で Backend を明示変更すると、選択した backend を再試行する
- `rawinput` (bool)
  - `backend` と一緒に保存される補助フィールド
  - `true` の場合、menu/gameplay は RawInput を優先
  - gameplay は同じ `InputThread` 内で note/control key を bound-key polling shadow により常時監視する
- `use_qpc` (bool)
- `grab` (bool)
  - 現在は Linux preview 向けの設定
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - Polling backend と gameplay polling shadow の sampling 頻度
  - 既定値は `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - input config に残している互換フィールド
  - 現行 runtime はこの値で別の audio-thread judgement sub-step loop を駆動しない
  - 既定値は `4000` (`0.25ms`)
- `debounce_ms` (double)
  - 実際の Press/Release 遷移は維持し、同一状態の重複 event のみ pressed-state tracking から除去する
  - `0..25` に clamp
  - 既定値は `8ms`
### `judge`

[最新の判定幅・RANK表・LN規則（韓国語）](judgement-windows.md)

- BMS `#RANK`: `3/EASY` PG21ms、`2/NORMAL` PG18ms、`1/HARD` PG15ms、`0/VERYHARD` PG8ms。EASYのGR/GD/BADは65/115/210msのまま、他の難度は18/21、15/21、8/21倍に縮小します。省略・未対応値はEASY。Judge Easy/Hard MODはその後に適用し、`hold_grace`・`hold_break`設定と自動ミス期限はRANKでは変更しません。終端PG/GR/GD判定にはRANKを適用します。
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 既定 `pg / gr / gd` は `21ms / 65ms / 115ms`
- 既定 `bd` は `210ms`
- `Judge Easy` は基本判定幅を `1.35x` に拡大: `pg/gr/gd/bd=28.35/87.75/155.25/283.5ms`。`hold_grace`・`hold_break`も同倍率、`mask` は変更しない（例示の数値はBMS RANK EASYの場合）
- `Judge Hard` は PG/GR/GD と `hold_grace`・`hold_break`を維持し、`bd` の上限を `180ms` に制限する。より小さいカスタムBAD幅は広げない
- `indirect_miss` (double, ms)
  - 現在のプロファイルでは `340ms` に保存・正規化し、BADの入力判定幅から独立した自動ミスの期限として使用する
  - 標準のNormal/Easy/Hardは未入力ノートが `340ms` を超えると自動ミス。Normal/EasyはBAD、Hardはコンボを切る間接 `POOR` / OD8 `MISS` を記録する
  - BAD範囲外から自動ミス期限までの遅い入力はBADヒットにせず、前のノートをミスにして次のノートを調べる
- 新しいプレイは `tenriff-native-score-v2-ruleset-3` を記録します。旧ruleset-1/2の再生・ゴースト・検証はPG20ms、RANK無効、以前のLN解放動作を復元します。ruleset-1はEasy1.25x、Hard BAD340ms、自動ミス=BADも維持します。カスタム判定は公式扱いにしません。
- `hold_grace` (double, ms; 既定 `80ms`)
  - 設定互換性と `hold_break` の下限のために保持。現在のnative終端PG/GR境界ではない
- `hold_break` (double, ms; 既定 `200ms`)
  - CN終端を離さず押し続けた場合、終端時刻から自動BADまでの待機時間。Judge Easyでは `270ms`、RANKでは変更しない
  - 内部的には常に `hold_grace` 以上に維持
- LN解放は現在のPG/GR/GD幅で即時判定し、GD範囲外はBAD。通常LNは終端まで押し続けると自動完了。`No LN Release` でも途中解放は無視しない
- `mask` (double, ms; 既定 `30ms`)
  - マスク中のレーンの追加押下を一時的に無視する幅。RANK・Judge MODでは変更しない

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

- 基準 BPM は累積進行時間が最も長いテンポです。同じ BPM の区間を合算し STOP 待機を除外します。Hi-Speed はこの基準に固定し、明示的な `#SCROLL`・停止・逆走効果は維持します。[計算規則](reference-bpm.md)を参照してください。

### `gauge`

Gauge Shift は常に有効です。`mode.gauge` の `ex_hard / hard / normal / easy` は EX から Easy への開始段階です。選択段階と下位段階をそれぞれ 100% から並列計算し、脱落すると次の生存段階へ移ります。対象段階がすべて脱落したときにゲージ失敗となります。旧 `shift` は EX 開始として解釈します。

段位/Session Mix は曲間で引き継ぐ LR2 参照ゲージを使用し、間接ミスを維持して HP 2% 未満で失敗します。通常 LN は完走/解除時に一度だけ反映し、通常の `gauge.delta` は適用しません。[比較と互換範囲](lr2-gauge-audit.ko.md)を参照してください。

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - 既定値は `borderless`。Discord、OBS、Game Bar などの外部 overlay にもこの mode を推奨
  - `windowed` はタイトルバー付き固定サイズウィンドウ
  - `fullscreen` は DXGI exclusive fullscreen のため、現在の Discord Game Overlay は表示されない
- `resolution` (string)
  - `native`、従来の別名 `720p | 1080p | qhd`、または `幅x高さ`（例: `1600x900`, `1366x768`, `1280x800`, `3440x1440`）。各軸は320–8192px。
  - グラフィック設定では一般的なサイズと現在のモニターの表示モードを選択できます。`F5`で一覧を更新。手動指定したサイズも保存・再起動後に保持します。
  - 1920×1080の画面比率を維持し、異なる比率では余白を表示。ウィンドウはタイトルバーとタスクバーの作業領域に収まるよう比例縮小します。
  - スキン設定は実際のゲーム描画を同じ比率で縮小し、フィールド移動、レーン・ノート・ギアのサイズ、判定・コンボ位置とスキンのフォントを反映します。
- `vsync` (bool)
- `refresh_hz` (int)
  - `-1` は `Match Display`、`0` は profile 互換の `Unlimited` 選択値（実際の上限は 1500 FPS）
  - 既定値は `-1`
  - `vsync=false` のときだけ直接 FPS cap として使われる
  - `vsync=false` では menu は実効 `300` cap。gameplay は `-1` で monitor Hz に従い、`0` で 1500 FPS の render pacing を適用
  - `vsync=true` では present refresh は active monitor Hz に従い、render pacing は `monitor_hz * 2` を狙う（`1050` clamp）
- `performance_overlay` (bool)
  - 既定値は `false`。右上を使うため、同じ角に置いた Discord Voice widget と重なる場合がある
  - gameplay frame pacing は成功した DXGI `Present()` 完了時刻の間隔を測定し、HUD update cadence は FPS sample に使用しない
- `bga_enabled` (bool)
  - 既定値は `true`。`false` では gameplay の image/video BGA と decoder/upscaler 処理を無効化
  - Song Select の background preview は別機能なので表示を維持
- `background_upscale_mode` (string)
  - `onnx | off`。旧 `lunasr` 値は互換性のため `onnx` に移行
  - 既定値は `off`。Graphics Settings の `BGA Upscaler` で明示的に ON/OFF
  - ON 時は high-spec 警告の確認が必要で、自動 performance benchmark は実行しない
- `background_upscale_model_path` (string)
  - Graphics Settings の `ONNX Model` で選択するか、その画面に `.onnx` file を drop。選択は path だけを保存し、upscaler を自動で ON にしない
  - 絶対 path または executable/current directory 基準の相対 path。公開 package は model を含まない
  - 現在の契約: float32 または float16 NCHW `rgb_lr [1,3,540,960]` -> `rgb_residual_x2 [1,3,1080,1920]` residual x2。外部 boundary を float のままにする INT8 QDQ model は内部量子化を検出して対応
  - load・contract・inference 失敗時は native scaling を維持
  - model の権利・品質・性能は user が確認。詳細は `tools/onnx_upscaler/README.md`
- `background_upscale_prefer_npu` (bool)
  - 既定値は `false` で、default path は high-performance DirectX GPU を要求
  - Graphics Settings の実験的な `Low-Power DirectX` で `DirectXMinPower` を要求
  - legacy WinML path は NPU を明示選択・検証できないため、この option は NPU 実行の証拠にならない
  - low-power session 作成失敗時は既存の high-performance DirectX 経路へ fallback

### `mode`
chart loader/indexer は BMS family（`.bms/.bme/.bml/.pms`）専用です。旧 `enable_osu_charts` と `format` の値は読み込み時に無視し、再保存しません。

- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
  - `none` は譜面本来の key count をそのまま使う
- `key_conversion_algorithm` (string)
  - `krrcream | nk2 | nk3`
  - ゲーム内の `Mode Settings > Key Converter` で `Krrcream`、`KeyWeaver nK2`、`KeyWeaver NK3 ONNX` を選択
  - 既定値は `krrcream`。NK3 は同じ key count でも remaster を実行し、既定の `AUTO` backend は ncnn Vulkan を優先
  - Krrcream は元 note を target lane へ再配置するだけ
  - nK2 は key count 拡張時、元 pattern へ先に note を追加せず、変換中に target layout へ安全な support note を直接生成
  - NK3 は P64 と host beam safety solver を常に使い、10K 以外の source を 10K に変換するときだけ generalized MLP を追加する。`TENRIFF_NK3_BACKEND=AUTO|VULKAN|NCNN_CPU|OPENVINO` で backend を選び、`AUTO` は ncnn Vulkan を先に試す。複数の Vulkan GPU は `TENRIFF_NK3_VULKAN_DEVICE=<index>` で選択
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`。既定値は `native`
  - nK2 の `Native (12%)` / `Transform (35%)` / `Remaster (65%)` を選択し、Krrcream では設定 row を lock
  - `Remaster` は budget を上げつつ原曲の配置を保ち、LN 区間の support note を同じ長さの LN で埋める
  - 3 つとも上限であり、実際の追加量は原曲の密度と safety window によってさらに低くなる
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
- `random` (string)
  - `off | mirror | rr | frns | sr` (`fr` = `frns`)
- `random_seed` (int)
  - RR/SR、強制 key-mode 変換、LN Mix 対象選択の固定 seed。通常 Random は play ごとに新しい session seed を生成し、実際の値を replay に記録
- `mods` (string array)
  - Note Structure では `full_long_notes`、`ln_mix_10`～`ln_mix_90`、`full_short_notes` のいずれか一つを選択できる
  - LN Mix は base BPM 基準の 1/8-note hold が次の同一 lane note より 50ms 以上前に終わる tap のみを候補にし、選択した hold の長さを長い 1/8-note 60% / 中間 1/16-note 20% / 短い 1/24・1/32-note 20% に配分する
  - 既存 hold は維持され、同じ lane の既存 span と重なる head は除外され、同じ `random_seed` では同じ tap が選択される
- `ghost_battle_enabled` (bool)
  - 既定値は `false`
  - `true` のとき、選択譜面の互換性ある best replay を自動ロードして ghost 比較を行う
  - `false` のとき、通常 gameplay は single-field のまま
- `autoplay_enabled` (bool)
  - QA 用の非競争 automatic play mode
  - `true` のとき playable note 入力を自動処理し、result を `AUTOPLAY` として保存
  - official clear、best score、clear lamp、既定 ghost の対象外だが、local result/replay history は保持
- `practice_no_fail_enabled` (bool)
  - QA assist mode
  - `true` のとき gauge による早期失敗を無効化し、判定と result export は最後まで継続
  - result には `ASSIST` が付く
- `one_miss_fail_enabled` (bool)
  - `true` のとき最初の OD8 換算 object `MISS` で gauge が 0 になり、即座に失敗する
  - native `BAD` timing だけでは発動せず、空打ちの `POOR` も即死条件に含めない
  - Mode Settings で有効にすると `practice_no_fail_enabled` は自動的に無効になる
- `pacemaker_mode` (string)
  - `off | accuracy | score` (default `off`)
  - 精度・スコア目標との差をリアルタイム表示。通常のゲージ失敗・クリア・スコア計算を維持し、目標未達だけではクリアを失敗にしません。他の条件を満たせば最高記録・ランキングの対象
  - Pacemaker を有効にすると Practice と Sudden Death は無効になり、replay playback と multiplayer では強制 off
- `pacemaker_target_accuracy` (double)
  - 0..100、既定90.0。現在の標準Accuracyと目標との差をパーセントポイントで表示
- `pacemaker_target_score` (int)
  - 0..10000、既定8000。判定済みノートの重みに比例する目標ペースと現在スコアとの差を表示
- `auto_scratch_hide_lanes` (bool)
  - 既定false。modsにauto_scratchがある場合、実際のBMS皿レーンを非表示にします。他のキー幅と入力番号は維持
  - `auto_scratch`: 実際のBMS皿だけを自動入力。スコア倍率0%、ASSIST記録で正式な最高記録・ランキングから除外
- `song_index_profile` (string)
  - `safe | fast`
  - `safe` は大規模ライブラリで RAM high-water を抑える既定値
  - `fast` は file hash、preview、difficulty table、native LV/CR を省略する任意の最小 profile
- `calculate_song_index_difficulty` (bool)
  - 既定値は `false`
  - `false` は BMS `#PLAYLEVEL` を menu LV として保持し、CPU 負荷の高い native LV/CR 計算を省略
  - `true` は full `safe` index 中に Revive LV/Circus Rating を計算し、`fast` では常に省略
  - 設定変更時は cache mode を分離し、現在の song source を full reindex

### `ui`

`last_played_chart_path` は最後にプレイを開始した譜面のローカルパスです。既定値は空文字列。リプレイ・エディター練習では更新せず、再起動後も `title_music=last_played` で使用します。

| 項目 | 型・範囲・既定値 | 動作 |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | 表示名。空ならプロファイル ID。空白・制御文字を正規化。 |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | ローカル PNG/JPG アバターのパス。 |
| `language` | `en`, `ko`, `ja`; `en` | UI 言語。不正値は en に正規化。 |
| `menu_font_size` | `normal`, `large`, `extra_large`; `normal` | プロファイル・初回設定のメニュー文字サイズ（100%・115%・130%）。プレイ中の文字サイズはスキン設定を維持。 |
| `all_song_sources` | bool; `false` | 登録フォルダーを統合する ALL SONG を復元。同一チャートパスの重複を除外。 |
| `result_tail_ms` | double; `500` ms | 判定完了後の結果遷移の追加待機時間。`audio.play_to_end=true` の場合は譜面音声の終了時刻も考慮します。 |
| `require_enter_to_exit` | bool; `true` | 読込・保存互換用。現在の Windows 結果入力経路はこの値で自動終了しません。 |
| `show_cursor_in_gameplay` | bool; `true` | ゲーム中のマウスポインター表示。 |
| `active_song_source`, `recent_song_sources` | string / string[] | 現在と最近の曲フォルダー。 |
| `song_sources_initialized` | bool; `false` | 明示的な曲フォルダー追加・除去後に true。最後のフォルダーを除去した空一覧を再起動時に復元しません。[曲管理](library-management.md)参照。 |
| `session_mix_lr2_course_path` | string | 選択した LR2 コースのパス。 |
| `favorite_chart_keys` | string[] | お気に入り譜面の内部識別キー。 |
| `collections` | object: name → string[] | 名前付きコレクションごとの譜面キー。 |
| `song_collection_filter` | string; `all` | 全譜面・お気に入り・コレクションのフィルター。即時保存。 |
| `song_key_filter` | int: `0..16`; `0` | キー数フィルター。0 は全件。UI は 4K–10K、12K、14K、16K。 |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | 難易度の境界。0 はその境界を無効化。 |
| `difficulty_table_path` | string | ローカル header JSON または取得済みキャッシュ。表の選択は safe 索引へ切替。標準 LV はパスと有効なキャッシュの表情報を除去。 |
| `difficulty_table_url` | string | 元の HTTP(S) BMSTable ページ/header URL。meta を解決し header/data をキャッシュ。ローカル JSON 選択時は空にする。 |
| `online_records_server_url` | string | 記録・ランキング・チャット API URL。失敗してもローカルプレイ・記録は維持。 |
| `tenriff_main_server_url` | string | F10 のメイン API URL。既定値は Config.h の `kTenRiffMainApiUrl`。 |
| `private_server_url` | string | F10 の私設 API URL。リモートは HTTPS、localhost のみ HTTP 可。 |
| `account_server_mode` | `main`, `private`; `main` | 最後に選択したアカウントサーバー。 |

難易度表 header は `name`、`symbol`、ローカル相対 `data_url` を使用し、data 項目は `md5` または `sha256` と `level` で照合します。取得内容はプロファイルの `difficulty_tables` キャッシュへ保存します。

標準 LV は `difficulty_table_path` と `difficulty_table_url` を消去し、有効なキャッシュから表情報のみ除去します。全再走査は不要です。既定は BMS `#PLAYLEVEL`、難易度計算を有効にしたキャッシュでは既存の計算 LV を使用します。

### `skin`

現在の `skin` 設定と有効なスキン素材は[移動可能な `.trskin` プリセット](skin-presets.md)として書き出し・読み込みできます。オーディオ機器・キー割当・アカウント・曲フォルダー・タイミング補正は含みません。

これはプロファイル `config.json` の skin 設定です。スキンパッケージの `skin.json` 契約は [スキン形式](skin-format.md)を参照してください。

| 項目 | 型・範囲・既定値 | 動作 |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | スキンの種類。 |
| `tenriff_skin_name`, `lr2_skin_name` | string | 取り込んだスキンのフォルダー名。 |
| `scratch_position` | `left`, `right`; `left` | 7+1 の皿の表示順のみ変更。入力・判定レーンは維持。 |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 の座標解像度。auto はファイル名ではなく `#DST_NOTE` 座標を使用。 |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | メニューで選ぶと視覚設定の組み合わせを再設定。 |
| `note_shape` | `rect`, `circle`, `triangle`, `pentagon`, `hexagon`, `square`, `diamond`, `arrow`; `rect` | 標準図形ノートの形。`hex` は `hexagon` の互換用別名。 |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | 引き伸ばし / 比率を保って内側に収める / 幅を固定して比率から高さを計算。 |
| `preserve_note_image_aspect_ratio` | bool; `false` | 旧版互換。明示的な `note_image_aspect` が優先。stretch 以外は true で保存。 |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | ノート枠、レーン区切り線、判定線をそれぞれ表示。 |
| `note_divider_gap_px` | double: `0..40`; `12` px | ノート片側と区切り線の間隔。0 なら区切り線まで拡張。 |
| `show_gear_boundary_line` | bool; `false` | ギア境界線を表示。 |
| `show_timing_feedback` | bool; `true` | バーとは別にFAST/SLOW文字を表示。 |
| `show_timing_bar` | bool; `true` | タイミングバーを表示。旧設定で省略時は文字のスイッチを継承。 |
| `timing_feedback_override` | bool; `false` | ユーザーが表示を変更した後はプロファイルの設定をスキンより優先。 |
| `timing_text_offset_x`, `timing_bar_offset_x` | double: `-600..600`; `0` | 文字とバーの独立X移動。従来の判定周辺配置に加える1920x1080基準ピクセル。 |
| `timing_text_offset_y`, `timing_bar_offset_y` | double: `-400..400`; `0` | 文字とバーの独立Y移動。正は下、負は上。 |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | LN 尾端キャップ表示とテーパー。判定規則は変更しない。 |
| `judgement_line_glow_enabled` | bool; `true` | 判定線周囲の発光。 |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst の明るさ。0 で無効。 |
| `key_pulse_enabled` | bool; `true` | 旧版 ON/OFF 互換。false または明るさ 0 で無効。 |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | 内蔵 Hit Burst の形。 |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | メニューの書体。default は Segoe UI。ロゴ・ランク・コンボ専用書体は維持。 |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | レーンのキー名表示位置。 |
| `judgement_line_position` | double: `0..1`; `0.82` | 判定線の縦位置比率。 |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | 1920×1080 基準のギア横移動。ギアと ↔ ハンドルが見える範囲に追加制限。 |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | コンボ・判定の独立した Y 位置。旧プロファイルで判定位置がなければ `combo_position` を継承。 |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | 1920×1080 基準のコンボ・判定の独立 X オフセット。 |
| `combo_font_scale`, `judgement_font_scale` | double: `0.50..2`; `1` | コンボ・判定の独立した文字サイズ倍率。スキン設定で50–200%を5%刻みまたはマウススライダーで調整し、取り込んだスキンにもプロファイル値を適用。 |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | レーン背景の不透明度。 |
| `black_playfield_enabled` | bool; `true` | レーン間隔も含めフィールド全体を黒く表示。 |
| `visual_opacity` | double: `0.20..1`; `0.96` | ノート・レセプター・キー名の共通不透明度倍率。 |
| `note_outline_opacity` | double: `0..1`; `0.78` | ノート枠の不透明度。 |
| `note_fade_in` | double: `0..1`; `0` | 上部の黒い霧の深さ。ノートが徐々に現れます。0はオフ。 |
| `note_fade_out` | double: `0..1`; `0` | 判定ライン上の黒い霧の深さ。ノートが徐々に消えます。0はオフ。キーとHUDは表示されます。 |
| `hold_body_opacity` | double: `0..1`; `1` | LN 本体の不透明度。 |
| `lane_width_scales` | object: mode → number[]; `0.50..1.75` | レーン数と同じ長さの個別幅配列。 |
| `note_width_scale` | double: `0.50..1.40`; `1` | Note & Field Size。中心を保ちフィールド・レーン・ノート・隣接ゲージを調整。 |
| `lane_spacing_scales` | object: mode → number[]; `0..2` | レーン間隔配列。長さは lane_count - 1。 |
| `note_height_scale` | double: `0.50..4`; `1.8` | ノート頭部・尾部の高さ倍率。 |
| `lane_divider_width_scale` | double: `0..2`; `1` | 全モード共通の区切り線幅。取り込んだ LR2 区切り線にも適用。 |
| `lane_center_gap_scale` | double: `0..2`; `0` | 16K 左右ブロックの中央間隔。 |
| `hold_body_width_scale` | double: `0.50..1.20`; `1` | LN 本体の幅倍率。 |
| `note_width_scales`, `note_height_scales`, `lane_center_gap_scales` | object: mode → number | 対応する共通値のモード別上書き。同じ範囲に制限。 |
| `lane_divider_width_scales` | object: mode → number | 旧版互換。現在は共通 `lane_divider_width_scale` を使用。 |
| `lane_colors` | object: mode → string[] | レーン数と同じ長さの色トークン配列。 |
| `single_color` | string; `off` | 色トークンで全レーンを上書き。既存の `lane_colors` は保持。 |

モード別配列・上書き: `4k`, `5k`, `6k`, `7k`, `7+1`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`。色トークン: `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`。`7+1` はスキンパレットであり独立したキーマップモードではありません。旧 `expand_notes_to_dividers=true` は間隔 0 として読み、明示的な `note_divider_gap_px` が優先します。

BGA全体の暗幕フィルターは適用せず、黒いフィールド・レーン背景・ギアは設定どおり維持します。Visual Latencyはスキン設定の5番目の項目です。

文字の読みやすさ補正はメニュー・設定・曲一覧・結果・ヘルプ・チャット・アカウント・エディター・ゲーム全体に適用します。明るい文字には暗い縁取り、暗い文字には明るい縁取りを付け、スキンの文字色と透明度は維持します。

### `offsets`

- `input` (double)
- `visual` (double)
  - `-500..500` に clamp
- `sound` (double, ms)
  - `-500..500` に clamp し、UI では `Audio Settings > Sound Offset` と `Calibration Wizard` に表示
  - 正の値は chart BGM/autoplay keysound を遅らせ、負の値は早める。判定、note/BGA timing、hit-triggered `follow` keysound は移動しない

## `keymap.json`

### Shape
- `layout` (string)
- `bindings`
  - legacy 10K compatibility
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`
  - 各 mode 内で lane id -> key token

### Notes
- 古い single-layout keymap は runtime で 10K map へ移行される。
- runtime は最終的な chart lane count に応じて relevant mode binding を選ぶ。
- key rebinding は成功捕捉後すぐ `keymap.json` へ保存され、別の最終保存手順はない。
- Song Select から keymap 編集を開いた場合、editor はまず選択譜面の lane count、次に `mode.key_mode`、最後に `10k` を使う。

## Runtime Migration Notes
- 古い profile は一部値を自動補正される。
- とくに BMS defaults と keysound policy が migration 対象で、旧 osu chart/skin field は保存されない。
- config file が存在しない場合、app は defaults で起動して直ちに profile を保存する。

## 設定操作とオーディオ同期

新しいプロファイルの `audio.volume` は `0.7` です。保存済みの音量を維持します。`audio.mute_when_inactive` は既定で `false`。有効にすると別のウィンドウの使用中は出力のみミュートし、曲の進行を維持します。ASIO バッファサイズはチャンネル当たりのサンプルで表示します。

キー設定は横並びの Key 1… です。基本キーと補助キーを指定でき、どちらかを押している間は押下状態が続きます。補助キーの × または入力待ち中の Delete で補助設定を解除します。`keymap.json` の任意の `secondary_modes` は既存のモード・レイン構造を使います。オプションの初期編集モードは 4K、譜面から開く場合は実際のキー数です。

`skin.key_backdrop_enabled` と `skin.key_backdrop_opacity` (0–1) はキー背景色を制御します。初回編集で `key_backdrop_override=true` を保存し、プロファイルの設定を優先します。ヒットの明るさは別設定です。判定ラインの太さはノート高さに比例します。難易度表の選択中は ALL SONG に表の譜面だけを表示し、Native LV で全譜面に戻ります。画面と BGA は実際の音声再生位置を使い、判定・音声予約の時刻は維持します。速度を変更するとクリック音が鳴ります。

キー背景のRGB明るさは `skin.key_backdrop_brightness`（0–2、既定1）、最大高さは `skin.key_backdrop_height`（0–1、既定1）です。高さはフィールド下端を基準とし、不透明度とは独立しています。プレイ・ゴースト・スキンプレビューに同じ設定を適用します。`−/+` が表示される設定は左右キーを押し続けて連続調整できます。
