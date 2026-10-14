# TenRiff Gameplay Guide

1.9.0リリースでは `Options > Key Mode Settings > Judge Window` から通常/Hard/EXを選択します。Easyは新しい選択肢から外し、過去のリプレイでは維持します。選択は実行中のみ有効で、再起動後は通常に戻ります。[EX判定と互換性](judge-ex-1.9.0.ko.md)（韓国語）。
- 新しい通常/Hardは `tenriff-native-score-v2-ruleset-5`、EXは `tenriff-native-score-v2-ruleset-6` を記録します。過去のR1–R5・Easy・LN規則は維持し、カスタム判定は非公式です。
- `Judge EX` はEASY基準PG/GR/GD=`14/44.571429/78.857143ms`（Hardの80%）でRANK比率を維持。BADは全RANKで250ms固定、別の自動ミス期限は340ms、スコア倍率はHardと同じ1.10です。

この文書は、TenRiff を初めて触るプレイヤーが実際にどうやって曲を選び、プレイし、結果を確認するかに焦点を当てたユーザーガイドです。

詳細な設定構造は [`docs/config.ja.md`](config.ja.md)、現在の実装範囲は [`docs/current-state.ja.md`](current-state.ja.md) を参照してください。

## 1. 初回起動

Windows では通常、次のどちらかで起動します。

```powershell
.\launch_win.bat
```

または

