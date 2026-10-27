> **2026-10-27 · 1.9.9.3:** 長押し調整・LR2CRSコース・BMS/KPSオーバーレイ・クリアカード・ランク色・EX虹色修正を含みます。一般固定基準は **1.9.6**。下記の旧版・ローカル候補説明は当時の履歴で、現在の範囲はこの文書を優先します。 [1.9.9.3](release-1.9.9.3.ja.md) · [Verification](release-1.9.9.3-gate.md).

> **2026-10-24 · 1.9.9.1 リリース範囲:** 21:9画面・LN途中解除表示と最後の末尾待機・同レーン地雷の前後50msを空けるLN Mix v3。[変更と互換性](release-1.9.9.1.ja.md) · [検証ゲート](release-1.9.9.1-gate.md)。一般固定基準は **1.9.6**。旧v1/v2リプレイを維持し、v3には1.9.9.1検証器が必要。以下の旧案内は当時の履歴であり、公開状態とダウンロードは[1.9.9.1リリース](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.1)を基準とする。

> **2026-10-22 · 1.9.9 · NK2 E / 16K送信:** [1.9.9](release-1.9.9.ja.md) · [Verification](release-1.9.9-gate.md). Baseline **1.9.6**. 4/5/6/7/8/9/10/12/14/16K. Earlier dated entries below are historical.

> **2026-10-21 · 1.9.8.5 リリース範囲:** ユーザー指定の1.9.8にBGA再生・黒の透過、選曲循環・ライブラリ・クリア表示、小数のタイミング、スクラッチなし7Kと第2キー所有権の修正を追加します。一般固定基準は **1.9.6** のままです。以下の旧案内は当時の履歴です。動作と互換性は[1.9.8.5](release-1.9.8.5.ja.md)、検証は[リリースゲート](release-1.9.8.5-gate.md)、公開状態とダウンロードは[リリースページ](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5)を参照してください。

> **2026-10-19 · 1.9.8:** 1.9.7を基に、判定・コンボ・FAST/SLOWインジケーター・バーの各Y設定の直前にX設定を表示し、キー区切り線の明るさを0～100%で1%刻みに調整できます。 1.9.7の共通キー、拡大した色付きタブ、毎回の新シード、シーン音楽の基準ゲインを含み、一般固定基準は1.9.6を維持します。 [1.9.8](release-1.9.8.ja.md) · [X/Y](skin-xy-20261019.ja.md) · [Baseline 1.9.6](baseline-1.9.6.ja.md) · [Verification](release-1.9.8-gate.md).

以下の以前のバージョン・日付の案内は当時の履歴です。現在の範囲は1.9.8.5文書を参照し、公開状態とダウンロードは各版のリリースページを基準とします。

> **2026-10-19 · 1.9.7:** 共通キータブ、拡大したモードタブと選曲色、FAST/SLOWの色、1%調整、ゲームモード設定・Normal表示、毎回の新シードを含む。 公開状態とダウンロードは各バージョンのリリースページを参照。以前の日付の案内は当時の履歴。 [1.9.7](release-1.9.7.ja.md) · [Baseline 1.9.6](baseline-1.9.6.ja.md) · [Verification](release-1.9.7-gate.md).

**2026-10-18 · 1.9.6:** [固定判定・スキンタブと検索・ウィンドウドロップ](release-1.9.6.ja.md) · [検証範囲](release-1.9.6-gate.md)。Sitesは標準設定の新規Online R9/R10のみ受け付け、旧ボード・記録はレガシー読み取り専用で保持します。別Rankedの未対応規則とGuest・カスタム除外は維持します。

[固定判定表](judgement-baseline-20261018.ja.md)：通常PG22/19/15/10msと従来の正確なGR/GD/BADを維持します。HardはすべてのファイルRANKで通常NM全体、EXは通常VHD全体を使い、整数msへ丸めません。一般ソース基準は **1.9.0** です。

