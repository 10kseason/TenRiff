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
  - 現行 `1.2.1` リリースラインの既定値は `rawinput`
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
- 既定 `gd` は `75ms`
- 既定 `bd` は `340ms`
- `indirect_miss` (double, ms)
  - 入力が来ないまま note が auto-miss になるときの閾値
  - 現在の runtime では保存値に関係なく常に `bd` と同値へ折りたたまれる
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
- automatic gauge shift はありません。選択した gauge type は曲終了または失敗まで固定です。
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
  - `60..1050` に clamp
  - 既定値は `300`
  - `vsync=false` のときだけ直接 FPS cap として使われる
  - `vsync=false` では menu は実効 `300` cap、gameplay は `min(configured target, max(300, monitor_hz * 2))` で safety clamp
  - `vsync=true` では present refresh は active monitor Hz に従い、render pacing は `monitor_hz * 2` を狙う（`1050` clamp）
- `performance_overlay` (bool)
  - 既定値は `false`。右上を使うため、同じ角に置いた Discord Voice widget と重なる場合がある
- `background_upscale_mode` (string)
  - `lunasr | off`
  - 既定値は `lunasr`。1920x1080 未満の BMS image BGA と osu!mania 背景を FHD に非同期補間
  - 処理中または model load / image decode / inference 失敗時は native bitmap を維持
  - Graphics Settings の `BGA Upscale` row に接続

### `mode`
- `format` (string)
  - chart filtering とあわせて使う
  - `bms | osu | auto`
  - `auto` は実質 `All`
- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
  - `none` は譜面本来の key count をそのまま使う
- `gauge` (string)
  - `normal | hard | ex_hard | easy`
- `random` (string)
  - `off | mirror | fr | sr`
- `random_seed` (int)
  - FR/SR、強制 key-mode 変換、LN Mix 対象選択の固定 seed。Mirror 変換自体は使用しない
- `mods` (string array)
  - Note Structure では `full_long_notes`、`ln_mix_10`～`ln_mix_90`、`full_short_notes` のいずれか一つを選択できる
  - LN Mix は 50ms 以上の hold と同じ lane の次ノート前 50ms の余裕を両方確保できる tap のみを候補にし、指定割合を丸めた個数だけ standard hold に変換する
  - 既存 hold は維持され、同じ lane の既存 span と重なる head は除外され、同じ `random_seed` では同じ tap が選択される
- `enable_osu_charts` (bool)
- `ghost_battle_enabled` (bool)
  - 既定値は `false`
  - `true` のとき、選択譜面の互換性ある best replay を自動ロードして ghost 比較を行う
  - `false` のとき、通常 gameplay は single-field のまま
- `autoplay_enabled` (bool)
  - QA assist mode
  - `true` のとき playable note 入力を自動処理し、result には `ASSIST` が付く
  - 既定の ghost/replay 比較フローには入れない前提
- `practice_no_fail_enabled` (bool)
  - QA assist mode
  - `true` のとき gauge による早期失敗を無効化し、判定と result export は最後まで継続
  - result には `ASSIST` が付く
- `one_miss_fail_enabled` (bool)
  - `true` のとき最初の osu!mania OD8 object `MISS` で gauge が 0 になり、即座に失敗する
  - native `BAD` timing だけでは発動せず、空打ちの `POOR` も即死条件に含めない
  - Mode Settings で有効にすると `practice_no_fail_enabled` は自動的に無効になる
- `song_index_profile` (string)
  - `safe | fast`
  - `safe` は大規模ライブラリで RAM high-water を抑える既定値
  - `fast` は 32GB+ 環境向けに worker/batch 予算を増やして再スキャン高速化を狙う任意値

### `ui`
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

### `skin`
- `source` (string)
  - `native | osu | lr2`
- `osu_skin_name` (string)
  - 取り込んだ osu!mania skin 名
- `lr2_skin_name` (string)
  - 取り込んだ LR2 playskin 名
- `lr2_resolution_mode` (string)
  - `auto | sd | hd | fhd`
  - LR2 playskin resolution override token
  - `auto` は asset file 名ではなく `#DST_NOTE` レイアウト座標を見て SD/HD/FHD を解決
- `note_shape` (string)
  - `rect | triangle | pentagon | hexagon | circle`
- `show_hold_tail` (bool)
  - long-note の判定と body の連続性を変えず、tail cap だけを表示または非表示にする
- `note_border_enabled` (bool)
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
  - note head / tail の幅スケール
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
- とくに BMS-first defaults、osu key-mode mismatch、keysound policy が migration 対象。
- config file が存在しない場合、app は defaults で起動して直ちに profile を保存する。
