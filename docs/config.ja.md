# TenRiff Config Schema (current)

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
  - menu BGM と chart background audio の on/off
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `keysound_volume` (double)

### `input`

- `backend` (string)
  - `polling | rawinput`
  - 現行 `1.4.5.1` リリースラインの既定値は `rawinput`
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
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 既定 `pg / gr / gd` は `20ms / 45ms / 90ms`
- 既定 `bd` は `210ms`
- `Judge Easy` は従来の `1.25x` 倍率で `bd=262.5ms`、`Judge Hard` は `bd=340ms` を使用。Hard でも PG/GR/GD と LN tail window は基本値のまま
- `indirect_miss` (double, ms)
  - 入力が来ないまま note が auto-miss になるときの閾値
  - timing は `bd` に合わせ、`Judge Hard` では未入力 note を BAD ではなく combo-breaking indirect `POOR` / OD8 `MISS` として記録
- `hold_grace` (double, ms)
  - long-note tail release を `PG` とみなす専用 window
  - 既定値は `80ms`
- `hold_break` (double, ms)
  - long-note tail release を最大 `GR` まで許容する終端 window
  - この範囲を外れると `BD`
  - 内部的には常に `hold_grace` 以上に維持
  - 既定値は `200ms`
- `mask` (double, ms)

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

### `gauge`
- `normal | hard | ex_hard | easy` は曲終了または失敗まで type が固定です。
- `shift` は EX-Hard / Hard / Normal / Easy をそれぞれ 100% から独立して同時に計算します。現在の tier が 0% で脱落すると、同じ判定をすでに累積している次の生存 tier を選び、終了時に生存している最上位 tier が最終 gauge になります。
- EX-Hard、Hard、Normal、Easy はすべて `100%` で開始し、`0%` で即失敗します。
- `delta`
  - `ex_hard`, `hard`, `normal`, `easy`
  - それぞれ `PG`, `GR`, `GD`, `BD`, `PR` を持つ

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - 既定値は `borderless`。Discord、OBS、Game Bar などの外部 overlay にもこの mode を推奨
  - `windowed` はタイトルバー付き固定サイズウィンドウ
  - `fullscreen` は DXGI exclusive fullscreen のため、現在の Discord Game Overlay は表示されない
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
- `vsync` (bool)
- `refresh_hz` (int)
  - `-1` は `Match Display`、`0` は `Unlimited`
  - 既定値は `-1`
  - `vsync=false` のときだけ直接 FPS cap として使われる
  - `vsync=false` では menu は実効 `300` cap。gameplay は `-1` で monitor Hz に従い、`0` で render pacing を解除
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
  - 既定値は `krrcream`。NK3 は同じ key count でも remaster を実行し、P64 path は既定で strict OpenVINO GPU を使用
  - Krrcream は元 note を target lane へ再配置するだけ
  - nK2 は key count 拡張時、元 pattern へ先に note を追加せず、変換中に target layout へ安全な support note を直接生成
  - NK3 は P64 と host beam safety solver を常に使い、10K 以外の source を 10K に変換するときだけ generalized MLP を追加する。`TENRIFF_NK3_DEVICE=CPU` は strict P64 CPU を選択し、有効な MLP は検証済み NPU、GPU、CPU の順に試行
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`。既定値は `native`
  - nK2 の `Native (12%)` / `Transform (35%)` / `Remaster (65%)` を選択し、Krrcream では設定 row を lock
  - `Remaster` は budget を上げつつ原曲の配置を保ち、LN 区間の support note を同じ長さの LN で埋める
  - 3 つとも上限であり、実際の追加量は原曲の密度と safety window によってさらに低くなる
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
- `random` (string)
  - `off | mirror | fr | sr`
- `random_seed` (int)
  - FR/SR、強制 key-mode 変換、LN Mix 対象選択の固定 seed。Mirror 変換自体は使用しない
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
  - `off | accuracy | score`、既定値は `off`
  - Accuracy/Score mode は譜面末尾まで進み、選択した result target 以上の場合だけ clear
  - Pacemaker を有効にすると Practice と Sudden Death は無効になり、replay playback と multiplayer では強制 off
- `pacemaker_target_accuracy` (double)
  - `0..100`、既定値 `90.0`。標準 result Accuracy と比較
- `pacemaker_target_score` (int)
  - `0..10000`、既定値 `8000`。倍率適用後の表示 final Score と比較
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
- `profile_nickname` (string)
  - Quick Setup で編集し、保存 record と multiplayer の表示名に使用
  - control 文字と重複空白を除去し UTF-8 で最大48 byte。空なら profile ID を表示
- `profile_avatar_path` (string)
  - Profile Setup で選択したローカル PNG/JPG パス。空の場合は TenRiff の既定表示を使用
  - profile ごとに保存し、UI-safe な UTF-8 最大 2048 byte に正規化
- `language` (string)
  - `en | ko`
  - 無効値は load 時に `en` へ正規化
  - Graphics Settings の Language 行に接続されている
- `result_tail_ms` (double)
- `require_enter_to_exit` (bool)
- `active_song_source` (string)
  - 最後に開いた song root
- `recent_song_sources` (array of string)
  - recent external/internal song source 一覧
- `difficulty_table_path` (string)
  - Browse で選択した local BMS difficulty-table header JSON、または link から作成した profile cache header の path
  - header は `name`, `symbol`, local relative `data_url`、data array entry は `md5` または `sha256` と `level` を使用
  - 選択/解除時に現在の source を再インデックスして一致譜面へ table level を表示し、選択時は hash が必要な `safe` index へ自動切替
- `difficulty_table_url` (string)
  - Browse から import した http(s) BMSTable HTML page または header JSON の元 link
  - 標準 `<meta name="bmstable" content="...">` を解決して header/data JSON を profile の `difficulty_tables` cache に保存。local JSON 選択時は空になる

### `skin`
- `source` (string)
  - `native | tenriff | lr2`
- `tenriff_skin_name` (string)
  - 取り込んだ TenRiff `skin.json` スキンフォルダー名
- `lr2_skin_name` (string)
  - 取り込んだ LR2 playskin 名
- `lr2_resolution_mode` (string)
  - `auto | sd | hd | fhd`
  - LR2 playskin resolution override token
  - `auto` は asset file 名ではなく `#DST_NOTE` レイアウト座標を見て SD/HD/FHD を解決
