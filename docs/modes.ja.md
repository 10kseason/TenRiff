# Mode System (Key / Gauge / Random / Mods)

この文書は、現在実装されている mode system、lane transform/random rule（Mirror / FR / SR）、note structure mod をまとめたものです。

## Configuration Location
- Global: `config/config.json` の `mode` セクション
- Profile: `profiles/<name>/config.json` の `mode` セクション

```json
"mode": {
  "key_mode": "none",
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
- chart input は BMS family（`.bms/.bme/.bml/.pms`）専用で、`format` と `enable_osu_charts` setting は削除済み
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`: `normal | hard | ex_hard | easy`
- `random`: `off | mirror | fr | sr`
- `random_seed`: FR/SR、強制 key-mode 変換、LN Mix 対象選択の固定 seed（`0` も固定値として扱う）
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
- **Mirror**: key-mode 変換後の最終 lane を決定的に反転
  - 10K/16K は二つの player field を交換せず、各 half 内で独立して反転
  - Mirror 自体は `random_seed` を使わないが、先に行う強制 key-mode 変換は seed を使用する場合がある
- **FR (Full Random)**: lane 集合全体を random permutation で置き換える
- **SR (Super Random)**: note ごとに random placement
  - 同時刻も含めて同一 lane overlap が起きないよう candidate lane を選ぶ
  - long note は head / tail を同じ lane に保つ
  - 候補 lane がない場合は元の lane を維持し、warning を出す

## Note-Structure Mods
- **Full LN**: 対象 tap を同じ lane の次 note 直前までの standard hold に変換
- **LN Mix 10%～90%**: 既存 hold を維持し、同じ lane の既存 span と重なる head を除外したうえで、50ms 以上の hold と次 note 前 50ms の余裕を確保できる tap から設定割合を `random_seed` で決定的に選択
- **Full Tap**: すべての hold tail を削除して tap に変換
- 三つは同じ `Note Structure` category のため一つだけ有効。同じ譜面と seed では同じ LN Mix 結果を再現する

## Key-Mode Handling
- `none` は譜面の lane count と base pattern layout をそのまま維持する
- `auto` は legacy alias で、現状は `none` と同じ挙動
- key-mode 変換は Mirror / FR / SR より先に適用
- `4k..16k` は N2NC ベースの lane remap で key count を合わせる

## Gauge Rules
- すべての gauge は `100%` で開始し、`0%` に到達すると即失敗する。
- `ex_hard` は Hard より回復が低く、`BAD` / `POOR` の損失が大きい challenge gauge。
- clear status は `EX-HARD CLEAR`, `HARD CLEAR`, `CLEAR`, `EASY CLEAR` として区別される。
- `Sudden Death (1 MISS)` は gauge type ではなく、最初の OD8 換算 object `MISS` で現在 gauge を 0 にして即終了する別の failure rule。

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
- Mod registry / note-structure transform: `src/app/ModeManager.*`
