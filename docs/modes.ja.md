# Mode System (Format / Key / Gauge / Random)

この文書は、現在実装されている mode system と random rule（SR / FR）をまとめたものです。

## Configuration Location
- Global: `config/config.json` の `mode` セクション
- Profile: `profiles/<name>/config.json` の `mode` セクション

```json
"mode": {
  "format": "auto",
  "key_mode": "none",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "enable_osu_charts": false,
  "ghost_battle_enabled": true,
  "song_index_profile": "safe"
}
```

## Mode Meanings
- `format`: `auto | bms | osu`
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`: `normal | hard | ex_hard | easy`
- `random`: `off | fr | sr`
- `random_seed`: 固定 random seed（`0` も固定値として扱う）
- `enable_osu_charts`: `false | true`
- `ghost_battle_enabled`: `false | true`
  - `true`: 選択譜面の best compatible replay を自動ロードして ghost 比較する
  - `false`: 通常 gameplay を single-field のままにする
- `song_index_profile`: `safe | fast`
  - `safe`: 大規模ライブラリでの RAM high-water 抑制を優先する既定値
  - `fast`: 32GB+ 環境で再インデックス高速化を狙う任意値

## Random Rules
- **FR (Full Random)**: lane 集合全体を random permutation で置き換える
- **SR (Super Random)**: note ごとに random placement
  - 同時刻も含めて同一 lane overlap が起きないよう candidate lane を選ぶ
  - long note は head / tail を同じ lane に保つ
  - 候補 lane がない場合は元の lane を維持し、warning を出す

## Key-Mode Handling
- `none` は譜面の lane count と base pattern layout をそのまま維持する
- `auto` は legacy alias で、現状は `none` と同じ挙動
- `4k..16k` は N2NC ベースの lane remap で key count を合わせる

## Gauge Rules
- すべての gauge は `100%` で開始し、`0%` に到達すると即失敗する。
- `ex_hard` は Hard より回復が低く、`BAD` / `POOR` の損失が大きい challenge gauge。
- clear status は `EX-HARD CLEAR`, `HARD CLEAR`, `CLEAR`, `EASY CLEAR` として区別される。

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
