# Mode System (Key / Gauge / Random / Mods)

この文書は、現在実装されている mode system、lane transform/random rule（Mirror / FR / SR）、note structure mod をまとめたものです。

## Configuration Location
- Global: `config/config.json` の `mode` セクション
- Profile: `profiles/<name>/config.json` の `mode` セクション

```json
"mode": {
  "key_mode": "none",
  "key_conversion_algorithm": "krrcream",
  "enable_osu_charts": false,
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe"
}
```

## Mode Meanings
- `enable_osu_charts`: 既定 `false`。Mode Settings の `OSU Charts` で osu!mania 4K～10K `.osu` の index/play を有効にし、library を再 scan
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
- `key_conversion_algorithm`: `krrcream | nk2`（既定値は `krrcream`。ゲーム内 Key Converter で選択し、実際のキー数変換時のみ使用）
- `gauge`: `normal | hard | ex_hard | easy | shift`
- `random`: `off | mirror | rr | fr | sr`
- `random_seed`: RR/FR/SR、強制 key-mode 変換、Note Add、LN Mix 対象選択の固定 seed（`0` も固定値として扱う）
- `mods`: Mod Manager が正規化して保存する mod token 配列
- `ghost_battle_enabled`: `false | true`
  - 既定値は `false`
  - `true`: 選択譜面の best compatible replay を自動ロードして ghost 比較する
  - `false`: 通常 gameplay を single-field のままにする
- `autoplay_enabled`: 判定可能な note を自動処理し、result を `ASSIST` として表示
- `practice_no_fail_enabled`: gauge による途中失敗を防ぎ、譜面末尾まで継続
- `one_miss_fail_enabled`: 最初の OD8 換算 object `MISS` で即失敗する `Sudden Death (1 MISS)`
  - native `BAD` timing だけでは発動せず、空打ちの `POOR` も発動条件ではない
  - Mode Settings では Practice No-Fail と排他的
- `song_index_profile`: `safe | fast`
  - `safe`: 大規模ライブラリでの RAM high-water 抑制を優先する既定値
  - `fast`: 32GB+ 環境で再インデックス高速化を狙う任意値

`Rate` は `mode` ではなく `speed.rate` に保存されます。Mode Settings で変更でき、検索入力中でなければ Song Select の `-` / `+` でも次の play 値を直接変更できます。

## Lane Transform / Random Rules
- **DP Flip**: DP の左右 player field 全体を交換し、各 field 内の lane 順は維持
- **Mirror**: key-mode 変換後の最終 lane を決定的に反転
  - DP layout は二つの player field を交換せず、各 field 内で独立して反転
  - Mirror 自体は `random_seed` を使わないが、先に行う強制 key-mode 変換は seed を使用する場合がある
- **RR (R-Random)**: scratch を固定し、playable lane group ごとに seed 付き offset で回転。DP の左右は独立処理
- **FR (Full Random)**: lane 集合全体を random permutation で置き換える
- **SR (Super Random)**: note ごとに random placement
  - 同時刻も含めて同一 lane overlap が起きないよう candidate lane を選ぶ
  - long note は head / tail を同じ lane に保つ
  - 候補 lane がない場合は元の lane を維持し、warning を出す

## Note-Structure Mods
- **Note Add 10%～100%**: 既存 note 時刻に無音の chord note を決定的に追加し、scratch、hold body、同 lane 重複、過大 chord を避ける。Records には残るが通常 best record は更新しない
- **Full LN**: 対象 tap を同じ lane の次 note 直前までの standard hold に変換
- **LN Mix 10%～90%**: 既存 hold を維持し、同じ lane の既存 span と重なる head を除外する。base BPM 基準の 1/8-note hold が次の同一 lane note より 50ms 以上前に終わる tap から設定割合を `random_seed` で選択し、すべての Mix 段階で長い 1/8-note 60% / 中間 1/16-note 20% / 短い 1/24・1/32-note 20% に決定的に配分
- **Full Tap**: すべての hold tail を削除して tap に変換
- 三つは同じ `Note Structure` category のため一つだけ有効。同じ譜面と seed では同じ LN Mix 結果を再現する

## Key-Mode Handling
- `none` は譜面の lane count と base pattern layout をそのまま維持する
- `auto` は legacy alias で、現状は `none` と同じ挙動
- `4k..10k`、`12k`、`14k`、`16k` は N2NC ベースの lane remap で key count を合わせる
- `5+1 SP` / `7+1 SP` の強制変換は scratch を除く鍵盤部だけを再配置し、`follow` の scratch keysound は autoplay へ移す
- `10+2 DP` / `14+2 DP` も両 scratch を除外し、左右の鍵盤部を独立変換
- 適用順: key-mode 変換 → DP Flip → Mirror/RR/FR/SR → Note Add → LN/Full Tap 構造変換

## Gauge Rules
- 固定 gauge（`ex_hard / hard / normal / easy`）は `100%` で開始し、`0%` で即失敗して type は変化しない。
- `shift` は EX-Hard / Hard / Normal / Easy をそれぞれ 100% から独立して並列計算し、現在の tier が脱落すると同じ判定履歴を累積した次の生存 tier を選び、終了時の最上位生存 tier で確定する。
- `ex_hard` は Hard より回復が低く、`BAD` / `POOR` の損失が大きい challenge gauge。
- clear status は固定 gauge の結果と、最終 Shift tier の `GAUGE SHIFT EX-HARD / HARD / NORMAL / EASY CLEAR` を区別する。
- `Sudden Death (1 MISS)` は gauge type ではなく、最初の OD8 換算 object `MISS` で現在 gauge を 0 にして即終了する別の failure rule。

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
- Mod registry / note-structure transform: `src/app/ModeManager.*`
