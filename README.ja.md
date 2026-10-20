> **2026-10-21 · 1.9.8.5 リリース範囲:** ユーザー指定の1.9.8にBGA再生・黒の透過、選曲循環・ライブラリ・クリア表示、小数のタイミング、スクラッチなし7Kと第2キー所有権の修正を追加します。一般固定基準は **1.9.6** のままです。以下の旧案内は当時の履歴です。動作と互換性は[1.9.8.5](docs/release-1.9.8.5.ja.md)、検証は[リリースゲート](docs/release-1.9.8.5-gate.md)、公開状態とダウンロードは[リリースページ](https://github.com/10kseason/TenRiff/releases/tag/1.9.8.5)を参照してください。

> **2026-10-19 · 1.9.8:** 1.9.7を基に、判定・コンボ・FAST/SLOWインジケーター・バーの各Y設定の直前にX設定を表示し、キー区切り線の明るさを0～100%で1%刻みに調整できます。 1.9.7の共通キー、拡大した色付きタブ、毎回の新シード、シーン音楽の基準ゲインを含み、一般固定基準は1.9.6を維持します。 [1.9.8](docs/release-1.9.8.ja.md) · [X/Y](docs/skin-xy-20261019.ja.md) · [Baseline 1.9.6](docs/baseline-1.9.6.ja.md) · [Verification](docs/release-1.9.8-gate.md).

以下の以前のバージョン・日付の案内は当時の履歴です。現在の範囲は1.9.8.5文書を参照し、公開状態とダウンロードは各版のリリースページを基準とします。

> **2026-10-19 · 1.9.7:** 共通キータブ、拡大したモードタブと選曲色、FAST/SLOWの色、1%調整、ゲームモード設定・Normal表示、毎回の新シードを含む。 公開状態とダウンロードは各バージョンのリリースページを参照。以前の日付の案内は当時の履歴。 [1.9.7](docs/release-1.9.7.ja.md) · [Baseline 1.9.6](docs/baseline-1.9.6.ja.md) · [Verification](docs/release-1.9.7-gate.md).

**2026-10-18 · 1.9.6:** [固定判定・スキンタブと検索・ウィンドウドロップ](docs/release-1.9.6.ja.md) · [検証範囲](docs/release-1.9.6-gate.md)。Sitesは標準設定の新規Online R9/R10のみ受け付け、旧ボード・記録はレガシー読み取り専用で保持します。別Rankedの未対応規則とGuest・カスタム除外は維持します。

[固定判定表](docs/judgement-baseline-20261018.ja.md)：通常PG22/19/15/10msと従来の正確なGR/GD/BADを維持します。HardはすべてのファイルRANKで通常NM全体、EXは通常VHD全体を使い、整数msへ丸めません。一般ソース基準は **1.9.0** です。

[スキンUIの動作と操作](docs/skin-ui-followup-20261018.ja.md)。以下の旧版・ローカル候補の説明は当時の履歴であり、現在の範囲は1.9.6文書に従います。

**2026-10-18 · 1.9.5.3:** [スキン設定整理と検索・入力の改善](docs/release-1.9.5.3.ja.md). プレビューの番号ボタンで編集レーンを選択します。UIフォント、フォントファイル、ゲーム中カーソルはプロフィールで設定します。スクラッチ位置は実行時キーモードが「Original」の場合だけ表示し、7+1配置で調整できます。

**2026-10-17 · 1.9.5.2:** [ノート・フィールドサイズ10%](docs/release-1.9.5.2.ja.md). GOMazk氏の貢献(PR #102)を含みます。Note & Field Sizeの範囲を50–140%から10–140%へ拡張しました。既定100%、保存済みの値とキーモード別設定を維持し、メニュー・プレイ・ゴースト・プレビューで同じ下限を使います。判定・スコア・入力・音声の計算と1.9.5.1の350ms FAST/SLOW・ファイル選択動作は維持します。

**2026-10-17 · 1.9.5.1:** [判定ライン・FAST/SLOW・ファイル選択](docs/release-1.9.5.1.ja.md). FAST/SLOWは最大判定範囲外の最後の入力から0.35秒残ります。最大判定でも即座に消えず、最大判定の連打で表示時間が延長されることもありません。新しい範囲外判定が出れば即座に更新します。常時表示の目盛りと点/バーの0.7秒寿命は維持します。

**2026-10-17 · 1.9.5:** メニュー・結果音楽も選択した出力先を使用します。FASTは青、最高判定は緑、LATEは赤。最高判定でもFAST/SLOWを更新します。ロビーはOFF → ミラー → ランダム → Sランダム → スクラッチ固定ランダム → OFFの順です。 [音楽の出力先統合と判定インジケーター](docs/release-1.9.5.ja.md).

**2026-10-17 · 1.9.4:** プレイ中は音源と静止 BGA を保持し、強制メモリ整理を保留します。インジケーター形状・0.7秒表示、出力機器、プロファイルキー、×0.5/×2、小節線整列を含みます。 [メモリ解放のタイミングとプレイ設定](docs/release-1.9.4.ja.md).

**2026-10-16 · 1.9.3リリース範囲:** 空POORの精度除外(R7/R8)、BMS判定バッジ・ゲーム中レベル・小節線、FAST/SLOW常時表示修正、安全な読み込みの既定化、残差/完成画像ONNXと対応構造の外部Safetensors変換を含みます。実行・ソース配布にBGA重みを含めず、OFF/空パスが既定です。ゲスト送信除外・固定等級・基準1.9.0を維持します。[変更と互換性](docs/release-1.9.3.ja.md) · [検証手順](docs/release-1.9.3-gate.md)。公開状態とダウンロードは[1.9.3リリースページ](https://github.com/10kseason/TenRiff/releases/tag/1.9.3)を基準とし、下の日付付き説明は当時の履歴です。

> 旧1.9.2: [ゲスト/オンライン・原寸アップスケール・UI変更](docs/release-1.9.2.ja.md)。

> 2026-10-16: [FFmpegデブロックと外部ONNX x2](docs/deblock-lunasr-20261016.ja.md)

> 2026-10-16: [BGAを元の解像度で復元してから表示領域に合わせる](docs/native-upscale-20261016.ja.md)

# TenRiff

**2026-10-16 · 1.9.1.1 UI:** [ホーム・グラフィック・読み込み・固定等級 — 1.9.1.1後続候補](docs/ui-feedback-20261016.ja.md).

**2026-10-15 · 1.9.1.1 BMSオンライン対応:** 新しいBMSゲージ・タイミング・RANDOM分岐の記録をSitesコミュニティランキングへ送信し、規則と選択された分岐内容ごとに順位を分けます。新クライアントと対応するWeb配信が必要です。Sitesは引き続き`client_submitted`記録であり、別系統のRanked検証サーバーの制限は維持します。下の1.9.1のローカル専用表記は同版の履歴です。公開済みファイルと固定基準1.9.0を保持します。 [範囲・互換性・検証](docs/feedback-1.9.1.1.ja.md)。

**2026-10-15 · 1.9.1:** ユーザー承認の1.9.1に、先の音量・入力・後奏修正、BMS TOTAL/RANDOM互換性、結果の色と10マークを含めます。ホームの言語ボタンはEN → KO → JA → ZHの順に切り替えて保存します。新しいBMSゲージ・分岐記録はサーバー検証が対応するまでローカル専用です。固定基準は1.9.0で、旧リリース・インストール・プロファイルを保持します。下の日付付きローカル記録の版維持・未公開表記は当時の状態であり、現在はこの1.9.1の範囲を基準とします。 [変更・互換性](docs/feedback-1.9.1.ja.md) · [リリース検証手順](docs/release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL・RANDOM・結果/ロゴの追加変更 — 未公開・ローカルビルドとCTest回帰検証通過:** 1.9.0.2を基にしたローカル作業です。結果のレベル表示を選曲ロビーと同じ赤いレベル文字・キーモード別の記号色に揃え、ロゴを提供された10形のマークへ変更します。新しい通常BMSプレイは#TOTALとノート数に基づくLR2系の回復・ダメージを使い、譜面内#RANDOMの分岐はレーンランダムとは独立したシードで選択します。過去のリプレイは元の動作を維持します。先の音量統合も含みます。公開済み1.9.0.2・固定基準1.9.0・バージョン番号は変更しません。 [計算式・分岐・互換性と検証範囲（英語）](docs/bms-total-random-followup.en.md).

**2026-10-15 · BMS音量統合の追加変更 — 未公開・ローカルビルドとCTest回帰検証通過:** 1.9.0.2を基にしたローカル作業で、伴奏・キー音の個別調整をBMS演奏音量（韓国語表示：**BMS 연주 볼륨**）へ統合します。新規既定値は20%で保存値は保持します。マスター10%・TenRiff音楽100%の既定値、残る音量の1パーセントポイント刻み、ロビー試聴×0.75は維持します。バージョン番号と固定基準1.9.0は変更しません。以下の日付付き項目は当時の記録です。 [設定・互換性](docs/config.ja.md#audio).

この未公開の追加変更では難易度表の読み込み状態を表示し、debounceのUIを1ms刻み・最大20ms（既定0ms）にします。従来の5→8ms自動補正は廃止し、保存した5msと8msをそれぞれ保持します。スキンの韓国語表示名は `LN 끝 줄임` となり、形状・判定動作は変わりません。通常のシングルプレイで最後まで聴く設定が有効なら、最後のノート・LN終端処理後の `Esc` は一時停止を開かず後奏を飛ばして結果へ進みます。

**2026-10-15 · 1.9.0.2 リリース候補 — 公開検証手順:** 公開済み1.9.0.1を基に、Optionsを緑色、組み込み5K Aery表の選択時だけ緑の⑤を表示し、登録済み4/6/8/10キーモード表の記号を対応色にし、レベルを赤・スコアを白にし、`lv:` レベル検索、新規debounce既定値0ms、音量の1%刻み調整を追加します。選曲の並び替え・フィルター・難易度表選択をSong Library横のヘッダーへ移し、アルバム画像の高さを広げます。マスター既定値10%と固定基準1.9.0は維持します。[変更・使い方](docs/feedback-1.9.0.2.ja.md)。 公開状況とダウンロードはリリースページを基準に確認してください。[範囲・検証](docs/release-1.9.0.2-gate.md)。

**2026-10-15 · 1.9.0.1:** マスター音量の既定値10%とロビーアイコンの中心揃えを含みます。プレビュー×0.75、プロフィールで指定するF5/F6 -10/+10、Xbox/XInputボタン入力、ファイル選択の重複防止、BMS BPM/STOP順序修正と旧リプレイ互換を提供します。固定基準は1.9.0を維持します。 [変更内容](docs/feedback-20261015.ja.md) · [リリース検証範囲](docs/release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 新しい固定基準:** 確認済みのローカル候補を指定された正式版へ昇格します。BMS/メニュー音量分離、ロビー・フィルター・スキン操作、ランク・SUBARTIST、韓国語モード表示、EX判定、送信状態を含みます。通常/HardはR5、EXはR6。旧R1–R5記録と保存設定を保持します。[固定基準](docs/baseline-1.9.0.ja.md) · [検証範囲](docs/release-1.9.0-gate.md)。以下の日付付き項目は当時の履歴です。

**1.9.0 リリース:** BMS/メニュー音量の分離、静かなメニュー音楽、コンパクトなロビー、難易度範囲・並び替え修正、インディケーター倍率、シンプル四角ノートとローカルUIフォント。新規設定はEX・高速索引・ゴーストOFF。保存値を保持し、固定基準を1.9.0へ更新します。[範囲](docs/controls-1.9.0.ko.md)。

**2026-10-14 · 1.8.9.7:** 入力スレッドにMMCSS Games/Highと実行速度の電力スロットリング無効化を適用し、ホームのメニュー枠にアイコン色の控えめなglowを加えます。TenRiff独自のメニュー・結果音楽だけを従来の40%に下げ、BMS楽曲・キー音・試聴は元の音量を維持します。固定基準は1.8.7のままで、物理入力遅延の改善は未確認です。 [Scope / 검증](docs/release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 入力とゲーム進行/HUDの短い待機に再利用する高分解能タイマーを使い、RawInputはメッセージで起床します。比較版Bの方式を適用し、判定幅・R1–R5・リプレイ・固定基準1.8.7を維持します。実プレイの感触は回ごとに異なり、すべての引っ掛かりの解消を保証しません。 [Scope / 검증](docs/release-1.8.9.6-gate.md).

**2026-10-13 · 1.8.9.5:** R5でLN終端・自動判定を時刻順に処理し、音声の先行書き込みと判定を分離。入力・表示時刻とBPM/STOP境界の表示位置を合わせます。ロビーは暗いカードと色付きアイコン、音声設定はマスター音量を先頭に戻します。R1–R4と固定基準1.8.7を保持。[変更と検証](docs/release-1.8.9.5-gate.md)。

**2026-10-13 · 1.8.9.4:** リザルトの大きなランクとスコアの配置・詳細精度を整理し、Gauge Shift CLEARの4色を区別します。BGM・キー音の音量をAudioの先頭に移し、新規設定では両方100%にします。韓国語スキン名称、明記されたobjクレジットとBPM!変速表示、同じ譜面時刻の同一音源の重複再生補正を含みます。譜面作者の送信は任意で、既存の記録IDを維持します。[変更・互換性・検証範囲（韓国語）](docs/release-1.8.9.4-gate.md)。検証とゲーム・ウェブ公開の状態は別途記録し、固定基準は1.8.7です。

**2026-10-13 · 1.8.9.3:** リザルトの文字サイズ、ロビーの色と控えめなBloom、スキンのレーン色に従うキー設定、曲一覧のレベル色を調整しました。インジケーター背景と対象間隔・ビジュアルプリセット項目を削除し、対象レーンをノート色の直上に移動しました。[変更と検証範囲（韓国語）](docs/release-1.8.9.3-gate.md)。固定基準1.8.7と1.8.9.2のBMS補正・リプレイ互換性を維持します。

**2026-10-13 · 1.8.9.2:** BMSの同じレーン・時刻に重複した通常ノートとロングノートの始点を、Native・変換の前に1本のロングノートへ補正します。原本、LNの長さとキー音を保持し、旧リプレイは当時の配置を維持します。[動作・互換性の制限（韓国語）](docs/release-1.8.9.2-gate.md)。固定基準は1.8.7です。

**2026-10-13 · 1.8.9.1:** 連続キー設定、全変換器のキー音・ロングノート保持、FAST/SLOW・タイミング背景切替、ゲージ拡大、プレビューのギア移動と判定文字の枠・背景除去を追加しました。既存リプレイは元の変換を維持します。[動作・検証・互換性の制限（韓国語）](docs/release-1.8.9.1-gate.md)。固定基準は1.8.7です。

**固定基準: [1.9.0](docs/baseline-1.9.0.ja.md)**（2026-10-14の明示的な依頼）。Git tag `1.9.0`を使い、`main`や`latest`に合わせて自動更新しません。旧1.8.7基準と既存リリースは履歴として保持します。

**2026-10-12 · 1.8.9:** ロビーの大きなBEST数値・キーモード色・白い共通外枠、右上情報・左下点数・右下横向きriff mapのStudio HUD、大きな結果文字・分析タブ・中央に揃えた操作を追加します。S+の色・書体を維持し、Judge Easy/Hardの選択を新しいセッションへ渡します。[動作・検証・残る範囲](docs/release-1.8.9-gate.md)、[Judgeの原因と互換性](docs/judge-session-handoff-1.8.9.ko.md)（韓国語）を参照してください。

**2026-10-12 · 1.8.8:** 結果分析、自由なウィンドウサイズとF9 PNG案内、長いLN Mixに加え、表示の読みやすさ、難易度表URL・フィルター、スキンのレーンショートカット、キー数削減時のキー音保全、ソロ開始・即時再試行を改善します。現在の検証・パッケージ・公開状況は[1.8.8リリース案内](docs/release-1.8.8-gate.md)を参照してください。固定基準は1.8.7を維持します。

**分析と拡張計画:** レーン別分布・時間帯別精度・単ノート/LN開始・離鍵・維持失敗を区別し、測定不能のミスを0ms標本にしません。長いLN Mix v2はローカル記録・リプレイ専用で、従来BEST・Sites送信から除外します。プラグインは今後の計画で、1.9.0には含みません。[分析基準](docs/result-analysis-1.8.8.ko.md)、[画面・メニュー・キー音](docs/release-feedback-1.8.8.ko.md)、[開始・再試行操作](docs/gameplay-start-controls.ko.md)（韓国語）。

**2026-10-09 · TenRiff 1.8.7:** 入力補正と読み込み時の音声処理を改善し、LR2ソロのスタジオデッキ、750msのFAST/SLOW表示、キー再割り当て、設定統合と曲名の自動サイズ調整を追加します。 [1.8.7 release](docs/release-1.8.7-gate.md).

**2026-10-08 · TenRiff 1.8.6:** Nativeソロプレイ用のスタジオデッキHUDを追加。左右のShiftをプレイキーに設定でき、F5/F6でHi-Speedを半分/2倍、F11でチャットを開きます。クラシックHUDも選択できます。 [1.8.6 release](docs/release-1.8.6-gate.md).

**2026-10-06 · TenRiff 1.8.5:** VSync OFFのプレイ中に対応ノート画像を描画順にまとめ、CPUの描画呼び出しを削減します。非対応スキン・ロングノートとVSync ONは従来の経路を維持し、ロビー・メニューとR1～R4の判定・リプレイルールを保持します。 [1.8.5 release](docs/release-1.8.5-gate.md).

**2026-10-05 · TenRiff 1.8.42:** 高密度フレームが描画予算をわずかに超えた際の余分な待機を修正し、性能オーバーレイOFF時の統計収集・ソートを省略します。ノートの画質・表示数とR1～R4の判定・リプレイルールは維持します。 [1.8.42 release](docs/release-1.8.42-gate.md).

**2026-10-05 · TenRiff 1.8.41:** Judge HardをEASY基準PG/GR/GD ±17.5/55.714286/98.571429msに調整します。BADは全BMS RANKでHard ±225ms・Easy ±210msに固定。新しい記録はR4を使い、R1/R2/R3リプレイと既存のウェブ記録を維持します。 [1.8.41 release](docs/release-1.8.41-gate.md).

**2026-10-05 · TenRiff 1.8.4:** ポーズのマウス操作と設定クリックを改善し、リアルタイムPacemaker、練習用Auto Scratch、設定の一括移行を追加。ゴースト対戦のプレイヤー幅、LN不透明度、HUD走査とフレームキュー制限を修正します。 [1.8.4 release](docs/release-1.8.4-gate.md).

**2026-10-04 · TenRiff 1.8.3:** 全画面スキンプレビュー、ノートの霧、FAST/SLOWの個別配置、シンプルな四角スキンを追加します。Normalize OFF時の隠れた圧縮と一時停止からの復帰表示を修正し、曲の終了方法、LN解放時の即時判定、BMS RANKに応じたR3判定を導入します。 [1.8.3 release](docs/release-1.8.3-gate.md).

**2026-10-03 · TenRiff 1.8.2:** オプションのアイコンを10色にし、選択枠の色と丸み、FAST/SLOWバーの表示、スキンエディターのプレビューを更新しました。 [1.8.2 release](docs/release-1.8.2-gate.md).

**2026-10-03 · TenRiff 1.8.1:** 機能別アイコン18種とPNGスキンを追加し、オプションと選曲ロビーを黒と無彩色に統一しました。項目別ヘルプ、記録画面のGPT Sitesランキング、左右で4K～16Kを選べるキーテストを追加しました。 [1.8.1 design notes](docs/menu-studio-1.8.1.ko.md).

**マルチプレイRate対応ビルド:** ロビーでリーダーが共通のRateを設定します。全員がこのprotocol v6ビルドを使用してください。旧v5クライアントとは接続できません。[案内](docs/multiplayer-rate-build.ko.md)（韓国語）。

公開状況・ダウンロード: [**1.9.2 リリースページ**](https://github.com/10kseason/TenRiff/releases/tag/1.9.2)。[検証手順](docs/release-1.9.2-gate.md)を参照してください。ゲストと由来情報のない過去記録はローカル専用で、オンラインで新しく開始したプレイだけが既存条件に従って送信されます。サーバー・DB変更は含みません。

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | 日本語

TenRiff は Windows GUI ベースの BMS リズムゲーム runtime/launcher です。固定基準は `1.9.0`、このソースのバージョンは `1.9.2` で、譜面入力は BMS family（`.bms/.bme/.bml/.pms`）専用です。公開 package は BGA upscaler model を含まず、key-mode conversion 用の deterministic NK3 P64 graph と generalized pattern MLP を同梱します。license は MIT です。

この README は導入文書です。現在の挙動、`1.9.2` リリースと固定 `1.9.0` の基準、設定と設計文書は [`docs/README.ja.md`](docs/README.ja.md) から参照してください。

TenRiff のコードベースは、伝統的な長文設計書主導だけで積み上がったものではなく、高速な反復と実験を重視した `vibe coding` 的な性格を持つ作品でもあります。

## 1.8.1 Studio メニュースキン

機能別アイコン18種とPNGスキンを追加し、オプションと選曲ロビーを黒と無彩色に統一しました。項目別ヘルプ、記録画面のGPT Sitesランキング、左右で4K～16Kを選べるキーテストを追加しました。

[Design / skin controls](docs/menu-studio-1.8.1.ko.md) · [Release verification](docs/release-1.8.1-gate.md) · [CHANGELOG](CHANGELOG.md)

## スクリーンショット

### ホーム

プレイ、マルチプレイ、設定を一つの画面から開けます。

![TenRiff のホーム画面](docs/images/studio-home.png)

### 選曲

譜面一覧、自己ベスト、難易度表の設定をまとめて確認できます。

![TenRiff の選曲画面と難易度表設定](docs/images/studio-song-select.png)

### プレイ結果

スコア、判定統計、タイミング分布の推定、ゲージ推移を表示します。

![TenRiff のプレイ結果とタイミング・ゲージ分析](docs/images/studio-result.png)

### スキン設定

リアルタイムプレビューを確認しながらレーンやノートを調整できます。

![TenRiff のスキン設定とリアルタイムプレビュー](docs/images/screenshots/skin-settings.png)

## プロジェクト概要

- 主対象プラットフォーム: Windows
- 対応譜面: BMS family（`.bms/.bme/.bml/.pms`）専用
- グラフィックス経路: D3D11 + Direct2D/DirectWrite
- オーディオ経路: WASAPI（既定）、ネイティブ ASIO（Windows x64 ドライバーが必要）
- 入力経路: RawInput または高ポーリング polling + Xbox/XInputのデジタルボタン・トリガー
- direct-IP multiplayer: 固定 host の TCP coordinator 方式で最大8人、multiplayer の選曲は BMS のみ（既定 `27301/TCP`、[利用案内](docs/multiplayer.md)）
- ライセンス: [MIT](LICENSE)
- サードパーティ通知: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- リリース変更履歴: [CHANGELOG.md](CHANGELOG.md)

## Credits / Attribution

TenRiff の現行キーモードコンバータ実装には、`krrcream-Toolkit` の N2NC アイデアとコードを基にした適応/移植が含まれます。

- 元プロジェクト: <https://github.com/krrcream/krrcream-Toolkit>
- 反映範囲: `Tools/N2NC/N2NC.cs` ベースのキーモード変換ロジックを TenRiff の C++ `GameplayChart` 構造へ移植
- 現在の TenRiff 実装位置: `src/gameplay/KeyModeConverter.*`
- ライセンス/出典通知: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)

可能な限り原作者 `krrcream` と元ツールキットへの出典を保持し、TenRiff 側の変更や統合内容は別途明記しています。

### Thanks

OpenAI Codex、ChatGPT、Claude Code、Gemini、そして検証に協力してくださったゲストテスターの皆さんに感謝します。

## 現在できること

現在のコードベースは、「メニューを開いて曲を選び、譜面を読み込み、プレイし、結果とローカル記録を確認する」水準まで到達しています。

- BMS パーサ/正規化/タイムライン処理
  - ヘッダ、辞書、マージャーコマンド
  - `#MEASURE` の分数処理
  - `#4K / #6K / #8K` ヘッダがある場合の compact lane mapping
  - `#LNOBJ`、LN チャンネル（`51`-`55`, `61`-`65`）
  - CP932（Shift-JIS）ベースのレガシー BMS テキスト対応
- BMS オーディオ処理
  - WAV ネイティブデコード
  - `stb_vorbis` による OGG Vorbis デコードを優先し、必要時は Windows Media Foundation にフォールバック
  - MP3 は Windows Media Foundation フォールバック
  - 必要時は `ffmpeg.exe` フォールバック
  - keysound モード `follow / autoplay / ignore`
  - 遅れて到着した real-time input は判定時刻を維持し、可聴 trigger だけを現在書き込み可能な buffer 境界へ固定して短い keysound の全欠落を防止
- Song Select
  - キャッシュ優先ロード
  - `F5` 強制再インデックスと中央表示の stage / percent / ETA progress
  - 検索、キー数フィルタ、難易度フィルタ
  - `LV ASC/DESC`, `TITLE A-Z/Z-A` ソート
  - 外部フォルダ/BMS のドラッグアンドドロップ
  - recent source 保存/再オープン
  - `-` / `+` で次の play の Rate を即時調整
  - Browse で local BMS difficulty-table JSON を選択し、MD5/SHA-256 一致譜面へ table level を適用
- Gameplay / HUD
  - リアルタイム HUD
  - 段階的な譜面ロード進行表示
  - 通常の gameplay ロード中は `Esc` で取消、Session Mix は Esc を無視
  - display offset
  - performance overlay
  - note head/tail ビットマップキャッシュ + static playfield command-list キャッシュ
  - Ghost Battle は新規または key のない設定で既定 `OFF`、既存の明示的 opt-in は維持
- オプション / スキン
  - Hi-Speed、Rate、gauge、audio、input、graphics 設定
  - `Skins` 画面での判定線位置、ノートサイズ、レーンカラー編集
  - `4K–10K / 12K / 14K / 16K / 7+1` レーンカラー編集とライブプレビュー
  - native vector skin、TenRiff `skin.json`、LR2 playskin に対応
  - 配布 package の `skins/` 完成 skin を既定一覧に表示し、同名の profile skin を優先
  - LR2 skin folder の選択/drag-and-drop で active profile にコピーし、note・LN・lane-gap・destination-size data を反映
- 結果 / ローカル記録
  - Result 画面
  - replay / result JSON export
  - 曲ごとのローカル記録蓄積
  - clear 状態優先の best record 判定

## まだ制限がある点

プロジェクトは使用可能ですが、完全に完成した製品ではありません。

- Windows GUI がメイン経路です。
- Linux GUI/audio/input バックエンドはまだ未完成です。
- LR2 playskin import は対応する gameplay 要素を移植する機能であり、LR2 UI 全体の pixel-perfect 再現を保証しません。
- 一部 GUI 経路は主に build/test ベースで検証されており、実機での手動検証がまだ残っています。
- 古い設計文書と現在の実装が食い違う場合があるため、現行挙動を判断するときは [`docs/current-state.ja.md`](docs/current-state.ja.md) を優先してください。

## クイックスタート

### 1. リポジトリ構成

通常は次のディレクトリを見れば十分です。

- `src/`: ランタイム/ゲームコード
- `tests/`: unit と smoke テスト
- `docs/`: 現在状態と設計文書
- `config/`: グローバル既定設定
- `profiles/`: ランタイム profile、keymap、ローカル結果
- `songs/`: 譜面ルート

### 2. Release ビルド

Windows での一般的なビルド例です。

```powershell
cmake -S . -B build-dist -G "Visual Studio 17 2022" -A x64
cmake --build build-dist --config Release --target tenriff
cmake --build build-dist --config Release --target bms_parser_tests
```

Windows Defender などが一時的に `TenRiff.exe` をロックした場合は、ロック解除後に同じ `cmake --build` コマンドを再実行します。

### 3. 公開ソースパッケージも直接ビルド可能

バージョン付き公開ソースバンドル（`TenRiff-1.2.1-source.zip` のようなパッケージ）には、`external/`（ただし `external/llama.cpp/` を除く）、`src/`、`tests/`、`config/`、`docs/`、`Mainmusic/` が含まれているため、展開したフォルダだけで configure/build できます。

- 公開ソースバンドルはローカルのビルドラッパーに依存しません。上記の通常の `cmake --build` 手順を使います。
- `10k-calc/` は公開ソースバンドルから除外されるため、Python reference ベースの optional チェックが `[skip]` でも正常です。
- `external/llama.cpp/` も公開ソースバンドルから除外されるため、ローカル LLM/tooling checkout は別途復元が必要です。
- `profiles/`、`songs/`、`logs/` もバンドルには含まれませんが、`launch_win.bat` が初回起動時に必要なフォルダを自動作成します。

展開したソースパッケージ内での一般的な流れは次のとおりです。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

### 4. テスト実行

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 5. 実行

直接実行:

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

ランチャースクリプト使用:

```powershell
.\launch_win.bat
```

## 設定とランタイムデータ

TenRiff はグローバル設定と profile 設定を分離しています。

- グローバル設定: `config/config.json`
- profile 設定: `profiles/<name>/config.json`
- keymap: `profiles/<name>/keymap.json`
- 曲インデックスキャッシュ: `profiles/<name>/.tenriff/song-index/<source-hash>.json`
- replay export: `profiles/<name>/replays/*.json`
- result export: `profiles/<name>/results/*.json`
- ランタイムログ: `logs/run.log`
- クラッシュログ: `logs/crash-*.log`

設定構造を素早く把握したいなら [`docs/config.ja.md`](docs/config.ja.md) が最短です。

## 読む順番

README は導入だけを担当します。詳細は次の順で読むのが効率的です。

1. [`docs/README.ja.md`](docs/README.ja.md)
   - 文書マップ全体
2. [`docs/current-state.ja.md`](docs/current-state.ja.md)
   - 現在実際に動いていること
3. [`docs/baseline-1.9.0.ja.md`](docs/baseline-1.9.0.ja.md)
   - 後続作業が参照すべき `1.9.0 fixed stable baseline` ベースライン文書
4. [`docs/gameplay-guide.ja.md`](docs/gameplay-guide.ja.md)
   - 実際のプレイ開始方法、基本操作、HUD/判定/結果画面の説明
5. [`docs/config.ja.md`](docs/config.ja.md)
   - config/profile/keymap 構造
6. [`docs/menu.ja.md`](docs/menu.ja.md)
   - menu / state machine / song-select フロー
7. [`docs/core-loop.ja.md`](docs/core-loop.ja.md)
   - プレイループとデータフロー
8. [`docs/roadmap.ja.md`](docs/roadmap.ja.md)
   - 今後の中長期方向

## 文書の解釈ルール

設計文書と現在のコードが食い違って見えることがあります。その場合の優先順位は次のとおりです。

1. 現在のコード
2. [`docs/current-state.ja.md`](docs/current-state.ja.md)
3. [`docs/config.ja.md`](docs/config.ja.md)
4. 古い設計文書

つまり、「現在の挙動」を判断するときは、古い設計ノートより current-state 文書を優先してください。

## 次に読む文書

- 実際のプレイ方法を知りたいなら [`docs/gameplay-guide.ja.md`](docs/gameplay-guide.ja.md)
- 設定構造を知りたいなら [`docs/config.ja.md`](docs/config.ja.md)
- menu の流れを知りたいなら [`docs/menu.ja.md`](docs/menu.ja.md)
- プレイループを知りたいなら [`docs/core-loop.ja.md`](docs/core-loop.ja.md)
- 全体状態を素早く把握したいなら [`docs/current-state.ja.md`](docs/current-state.ja.md)