```powershell
.\TenRiff.exe --profile default
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

1.8.9のロビーはBESTの大きな主数値と最大点数・COMBO単位・詳細行を分けます。ノート数のラベルと値は白で、一覧と中央のアート/BEST/フィルターを白い外枠でまとめます。有効なStartは結果のContinueと同じ水色です。S+の色・書体は維持します。
- `Up` / `Down`: 曲移動
- `PageUp` / `PageDown`: 高速移動
- マウスホイール: 曲移動
- 左クリック: 曲選択
- ダブルクリック: 曲開始
- `Enter`: 現在選択中の曲を開始
- 中央の `MY BEST` カードで最高記録の Result を開き、リプレイボタンまたは F1 で再生します。
- 上部の `RECORDS` タブで現在の譜面のローカル記録と選択結果を開きます。
- `Left / Right`: ナビゲーションのフォーカス移動。Tab のクイック設定では上下で項目、左右で値を選びます。
- `Esc`: 前の画面に戻る
- `-` / `+`: 次の play の Rate を即時調整
- `F5`: 曲ライブラリを再インデックス。実行中は stage / percent / ETA と progress bar を中央表示
- 中央下部の難易度表カード: 名前・URL で編集、File でローカル JSON、Reset で標準 LV。Enter で URL 適用、Esc で取消。Sort / Filter 内の難易度表行も使用できます。

### Song Select からよく使う画面
- `Mode`
  - Ghost Battle、Autoplay、Practice、Sudden Death、Key Mode、Gauge、Random、Mods、Rate、Hi-Speed を調整
- `Audio`
  - Master / BGM / Keysound volume と BMS keysound policy を調整
- `Graphics`
  - VSync、Refresh Hz、Performance HUD、BGA 表示、external ONNX BGA Upscaler を調整
  - `BGA` を off にすると gameplay image/video background と decoder/upscaler 処理を無効化し、Song Select preview は維持
  - model 選択後に upscaler を明示的に ON にして high-spec 警告を確認する。実験的 `NPU 優先` は Windows/driver が実際に NPU を選択した場合だけ NPU を使う
- `Skins`
  - native / TenRiff `skin.json` / LR2 skin 切り替え、Visual Latency、hold depth と打鍵 glitch を持つ native 下部 digital-piano key、LR2 folder 単体または 独立した non-IIDX `LR2files/Theme` の一括 import（IIDX 依存 theme は除外）、field size に連動して拡大し判定線の下へ clip する aspect ratio 維持の下部 Gear frame、固定 divider 基準の note gap/size、Black Playfield、judge line 位置、LN body width、lane colors を調整
- `Keymap`
  - key binding 変更と NKRO test 実行

表の選択画面は F1 Aery 5K、F2 Aery 7K、F3 Revive 10K、F4 標準 LV を提供します。標準 LV は表示・並べ替え・グループ・数値フィルターに直ちに適用します。[曲管理](library-management.md)参照。

## 4. 初期設定のおすすめ

- `Mode > Gauge`: `normal`
- `Mode > Sudden Death`: 最初の OD8 換算 `MISS` で終了する challenge が必要なときだけ有効化
- `Mode > Rate`: `1.0x`
- `Mode > Hi-Speed`: まずは既定値
- `Graphics > Display`: Discord voice overlay を使う場合は `Borderless` を推奨
- `Graphics > Performance HUD`: 必要なときだけ on
- `Skins > Visual Latency`: 最初は `0ms`
- `Audio > Keysound Mode`: BMS では `follow` 推奨

ノートが遅すぎる/速すぎるなら、まず `Hi-Speed` だけを調整してください。判定は合っているのに見た目だけ遅い/早い場合は `Visual Latency` を調整します。

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
- `12K`: `Q W E R F V M I O P [ ]`
- `14K`: `Q W E R T F V M I O P [ ] \`
- `16K`: `Q W E R A S D F U I O P J K L ;`

合わない場合は `Options > Keymap` で変更できます。

## 6. プレイ開始前に知っておくこと

### Chart format
- chart の index/play は BMS family（`.bms/.bme/.bml/.pms`）専用で、`.osu` は非対応です。

### Loading
- 曲開始直後に chart-loading progress が表示されることがあります。
- 通常の loading 中は `Esc` で開始を中止して Song Select に戻ります。Session Mix は読み込みと開始カウントダウン中も Esc を無視します。

### Countdown
- loading 完了後、まず `3 / 2 / 1` のカウントダウンが表示されます。
- カウントダウン中の入力は score judgement には入りません。

## 7. プレイ中の操作

- 譜面キー入力: 現在の keymap に従う
- `Esc`: 通常の single-player を直ちに一時停止します。Continue または Esc で3/2/1カウントダウン後に再開し、その間は音声・譜面時刻・判定が停止します。再開カウントダウン中の Esc は一時停止メニューへ戻ります。
- Session Mix のプレイ中は Esc を無視します。曲間の結果画面の終了操作と multiplayer の Esc 中止は維持します。
- `F3`: Hi-Speed を下げる
- `F4`: Hi-Speed を上げる
- `F5`: Hi-Speedを半分にする
- `F6`: Hi-Speedを2倍にする
- `F7` / `F8`: 表示レイテンシーを減らす / 増やす
- `F11`: ゲーム中のチャットを開く / 閉じる（メニューもF11）
- 左右の `LShift` / `RShift` をプレイキーに設定できます。上記ショートカットにShiftは不要です。
- `F9`: 現在画面のスクリーンショット保存

ポーズの再開・再試行・終了と設定−/+ボタンもマウスで操作できます。調整行の文字・空白は選択のみで、値を変えません。

Pacemakerは精度・スコア目標との差をリアルタイム表示。通常のゲージ失敗・クリア・スコア計算を維持し、目標未達だけではクリアを失敗にしません。他の条件を満たせば最高記録・ランキングの対象.


Hi-Speed が変えるのは見た目のスクロール速度だけで、判定タイミングそのものは変えません。
Rate は曲の再生速度と譜面スケジュールだけを変え、同じ Hi-Speed で見た目のスクロール速度は変えません。

基準 BPM は累積進行時間が最も長いテンポです。同じ BPM の区間を合算し STOP 待機を除外します。Hi-Speed はこの基準に固定し、明示的な `#SCROLL`・停止・逆走効果は維持します。[計算規則](reference-bpm.md)を参照してください。

## 8. HUD の見方

