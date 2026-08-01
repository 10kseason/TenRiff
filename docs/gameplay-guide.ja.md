# TenRiff Gameplay Guide

この文書は、TenRiff を初めて触るプレイヤーが実際にどうやって曲を選び、プレイし、結果を確認するかに焦点を当てたユーザーガイドです。

詳細な設定構造は [`docs/config.ja.md`](config.ja.md)、現在の実装範囲は [`docs/current-state.ja.md`](current-state.ja.md) を参照してください。

## 1. 初回起動

Windows では通常、次のどちらかで起動します。

```powershell
.\launch_win.bat
```

または

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

初回起動時には default profile が自動生成されます。

## 2. 基本フロー

1. Title 画面に入る
2. Song Select で曲を選ぶ
3. 必要なら Mode / Audio / Graphics / Skins / Keymap を調整する
4. 曲を開始する
5. `3 / 2 / 1` カウントダウン後にプレイする
6. Result 画面を確認する
7. `Enter` または `Esc` で Song Select に戻る

## 3. 基本メニュー操作

### Title
- `Up` / `Down`: メニュー移動
- `Enter`: 決定
- `Esc`: 終了

### Song Select
- `Up` / `Down`: 曲移動
- `PageUp` / `PageDown`: 高速移動
- マウスホイール: 曲移動
- 左クリック: 曲選択
- ダブルクリック: 曲開始
- `Enter`: 現在選択中の曲を開始
- `Left` / `Right`: 左側メニューのフォーカス切り替え
- `Esc`: 前の画面に戻る
- `-` / `+`: 次の play の Rate を即時調整
- `F5`: 曲ライブラリを再インデックス。実行中は stage / percent / ETA と progress bar を中央表示
- `Browse > Difficulty Table`: BMSTable HTML/header link をコピーして `Enter` で import、`Right` で local JSON 選択、`Left` で解除

### Song Select からよく使う画面
- `Mode`
  - Ghost Battle、Autoplay、Practice、Sudden Death、Key Mode、Gauge、Random、Mods、Rate、Hi-Speed を調整
- `Audio`
  - Master / BGM / Keysound volume と BMS keysound policy を調整
- `Graphics`
  - VSync、Refresh Hz、Performance HUD、Display Offset、BGA 表示、external ONNX BGA Upscaler を調整
  - `BGA` を off にすると gameplay image/video background と decoder/upscaler 処理を無効化し、Song Select preview は維持
  - model 選択後に upscaler を明示的に ON にして high-spec 警告を確認する。実験的 `NPU 優先` は Windows/driver が実際に NPU を選択した場合だけ NPU を使う
- `Skins`
  - native/LR2 skin 切り替え、LR2 folder import、固定 divider 基準の note gap/size、Black Playfield、judge line 位置、LN body width、lane colors を調整
- `Keymap`
  - key binding 変更と NKRO test 実行

## 4. 初期設定のおすすめ

- `Mode > Gauge`: `normal`
- `Mode > Sudden Death`: 最初の OD8 換算 `MISS` で終了する challenge が必要なときだけ有効化
- `Mode > Rate`: `1.0x`
- `Mode > Hi-Speed`: まずは既定値
- `Graphics > Display`: Discord voice overlay を使う場合は `Borderless` を推奨
- `Graphics > Performance HUD`: 必要なときだけ on
- `Graphics > Display Offset`: 最初は `0ms`
- `Audio > Keysound Mode`: BMS では `follow` 推奨

ノートが遅すぎる/速すぎるなら、まず `Hi-Speed` だけを調整してください。判定は合っているのに見た目だけ遅い/早い場合は `Display Offset` を調整します。

### Discord voice overlay

Discord の `User Settings > Game Overlay` で overlay と Voice widget を有効にし、TenRiff は `Graphics > Display > Borderless` または `Windowed` で実行してください。現在の Discord Game Overlay は DXGI exclusive fullscreen では表示されません。gameplay 情報との重なりを減らすには Voice widget を左下に固定し、TenRiff の `Performance HUD` は off にすることを推奨します。Discord が TenRiff を自動検出しない場合は、`Registered Games` で実行中の `TenRiff.exe` を追加してください。