- `note_shape` (string)
  - `rect | triangle | pentagon | hexagon | circle`
  - 100% では procedural 円・多角形が rect bar と同じ lane 全幅を使用
- `show_hold_tail` (bool)
  - long-note の判定と body の連続性を変えず、tail cap だけを表示または非表示にする
- `note_border_enabled` (bool)
- `black_playfield_enabled` (bool)
  - `true` の場合、lane spacing を含む player/ghost playfield 全体を完全な黒で表示
  - 既定値は `true`。既存プロファイルで明示された `false` は維持される
- `judgement_line_position` (double)
  - gameplay judgement line の縦位置比率
  - `0.00..1.00`（0%～100%）に clamp
  - 既定値は `0.82`
- `combo_position` (double)
  - gameplay field 内の combo 表示縦位置比率
  - `0.10..0.78` に clamp
  - 既定値は `0.24`
- `lane_width_scales` (object)
  - キーモードごとの lane width scale 配列
  - 各 mode 値は lane ごとに 1 要素を持つ数値配列
  - 各値は `0.50..1.75` に clamp
- `note_width_scale` (double)
  - 中央基準の playfield 全体、lane/divider、note head/tail、隣接 gauge をまとめて拡大・縮小する (`0.50..1.40`)
  - 100% で隣接 note 間の既定合計 gap は `24px`
  - `0.50..1.40` に clamp
- `lane_spacing_scales` (object)
  - キーモードごとの lane 間空白スケール配列
  - 各 mode 値は `(lane_count - 1)` 要素を持つ
  - 各値は `0.00..2.00` に clamp
- `note_height_scale` (double)
  - note head / tail の高さスケール
  - `0.50..4.00` に clamp
- `lane_divider_width_scale` (double)
  - 白い lane separator line の共通スケール
  - `0.00..2.00` に clamp
  - 全キーモードに一様適用
- `lane_center_gap_scale` (double)
  - 16K フィールド左右中央 gap スケール
  - `0.00..2.00` に clamp
  - 現状 `16k` レイアウトだけに適用
- `hold_body_width_scale` (double)
  - long-note body の幅スケール
  - `0.50..1.20` に clamp
- `note_width_scales` (object)
  - キーモードごとの `note_width_scale` override
- `note_height_scales` (object)
  - キーモードごとの `note_height_scale` override
- `lane_divider_width_scales` (object)
  - legacy compatibility field
- `lane_center_gap_scales` (object)
  - キーモードごとの `lane_center_gap_scale` override
- `single_color` (string)
  - `off` または対応 color token
  - color token を選ぶと、すべての key mode と scratch lane を同じ色で表示
  - lane ごとの `lane_colors` は保持されるため、`off` に戻すと元の palette を復元
- `lane_colors` (object)
  - キーモードごとの lane color palette
  - 現在の既定/保存 mode は `4k..10k` と `16k`
  - 各 mode 値は lane ごとに 1 要素を持つ string 配列
  - 対応 token:
    `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`

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
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`
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
