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
  - 現在の `1.1.2 final stable` ラインの既定値は `rawinput`
  - 保存時に `polling` へ強制正規化されず、保存値はそのまま維持される
  - ただし gameplay セッションの live capture 自体は安定性のため `Polling` に固定
- `rawinput` (bool)
  - `backend` と並ぶ便宜的な保存フラグ
  - 現在の `1.1.2 final stable` ラインではそのまま保存/復元される
  - gameplay runtime が live capture で無視することはあるが、config 値自体は保持される
- `use_qpc` (bool)
- `grab` (bool)
  - 現状では Linux preview 向け設定
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - polling backend が keyboard state を読む頻度
  - 既定は `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - input config に残っている互換フィールド
  - 現在の `1.1.2` runtime では別個の audio-thread judgement sub-step loop をこれで駆動しない
  - 既定は `4000` (`0.25ms`)
- `debounce_ms` (double)
  - 同じキーの極短い up/down ノイズを落とす debounce 時間
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
- Hard、Normal、Easy はすべて `100%` で開始し、`0%` で即失敗します。
- `delta`
  - `hard`, `normal`, `easy`
  - それぞれ `PG`, `GR`, `GD`, `BD`, `PR` を持つ

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - `windowed` はタイトルバー付き固定サイズウィンドウ
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

### `mode`
- `format` (string)
  - chart filtering とあわせて使う
  - `bms | osu | auto`
  - `auto` は実質 `All`
- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
  - `none` は譜面本来の key count をそのまま使う
- `gauge` (string)
  - `normal | hard | easy`
- `random` (string)
  - `off | fr | sr`
- `random_seed` (int)
- `enable_osu_charts` (bool)
- `ghost_battle_enabled` (bool)
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
  - `rect | circle`
- `note_border_enabled` (bool)
- `judgement_line_position` (double)
  - gameplay judgement line の縦位置比率
  - `0.55..0.86` に clamp
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
