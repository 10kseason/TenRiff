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
- `F5`: 曲ライブラリを再インデックス

### Song Select からよく使う画面
- `Mode`
  - Gauge、Random、Rate、Hi-Speed、OSU Charts、Chart Filter を調整
- `Audio`
  - Master / BGM / Keysound volume と BMS keysound policy を調整
- `Graphics`
  - VSync、Refresh Hz、Performance HUD、Display Offset を調整
- `Skins`
  - judge line 位置、note size、LN body width、lane colors を調整
- `Keymap`
  - key binding 変更と NKRO test 実行

## 4. 初期設定のおすすめ

- `Mode > Gauge`: `normal`
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
- 既定フィルタは BMS 寄りです。
- osu!mania はオプションで有効化すれば `4K` から `10K` までインデックス/プレイできます。

### Loading
- 曲開始直後に chart-loading progress が表示されることがあります。
- loading 中に `Esc` を押すと開始を中止して Song Select に戻ります。

### Countdown
- loading 完了後、まず `3 / 2 / 1` のカウントダウンが表示されます。
- カウントダウン中の入力は score judgement には入りません。

## 7. プレイ中の操作

- 譜面キー入力: 現在の keymap に従う
- `Esc`: プレイ停止
- `F3`: Hi-Speed を下げる
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
- Combo
- 直近 judgement (`PG / GR / GD / BD / PR`)
- タイミングずれ（ms）

`Graphics > Performance HUD` を有効にすると、frame graph、average FPS、low FPS、gameplay timing debug 情報も見られます。

## 9. 判定とゲージ

### Judgement
- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

### Gauge
- `hard`
- `normal`
- `easy`

選択した gauge は曲開始時に常に `100%` で始まります。

- `hard`: `0%` で即 Game Over
- `normal`: `0%` で即 Game Over
- `easy`: `0%` で即 Game Over

automatic gauge shifting はありません。

## 10. Result 画面

- Clear / Game Over 状態
- Rank
- Score
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
- 必要なら `Skins > Note Width / Note Height / Judge Line` も調整する

### 判定は合っているのに見た目だけ遅い/早い
- `Graphics > Display Offset` を調整する
- 正の値はノートをより早く描画する

### BMS keysound が大きすぎる/小さすぎる
- `Audio > Keysound Volume` を調整する
- BGM と別々に調整できる

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