client 側の設定は Discord の [公式 Game Overlay guide](https://support.discord.com/hc/en-us/articles/217659737-Game-Overlay-101) を参照してください。

## 5. 既定キー配置

- `4K`: `D F L ;`
- `5K`: `D F K L ;`
- `6K`: `S D F J K L`
- `7K`: `W E R M I O P`
- `8K`: `W E R V M I O P`
- `9K`: `A S D F Space H J K L`
- `10K`: `Q W E R V M I O P [`
- `16K`: `Q W E R A S D F U I O P J K L ;`

合わない場合は `Options > Keymap` で変更できます。

## 6. プレイ開始前に知っておくこと

### Chart format
- BMS family（`.bms/.bme/.bml/.pms`）が既定です。Mode Settings の `OSU Charts` を ON にすると osu!mania 4K～10K `.osu` も index/play します。

### Loading
- 曲開始直後に chart-loading progress が表示されることがあります。
- loading 中に `Esc` を押すと開始を中止して Song Select に戻ります。

### Countdown
- loading 完了後、まず `3 / 2 / 1` のカウントダウンが表示されます。
- カウントダウン中の入力は score judgement には入りません。

## 7. プレイ中の操作

- 譜面キー入力: 現在の keymap に従う
- `Esc`: single-player では pause menu（Continue / Restart / Exit）、multiplayer ではプレイを中止
- F3: Hi-Speed を下げる
- `F4`: Hi-Speed を上げる
- `F5`: Hi-Speed を大きく下げる
- `F6`: Hi-Speed を大きく上げる
- `F9`: 現在画面のスクリーンショット保存

Hi-Speed が変えるのは見た目のスクロール速度だけで、判定タイミングそのものは変えません。
Rate は曲の再生速度と譜面スケジュールだけを変え、同じ Hi-Speed で見た目のスクロール速度は変えません。

## 8. HUD の見方

- Title / artist
- BPM
- 現在の `Rate`
- 現在の `Hi-Speed`
- Gauge 値と現在の gauge type
- TenRiff native Score
- 実入力 timing を osu!mania stable OD8 / ScoreV1 で換算した補助 `OSU OD8` score
- Combo
- 直近 judgement (`PG / GR / GD / BD / PR`)
- タイミングずれ（ms）

中心からずれた入力は judgement の下に符号付きで表示されます。早い入力は `FAST -12 ms`、遅い入力は `SLOW +18 ms` となり、`0 ms` に丸められる入力では timing 表示を省略します。

`Graphics > Performance HUD` を有効にすると、frame graph、average FPS、low FPS、gameplay timing debug 情報も見られます。

## 9. 判定とゲージ

### Judgement
- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

`OSU OD8` は最大 1,000,000 の補助比較 score であり、TenRiff の native score、rank、clear result は変更しません。

native Score は judgement 90,000 点 + 累積 Combo 10,000 点で構成されます。全 `PG` の full combo は正確に 100,000 点で、LN は head / tail を各 0.5 weight とする 1 object です。

Accuracy は `PG / GR / GD / BD = 100 / 80 / 50 / 20%` を基準に、各 judgement band 内の timing に応じて最大 0.5 percentage point を追加減算します。全 `PG` でも PG timing span が 8ms を超えると 99.5% を超えません。

Rank は `<75 F`, `75 B`, `80.5 A`, `86.5 A+`, `90 S`, `95.5 S+`, `98 AA`, `99 SS`, `99.75 SSS` の境界を使います。

### Gauge
- `ex_hard`
- `hard`
- `normal`
- `easy`
- `shift`

固定 gauge（`ex_hard / hard / normal / easy`）は曲開始時に `100%` で始まり、play 中に type は変わりません。

- `ex_hard`: Hard より回復が低く `BAD` / `POOR` damage が大きい。 `0%` で即 Game Over
- `hard`: `0%` で即 Game Over
- `normal`: `0%` で即 Game Over
- `easy`: `0%` で即 Game Over
- `shift`: EX-Hard / Hard / Normal / Easy をそれぞれ 100% から独立して並列計算。現在の tier が脱落すると、同じ判定履歴を累積した次の生存 tier を選び、終了時の最上位生存 tier で確定

gauge transition は `shift` を明示的に選択した場合だけ発生します。
`Sudden Death (1 MISS)` は gauge type ではなく、最初の OD8 換算 object `MISS` で gauge を 0 にして即終了する rule です。native `BAD` timing だけでは発動せず、空打ちの `POOR` も対象外で、Practice No-Fail と同時には有効化できません。

## 10. Result 画面

- Clear / Game Over 状態
- Rank
- TenRiff native Score
- `OSU OD8` 補助 score と換算 judgement 集計
- Accuracy
- Max Combo
- PG / GR / GD / BD / PR の合計
- 平均 timing deviation と分散
- 最終 gauge と gauge 履歴
- 保存された replay / result file 名

戻るキー:
- `Left`: 同じ譜面を即再開
- `F1`: 保存 replay がある場合に再生
- `Enter`: Song Select に戻る
- `Esc`: Song Select に戻る

## 11. よくある調整

### ノートが詰まって見え、読みにくい
- `Hi-Speed` を上げる
- 必要なら `Skins > Note & Field Size / Note Height / Judge Line` も調整する

### 判定は合っているのに見た目だけ遅い/早い
- `Graphics > Display Offset` を調整する
- 正の値はノートをより早く描画する

### BMS keysound が大きすぎる/小さすぎる
- `Audio > Keysound Volume` を調整する
- BGM と別々に調整できる
- `follow` でも無音になり得た late-input 経路は 1.2.6 で修正され、判定時刻は維持したまま可聴開始だけを現在書き込み可能な buffer へ固定する
- まだ無音なら `Keysound Mode=follow`、0 以外の volume、譜面の `#WAV` asset path を確認する

### 入力が不安定、またはキー競合が疑わしい
- `Options > Keymap > NKRO Test` で複数キー同時押しを確認する
- 必要なら手の配置が重ならないよう key layout を変える

## 12. おすすめの慣らし順

1. `Song Select` で簡単な曲を選ぶ
2. `Mode` で `Gauge=Normal` と `Rate=1.0x` を確認する
3. プレイ後、まず `Hi-Speed` だけ調整する
4. まだ違和感があるなら `Display Offset` を調整する
5. 手の配置が合わなければ `Keymap` を変える
6. 最後に `Skins` で note size と judge line 位置を調整する

## 13. 関連文書

- 現在の実装状態: [`docs/current-state.ja.md`](current-state.ja.md)
- 設定構造: [`docs/config.ja.md`](config.ja.md)
- menu 構造: [`docs/menu.ja.md`](menu.ja.md)
- play loop / engine 構造: [`docs/core-loop.ja.md`](core-loop.ja.md)