**1.8.9 Studio Deck:** `Options > Skins`でNative・LR2ソロ用のStudio Deck/ClassicとStudio Riff Map表示を選びます。Studioでは曲情報とRate/HS/BPM/Scroll/Line/Latencyを右上、点数・精度・判定数を左下、64区間の横向き密度マップを右下に配置します。マップは左から右へ進み、現在位置と経過／全体時間を表示します。ゲージのレールとマーカーも拡大しました。

フィールド外の余白が狭い場合はカード全体を縮小し、より広い反対側へ移動します。点数とマップが同じ側なら点数を上へ移し、非常に狭いマップでは時間文字を省略します。極端なフィールド位置・幅とPerformance HUDによって両側上部の余白がなくなる場合は、上部情報カードだけを非表示にします。マップを隠しても他のカード位置は変わりません。LR2のノート・キー・ギアと、ゴースト・マルチ・他の画像スキンのClassic配置は維持します。[実行検証と制限](release-1.8.9-gate.md)です（韓国語）。

- Title / artist
- BPM
- 現在の `Rate`
- 現在の `Hi-Speed`
- Gauge 値と現在の gauge type
- TenRiff native Score
- Combo
- 直近 judgement (`PG / GR / GD / BD / PR`)
- タイミングずれ（ms）

中心からずれた入力は judgement の下に符号付きで表示されます。早い入力は `FAST -12 ms`、遅い入力は `SLOW +18 ms` となり、`0 ms` に丸められる入力では timing 表示を省略します。
`Skin Settings > FAST/SLOW Indicator` でこの表示と timing-history marker をまとめて切り替えられます。Off にしても judgement grade は表示されます。

`Graphics > Performance HUD` を有効にすると、frame graph、average FPS、low FPS、gameplay timing debug 情報も見られます。

### 判定・コンボ表示と音量

- P-GREAT のみ黄色・虹色のきらめき・ポップ演出を使います。GREAT は水色、GOOD は灰色で、下位判定の文字は動きません。
- FAST/SLOW は該当方向のみ表示し、P-GREAT または丸めて 0ms になる偏差では省略します。
- `Options > Skins` で判定 X/Y とコンボ X/Y を個別に調整・保存できます。
- `Options > Audio > Normalize Audio` はゲーム内ミックスの RMS 音量調整で、既定は OFF。メニュー音楽・選曲プレビューは変更しません。

## 9. 判定とゲージ

1.9.0リリースでは `Options > Key Mode Settings > Judge Window` から通常/Hard/EXを選択します。Easyは新しい選択肢から外し、過去のリプレイでは維持します。選択は実行中のみ有効で、再起動後は通常に戻ります。[EX判定と互換性](judge-ex-1.9.0.ko.md)（韓国語）。

### Judgement
- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

基本PGはBMS RANK EASY/NORMAL/HARD/VERYHARDでそれぞれ **±21/18/15/8ms** です。EASYのGR/GD/BADは±65/115/210msを維持し、他のRANKでは同比率で縮小します。`Judge Hard/EX` はRANK適用後の別MODです。全判定幅と境界を含む条件は[判定範囲（韓国語）](judgement-windows.md)を参照してください。

空POOR は次の note より早い側だけに適用します。消費済み、または判定線を通過済みの note の後ろで入力しても後ろ空POORは追加しません。

`No LN Release` は charge LN 終端の release judgement を無効化し、score multiplier は正確に `1.00x` です。

OD8 換算統計は Sudden Death と既存 replay 互換のため JSON 内部に維持されますが、Gameplay / Result 画面には表示しません。

Casual Score は最大 10,000 点で、judgement weight は `PG 6 / GR 3 / GD 1 / PR 0 / FAIL 0` です。LN は head / tail を各 0.5 weight とする 1 object で、Detail Score は別の計算を維持します。

通常の Accuracy は `PG / GR / GD / BD / PR = 100 / 80 / 50 / 20 / 0%` の判定加重平均です。判定区間内のタイミングを反映する詳細精度とは別で、通常の Accuracy にタイミングのばらつきによる追加減点や99.5%の上限は適用しません。