[スキンUIの動作と操作](skin-ui-followup-20261018.ja.md)。以下の旧版・ローカル候補の説明は当時の履歴であり、現在の範囲は1.9.6文書に従います。

**2026-10-18 · 1.9.5.3:** [スキン設定整理と検索・入力の改善](release-1.9.5.3.ja.md). プレビューの番号ボタンで編集レーンを選択します。UIフォント、フォントファイル、ゲーム中カーソルはプロフィールで設定します。スクラッチ位置は実行時キーモードが「Original」の場合だけ表示し、7+1配置で調整できます。

**2026-10-17 · 1.9.5.2:** [ノート・フィールドサイズ10%](release-1.9.5.2.ja.md). GOMazk氏の貢献(PR #102)を含みます。Note & Field Sizeの範囲を50–140%から10–140%へ拡張しました。既定100%、保存済みの値とキーモード別設定を維持し、メニュー・プレイ・ゴースト・プレビューで同じ下限を使います。判定・スコア・入力・音声の計算と1.9.5.1の350ms FAST/SLOW・ファイル選択動作は維持します。

**2026-10-17 · 1.9.5.1:** [判定ライン・FAST/SLOW・ファイル選択](release-1.9.5.1.ja.md). FAST/SLOWは最大判定範囲外の最後の入力から0.35秒残ります。最大判定でも即座に消えず、最大判定の連打で表示時間が延長されることもありません。新しい範囲外判定が出れば即座に更新します。常時表示の目盛りと点/バーの0.7秒寿命は維持します。

**2026-10-17 · 1.9.5:** メニュー・結果音楽も選択した出力先を使用します。FASTは青、最高判定は緑、LATEは赤。最高判定でもFAST/SLOWを更新します。ロビーはOFF → ミラー → ランダム → Sランダム → スクラッチ固定ランダム → OFFの順です。 [音楽の出力先統合と判定インジケーター](release-1.9.5.ja.md).

**2026-10-17 · 1.9.4:** プレイ中は音源と静止 BGA を保持し、強制メモリ整理を保留します。インジケーター形状・0.7秒表示、出力機器、プロファイルキー、×0.5/×2、小節線整列を含みます。 [メモリ解放のタイミングとプレイ設定](release-1.9.4.ja.md).

**2026-10-16 · 1.9.3リリース範囲:** 空POORの精度除外(R7/R8)、BMS判定バッジ・ゲーム中レベル・小節線、FAST/SLOW常時表示修正、安全な読み込みの既定化、残差/完成画像ONNXと対応構造の外部Safetensors変換を含みます。実行・ソース配布にBGA重みを含めず、OFF/空パスが既定です。ゲスト送信除外・固定等級・基準1.9.0を維持します。[変更と互換性](release-1.9.3.ja.md) · [検証手順](release-1.9.3-gate.md)。公開状態とダウンロードは[1.9.3リリースページ](https://github.com/10kseason/TenRiff/releases/tag/1.9.3)を基準とし、下の日付付き説明は当時の履歴です。

> 旧1.9.2: [ゲスト/オンライン・原寸アップスケール・UI変更](release-1.9.2.ja.md)。

> 2026-10-16: [FFmpegデブロックと外部ONNX x2](deblock-lunasr-20261016.ja.md)

> 2026-10-16: [BGAを元の解像度で復元してから表示領域に合わせる](native-upscale-20261016.ja.md)

# TenRiff Development Roadmap (staged)

**2026-10-16 · 1.9.1.1 UI:** [ホーム・グラフィック・読み込み・固定等級 — 1.9.1.1後続候補](ui-feedback-20261016.ja.md).

**2026-10-15 · 1.9.1.1 BMSオンライン対応:** 新しいBMSゲージ・タイミング・RANDOM分岐の記録をSitesコミュニティランキングへ送信し、規則と選択された分岐内容ごとに順位を分けます。新クライアントと対応するWeb配信が必要です。Sitesは引き続き`client_submitted`記録であり、別系統のRanked検証サーバーの制限は維持します。下の1.9.1のローカル専用表記は同版の履歴です。公開済みファイルと固定基準1.9.0を保持します。 [範囲・互換性・検証](feedback-1.9.1.1.ja.md)。

**2026-10-15 · 1.9.1:** ユーザー承認の1.9.1に、先の音量・入力・後奏修正、BMS TOTAL/RANDOM互換性、結果の色と10マークを含めます。ホームの言語ボタンはEN → KO → JA → ZHの順に切り替えて保存します。新しいBMSゲージ・分岐記録はサーバー検証が対応するまでローカル専用です。固定基準は1.9.0で、旧リリース・インストール・プロファイルを保持します。下の日付付きローカル記録の版維持・未公開表記は当時の状態であり、現在はこの1.9.1の範囲を基準とします。 [変更・互換性](feedback-1.9.1.ja.md) · [リリース検証手順](release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL・RANDOM・結果/ロゴの追加変更 — 未公開・ローカルビルドとCTest回帰検証通過:** 1.9.0.2を基にしたローカル作業です。結果のレベル表示を選曲ロビーと同じ赤いレベル文字・キーモード別の記号色に揃え、ロゴを提供された10形のマークへ変更します。新しい通常BMSプレイは#TOTALとノート数に基づくLR2系の回復・ダメージを使い、譜面内#RANDOMの分岐はレーンランダムとは独立したシードで選択します。過去のリプレイは元の動作を維持します。先の音量統合も含みます。公開済み1.9.0.2・固定基準1.9.0・バージョン番号は変更しません。 [計算式・分岐・互換性と検証範囲（英語）](bms-total-random-followup.en.md).

**2026-10-15 · BMS音量統合の追加変更 — 未公開・ローカルビルドとCTest回帰検証通過:** 1.9.0.2を基にしたローカル作業で、伴奏・キー音の個別調整をBMS演奏音量（韓国語表示：**BMS 연주 볼륨**）へ統合します。新規既定値は20%で保存値は保持します。マスター10%・TenRiff音楽100%の既定値、残る音量の1パーセントポイント刻み、ロビー試聴×0.75は維持します。バージョン番号と固定基準1.9.0は変更しません。以下の日付付き項目は当時の記録です。 [設定・互換性](config.ja.md#audio).

この未公開の追加変更では難易度表の読み込み状態を表示し、debounceのUIを1ms刻み・最大20ms（既定0ms）にします。従来の5→8ms自動補正は廃止し、保存した5msと8msをそれぞれ保持します。スキンの韓国語表示名は `LN 끝 줄임` となり、形状・判定動作は変わりません。通常のシングルプレイで最後まで聴く設定が有効なら、最後のノート・LN終端処理後の `Esc` は一時停止を開かず後奏を飛ばして結果へ進みます。

**2026-10-15 · 1.9.0.2 リリース候補 — 公開検証手順:** 公開済み1.9.0.1を基に、Optionsを緑色、組み込み5K Aery表の選択時だけ緑の⑤を表示し、登録済み4/6/8/10キーモード表の記号を対応色にし、レベルを赤・スコアを白にし、`lv:` レベル検索、新規debounce既定値0ms、音量の1%刻み調整を追加します。選曲の並び替え・フィルター・難易度表選択をSong Library横のヘッダーへ移し、アルバム画像の高さを広げます。マスター既定値10%と固定基準1.9.0は維持します。[変更・使い方](feedback-1.9.0.2.ja.md)。 公開状況とダウンロードはリリースページを基準に確認してください。[範囲・検証](release-1.9.0.2-gate.md)。

**2026-10-15 · 1.9.0.1:** マスター音量の既定値10%とロビーアイコンの中心揃えを含みます。プレビュー×0.75、プロフィールで指定するF5/F6 -10/+10、Xbox/XInputボタン入力、ファイル選択の重複防止、BMS BPM/STOP順序修正と旧リプレイ互換を提供します。固定基準は1.9.0を維持します。 [変更内容](feedback-20261015.ja.md) · [リリース検証範囲](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 新しい固定基準:** 確認済みのローカル候補を指定された正式版へ昇格します。BMS/メニュー音量分離、ロビー・フィルター・スキン操作、ランク・SUBARTIST、韓国語モード表示、EX判定、送信状態を含みます。通常/HardはR5、EXはR6。旧R1–R5記録と保存設定を保持します。[固定基準](baseline-1.9.0.ja.md) · [検証範囲](release-1.9.0-gate.md)。以下の日付付き項目は当時の履歴です。

**2026-10-14 · 1.8.9.7:** 入力スレッドにMMCSS Games/Highと実行速度の電力スロットリング無効化を適用し、ホームのメニュー枠にアイコン色の控えめなglowを加えます。TenRiff独自のメニュー・結果音楽だけを従来の40%に下げ、BMS楽曲・キー音・試聴は元の音量を維持します。固定基準は1.8.7のままで、物理入力遅延の改善は未確認です。 [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 入力とゲーム進行/HUDの短い待機に再利用する高分解能タイマーを使い、RawInputはメッセージで起床します。比較版Bの方式を適用し、判定幅・R1–R5・リプレイ・固定基準1.8.7を維持します。実プレイの感触は回ごとに異なり、すべての引っ掛かりの解消を保証しません。 [Scope / 검증](release-1.8.9.6-gate.md).

**固定基準: [1.9.0](baseline-1.9.0.ja.md)**（2026-10-14の明示的な依頼）。Git tag `1.9.0`を使い、`main`や`latest`に合わせて自動更新しません。旧1.8.7基準と既存リリースは履歴として保持します。

**2026-10-13 · 1.8.9.4:** リザルトの大きなランクとスコアの配置・詳細精度を整理し、Gauge Shift CLEARの4色を区別します。BGM・キー音の音量をAudioの先頭に移し、新規設定では両方100%にします。韓国語スキン名称、明記されたobjクレジットとBPM!変速表示、同じ譜面時刻の同一音源の重複再生補正を含みます。譜面作者の送信は任意で、既存の記録IDを維持します。[変更・互換性・検証範囲（韓国語）](release-1.8.9.4-gate.md)。検証とゲーム・ウェブ公開の状態は別途記録し、固定基準は1.8.7です。

**2026-10-13 · 1.8.9.3:** リザルトの文字サイズ、ロビーの色と控えめなBloom、スキンのレーン色に従うキー設定、曲一覧のレベル色を調整しました。インジケーター背景と対象間隔・ビジュアルプリセット項目を削除し、対象レーンをノート色の直上に移動しました。[変更と検証範囲（韓国語）](release-1.8.9.3-gate.md)。固定基準1.8.7と1.8.9.2のBMS補正・リプレイ互換性を維持します。

**2026-10-13 · 1.8.9.2:** BMSの同じレーン・時刻に重複した通常ノートとロングノートの始点を、Native・変換の前に1本のロングノートへ補正します。原本、LNの長さとキー音を保持し、旧リプレイは当時の配置を維持します。[動作・互換性の制限（韓国語）](release-1.8.9.2-gate.md)。固定基準は1.8.7です。

**2026-10-13 · 1.8.9.1:** 連続キー設定、全変換器のキー音・ロングノート保持、FAST/SLOW・タイミング背景切替、ゲージ拡大、プレビューのギア移動と判定文字の枠・背景除去を追加しました。既存リプレイは元の変換を維持します。[動作・検証・互換性の制限（韓国語）](release-1.8.9.1-gate.md)。固定基準は1.8.7です。

> 文書の範囲: 初期設計・改善案・当時の分析を含みます。未完了の提案や過去の測定値は現在の挙動・既定値・性能保証ではありません。[現在の状態](current-state.ja.md)と[設定](config.ja.md)を参照してください。

この roadmap は、スコープ肥大化を避けながらゲームループを拡張していくための推奨高位順序をまとめたものです。各段階でまず方向を固定し、その上に追加機能を積み重ねます。

## Current Baseline
- Windows GUI / runtime が主なサポート経路です。
- 固定基準は `1.9.0`、このソースはリリース `1.9.2`。公開 package は NK3 P64 と generalized pattern MLP inference model を同梱するが、BGA upscaler model は同梱しない。互換性と権利を確認済みの upscaler ONNX を選択しても path を保存するだけで、BGA Upscaler は user が有効化して high-spec warning を確認するまで off のまま。自動 benchmark gate はない。
- 現行 menu / runtime は BMS family（`.bms/.bme/.bml/.pms`）専用で、native、bundle/profile の TenRiff `skin.json`、LR2 skin をサポート。
- 現在出荷されている挙動は [`docs/current-state.ja.md`](current-state.ja.md) を先に見てください。この roadmap は方向と残作業を示す文書です。

## 1.9.0.2リリース候補の検証範囲

以下は最終的なAery ⑤・登録表の記号色・選曲配置調整前の候補での検証結果です。開発ビルドとZIP展開ソースの独立ビルドの両方で、Releaseビルド、CTest 3/3（各1,224件、integration skips 0）、エディター35/35、native MenuApp/GameSession 307/307が通過しました。native検査には検索キー処理22件を含みます。復元前の描画は6 fixture/360 Presents/86 geometry hit checksを通過しました。Aery ⑤と4UE/6UE/8UE/Ⓡの色調整後のビルド・表示確認はリリース検証項目です。AeryのFHD/小さい画面、数値レベルの赤維持、既定Lv・他の表・カスタム表への非適用、新ヘッダー選択欄・拡大画像・ベスト記録・曲数の重なりも確認対象です。

初回パッケージ検査では既存のスキンレーン数字ショートカット検査が一度失敗しました。同一テスト実行ファイルの変更なし再検査、CTest全体の再実行、独立ビルド検査は通過しましたが、**根本原因は未確定**です。初期失敗も保全し、再実行の成功を間欠的原因の修正とは扱いません。

ローカルパッケージ・独立ビルドの根拠と初期失敗は別のevidenceに保全します。最終公開パッケージの対応、PR/main CI、公開アセットの確認は[リリース検証手順](release-1.9.0.2-gate.md)に従い、この文書では事前に通過としません。公開状況はリリースページが基準です。内部キーdispatch・geometry検査は実機入力の検証を代替せず、実機・遅延・長時間プレイ・別PCは未検証です。固定基準は1.9.0です。

## 1.9.0.1以降の確認範囲

今回はプレビュー音量・Hi-Speedキー・ファイル選択・BMSタイミング・XInputボタンに限定します。実機パッドと長時間プレイは別検証、タイミングバージョン2のWeb対応は後続作業です。Webが両タイミングを区別するまで、影響のある新記録はローカルに保持します。固定基準1.9.0とプラグイン・構造分離の既存範囲は維持します。

## 1.9.0の安定化範囲と今後

1.9.0は確認済み候補の配布と固定基準更新です。音量・設定・画面・クレジット・EX/R6・送信診断が範囲で、全PCの遅延解消は保証しません。旧計画のプラグイン先行条件は今回の指定範囲に置き換えます。プラグイン実行環境/API/SDKとGameSessionの大規模分離は未実装の別作業です。将来の拡張は入力・音声・判定・公式記録の所有権を維持する読み取り専用機能から検討します。不具合・互換性・セキュリティ改善を継続し、次の基準変更も明示的な依頼と検証を必要とします。


## 0) 骨格と master clock を固める
- すべての時間敏感処理で **AudioThread を master clock** として扱う。
- **InputThread** は RawInput / evdev から event に timestamp を付け、SPSC queue に積む。
- 譜面 timeline は **sample positions (int64)** へ正規化し、audio thread が決定的 timestamp を消費できるようにする。
- **Render** は snapshot を描くだけに限定し、judgement / score は audio 側で確定させる。

## 0.5) 低遅延ループを強化する
- 1 つの backend（WASAPI / ALSA）を end-to-end で仕上げ、device padding を露出させ、`buffer_start_samples` を明示計算して mixer が playback-buffer domain で動くようにする。
- ClockSync は外れ値除去、sliding-window EMA 更新、device 変更 / underrun reset hook、monotonic clamp で強化する。
- AudioThread では入力を取り出して sample time に変換し、late / normal / future に分岐して keysound を配置する。
- 判定 window を sample で表現し、callback budget / late inputs / xruns の HUD カウンタを出し、rate 可変時は window を適切にスケーリングする。
- ✅ replay を sample-time input trace（lane / state / sample）として保存し、決定的再現のための JSON export を書き出す。

## 1) 1 曲を end-to-end で遊べるようにする
- menu からも audio backend を silent callback 付きで動かし、gameplay 前から master clock を安定させる。
- ✅ UI state machine（console）として **Title -> Song Select -> Play -> Result** を InputThread / SPSC とともに立ち上げた。
- ✅ Windows D3D11 menu UI（text + background + focus styling）を追加した。
- ✅ async SongIndexerThread と cached index（mtime / hash）を追加し、Song Select がスキャン中も応答するようにした。
- 最小 BMS loader（essential channel のみ）-> note scheduling -> judgement -> result screen を通す。
- preview audio は audio engine で予約し、Song Select 中に keysound を preload する。

## 1.5) BMS-only chart support の強化
- 以前の multi-format 方針は現行 release line では superseded。loading、indexing、replay、result、difficulty-table は BMS family に集中する。
- archive や別 format の import path を再導入せず、real-pack の encoding、keysound、BGA、long note、lane layout 互換性を強化する。
- optional integration は user-supplied、明示的に有効化するまで disabled、失敗時は native behavior へ安全に復帰できる形を維持する。

## 2) Key remap と 8K / 10K modes
- ✅ 「リマップ UI フロー」仕様どおりの key remapping UI（NKRO test を含む）。
- ここまで来ると、プロジェクトは十分実用的な個人練習ツールになる。

## 3) Lane transform / Random mode
- ✅ **Mirror**、**Full Random (FR)**、**Super Random (SR)** は実装済み。**AR** は挙動を定義するまで後回し。

## 4) Launcher を付ける
- folder check、初回 config 生成、error code 整理を扱う。
- これが終わると、ゲームはローカル PC 上で自己完結する。

## 5) 公開 ranking より先に信頼できる記録を作る
- [`ranked-integrity-plan.en.md`](ranked-integrity-plan.en.md) に従い、共通 eligibility reason、Local Records 画面、read-only Online Records、shadow submission、公開 verified ranking の順で進める。
- server は承認済み BMS chart SHA-256 と replay evidence から結果を再計算し、client の score claim を信頼境界にしない。
- `.osu` chart と osu 派生 import / ruleset / scoring / conversion は ranked 登録不可。native BMS の補助 OD8 統計は local metadata のみ。

## 6) 規模拡大に合わせて menu architecture を refactor する
- `1.6.0` で Phase 0-6 を完了した。明示的 navigation、全 settings family の typed controller、exhaustive screen descriptor が現在の保守対象 architecture となる。
- [`menu-refactor-plan.md`](menu-refactor-plan.md) の段階設計と phase gate に従い、menu 全体の一括 rewrite から始めない。
- 新しい screen の追加で input dispatcher、render-data builder、複数の共有 cursor/return flag を同時に変更する必要が生じるか、screen behavior を focused unit test で覆えなくなった時点で着手する。
- 先に navigation、settings persistence、pointer/keyboard parity の characterization test を追加し、visible behavior を変えずに `MenuApp` から screen family ごとに local state/controller へ移す。
- 共有 return-screen flag は明示的な navigation stack/back policy に置き換え、slider を含む adjustable row は row index 条件ではなく typed value range と persistence callback で表現する。
- render DTO 構築と input/state mutation を分離する。各 extraction は focused test と通常の client build を通してから次へ進み、menu 全体の一括 rewrite は行わない。
