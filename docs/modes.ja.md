# Mode System (Key / Gauge / Random / Mods)

この文書は、現在実装されている mode system、lane transform/random rule（Mirror / FR / SR）、note structure mod をまとめたものです。

## Configuration Location
- Global: `config/config.json` の `mode` セクション
- Profile: `profiles/<name>/config.json` の `mode` セクション

```json
"mode": {
  "key_mode": "none",
  "key_conversion_algorithm": "krrcream",
  "key_conversion_nk2_preset": "native",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "pacemaker_mode": "off",
  "pacemaker_target_accuracy": 90.0,
  "pacemaker_target_score": 8000,
  "song_index_profile": "safe",
  "calculate_song_index_difficulty": false
}
```

## Mode Meanings
- chart input は BMS family（`.bms/.bme/.bml/.pms`）専用で、旧 osu toggle は表示・保存しない
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
- `key_conversion_algorithm`: `krrcream | nk2 | nk3`（既定値は `krrcream`。Krrcream は元 note の再配置のみ、nK2 は安全な support note を生成し、NK3 は同梱 P64 と host beam32 を常に使い、10K 以外の source を 10K に変換するときだけ generalized MLP を追加する）
- `key_conversion_nk2_preset`: `native | transform | remaster`（既定は `native`。nK2 選択時は `Native (12%)` / `Transform (35%)` / `Remaster (65%)`、Krrcream 選択時は row を lock）
- `gauge`: `normal | hard | ex_hard | easy | shift`
- `random`: `off | mirror | rr | fr | sr`
- `random`: `off | mirror | rr | frns | sr`（`fr` は旧設定互換 alias）
- `random_seed`: RR/SR、強制 key-mode 変換、Note Add、LN Mix 対象選択の固定 seed（`0` も固定値）。通常 Random は play ごとに新しい session seed を生成し、replay に実際の値を記録
- `mods`: Mod Manager が正規化して保存する mod token 配列
- `ghost_battle_enabled`: `false | true`
  - 既定値は `false`
  - `true`: 選択譜面の best compatible replay を自動ロードして ghost 比較する
  - `false`: 通常 gameplay を single-field のままにする
- `autoplay_enabled`: 判定可能な note を自動処理し、`AUTOPLAY` result を保存するが official clear・best score・clear lamp の対象外
- `practice_no_fail_enabled`: gauge による途中失敗を防ぎ、譜面末尾まで継続
- `one_miss_fail_enabled`: 最初の OD8 換算 object `MISS` で即失敗する `Sudden Death (1 MISS)`
  - native `BAD` timing だけでは発動せず、空打ちの `POOR` も発動条件ではない
  - Mode Settings では Practice No-Fail と排他的
- `pacemaker_mode`: `off | accuracy | score`、既定 `off`。目標は `pacemaker_target_accuracy`（`0..100`、既定 `90`）または `pacemaker_target_score`（`0..10000`、既定 `8000`）。ゲージによる途中失敗なしで最後まで進み、目標で結果を判定。Practice・Sudden Death と排他で、マルチ・リプレイには適用しません。
- `song_index_profile`: `safe | fast`
  - `safe`: 大規模ライブラリでの RAM high-water 抑制を優先する既定値
  - `fast`: title/artist/key count/#PLAYLEVEL/BPM のみ保持し、hash、preview、difficulty table、native LV/CR を省略する最小 indexing
- `calculate_song_index_difficulty`: `false | true`
  - 既定 `false`: BMS `#PLAYLEVEL` を保持し、native LV/CR 計算を省略
  - `true`: full `safe` reindex で Revive LV/Circus Rating を計算し、`fast` では常に省略

`Rate` は `mode` ではなく `speed.rate` に保存されます。Mode Settings で変更でき、検索入力中でなければ Song Select の `-` / `+` でも次の play 値を直接変更できます。

## Lane Transform / Random Rules
- **DP Flip**: DP の左右 player field 全体を交換し、各 field 内の lane 順は維持
- **Mirror**: key-mode 変換後の最終 lane を決定的に反転
  - DP layout は二つの player field を交換せず、各 field 内で独立して反転
  - Mirror 自体は `random_seed` を使わないが、先に行う強制 key-mode 変換は seed を使用する場合がある
- **RR (R-Random)**: scratch を固定し、playable lane group ごとに seed 付き offset で回転。DP の左右は独立処理
- **Random / FRNS**: scratch を固定し、key lane だけを play ごとの新しい seed で random permutation。replay は記録された seed で同じ配置を復元
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
- `4k..10k`、`12k`、`14k`、`16k` は選択した Krrcream / nK2 / NK3 アルゴリズムでキー数を変換します。
- `5+1 SP` / `7+1 SP` の強制変換は scratch を除く鍵盤部だけを再配置し、`follow` の scratch keysound は autoplay へ移す
- `10+2 DP` / `14+2 DP` も両 scratch を除外し、左右の鍵盤部を独立変換
- nK2 拡張は元 note を target key へ配置してから同じ target layout に support note を生成し、元の4Kなどへ note を先に追加して再変換することはない
- 適用順: key-mode 変換（nK2 の target-layout support 生成を含む）→ DP Flip → Mirror/RR/FR/SR → Note Add → LN/Full Tap 構造変換

## Gauge Rules

- `ex_hard / hard / normal / easy` は常時有効な Gauge Shift の開始段階です。旧 `shift` は EX 開始。
- 開始段階から Easy までそれぞれ100%から並列計算し、脱落時は次の生存段階へ移ります。すべての対象段階が脱落するとゲージ失敗。
- 最終生存段階を `GAUGE SHIFT EX / HARD / NORMAL / EASY CLEAR` で表示します。
- Practice・Pacemaker の独自終了規則は維持。Sudden Death は最初の OD8 換算 object MISS で即終了します。

## Implementation Location
- Mode parsing: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- Mode application: `src/gameplay/ModeApplier.*`
- Mod registry / note-structure transform: `src/app/ModeManager.*`