**1.9.0 リリースの Rank:** 生存して完走した場合、通常の Accuracy が `99%以上 S++ / 98%以上 S+ / 95%以上 S / 93%以上 A+ / 90%以上 A / 85%以上 B / 80%以上 C / 80%未満 D` です。75%未満でも生存完走ならD、失敗はFです。境界には表示用の丸め前の精度を使います。

明示的な中断（`ABORTED` または `aborted`）は `--`、自動演奏は `AUTO`、記録なしは `--` と表示します。死亡と中断を区別する情報がない旧失敗記録は、従来の失敗状態に従ってFのままです。結果・履歴・BESTの表示ランクだけを統計から再計算し、元の記録・得点計算・数字の順位は変更しません。固定基準1.8.7も維持します。[表示と互換性の詳細（韓国語）](grade-display.ko.md)。

### Gauge

Gauge Shift は常に有効です。`EX / Hard / Normal / Easy` は開始段階で、保存値は `ex_hard / hard / normal / easy` です。選択段階と下位段階をそれぞれ 100% から並列計算します。現在の段階が 0% で脱落すると、同じ判定履歴を累積した次の生存段階へ移り、終了時の最上位生存段階が結果になります。対象段階がすべて脱落するとゲージ失敗です。

段位/Session Mix は曲間で引き継ぐ LR2 参照ゲージを使用し、間接ミスを維持して HP 2% 未満で失敗します。通常 LN は完走/解除時に一度だけ反映し、通常の `gauge.delta` は適用しません。[比較と互換範囲](lr2-gauge-audit.ko.md)を参照してください。

旧 `shift` は EX 開始として解釈します。Practiceは独自の終了規則を維持します。Pacemakerは通常のゲージ・クリア規則に従います。

`Sudden Death (1 MISS)` は gauge type ではなく、最初の OD8 換算 object `MISS` で gauge を 0 にして即終了する rule です。native `BAD` timing だけでは発動せず、空打ちの `POOR` も対象外で、Practice No-Fail と同時には有効化できません。

## 10. Result 画面

1.8.9のソロ結果は`SUMMARY / LANES / NOTE TYPES / TIMELINE`ボタンで分析を切り替えます。緑の分析枠、青いタブ、水色のContinueを区別し、Replay/Retryのアイコンと文字をまとめて中央に配置します。左側は共通外枠と内部区分を使い、MAX COMBOの数値と単位を分けます。文字拡大は表示の変更で、点数・分析の定義は維持します。実画面・操作の検証状況は[リリース記録](release-1.8.9-gate.md)を参照してください。

- Clear / Game Over 状態
- Rank
- TenRiff native Score
- 記録された play で pause を使用したか
- Accuracy
- Max Combo
- PG / GR / GD / BD / PR の合計
- 平均 timing deviation と標準偏差
- 最終 gauge と gauge 履歴
- 保存された replay / result file 名

戻るキー:
- `R / Left`: 同じ譜面を即再開
- `F1`: 保存 replay がある場合に再生
- `Enter`: Song Select に戻る
- `Esc`: Song Select に戻る

## 11. よくある調整

### ノートが詰まって見え、読みにくい
- `Hi-Speed` を上げる
- 必要なら `Skins > Note & Field Size / Note Height / Judge Line` も調整する

### 判定は合っているのに見た目だけ遅い/早い
- `Skins > Visual Latency` を調整する
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
4. まだ違和感があるなら `Skins > Visual Latency` を調整する
5. 手の配置が合わなければ `Keymap` を変える
6. 最後に `Skins` で note size と judge line 位置を調整する

## 13. 関連文書

- 現在の実装状態: [`docs/current-state.ja.md`](current-state.ja.md)
- 設定構造: [`docs/config.ja.md`](config.ja.md)
- menu 構造: [`docs/menu.ja.md`](menu.ja.md)
- play loop / engine 構造: [`docs/core-loop.ja.md`](core-loop.ja.md)
