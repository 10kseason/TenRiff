# TenRiff Current State

**2026-10-15 · 1.9.0.1:** プレビュー×0.75、プロフィールで指定するF5/F6 -10/+10、Xbox/XInputボタン入力、ファイル選択の重複防止、BMS BPM/STOP順序修正と旧リプレイ互換を提供します。固定基準は1.9.0を維持します。 [変更内容](feedback-20261015.ja.md) · [リリース検証範囲](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 新しい固定基準:** 確認済みのローカル候補を指定された正式版へ昇格します。BMS/メニュー音量分離、ロビー・フィルター・スキン操作、ランク・SUBARTIST、韓国語モード表示、EX判定、送信状態を含みます。通常/HardはR5、EXはR6。旧R1–R5記録と保存設定を保持します。[固定基準](baseline-1.9.0.ja.md) · [検証範囲](release-1.9.0-gate.md)。以下の日付付き項目は当時の履歴です。

1.9.0リリースでは `Options > Key Mode Settings > Judge Window` から通常/Hard/EXを選択します。Easyは新しい選択肢から外し、過去のリプレイでは維持します。選択は実行中のみ有効で、再起動後は通常に戻ります。[EX判定と互換性](judge-ex-1.9.0.ko.md)（韓国語）。
- 新しい通常/Hardは `tenriff-native-score-v2-ruleset-5`、EXは `tenriff-native-score-v2-ruleset-6` を記録します。過去のR1–R5・Easy・LN規則は維持し、カスタム判定は非公式です。
- `Judge EX` はEASY基準PG/GR/GD=`14/44.571429/78.857143ms`（Hardの80%）でRANK比率を維持。BADは全RANKで250ms固定、別の自動ミス期限は340ms、スコア倍率はHardと同じ1.10です。

**2026-10-14 · 1.9.0 リリース:** 以下の新ランク説明は1.9.0に適用します。以前のバージョン項目は当時の履歴で、旧基準1.8.7のタグとコミットは保持します。[表示と互換性の詳細（韓国語）](grade-display.ko.md)。

**2026-10-14 · 1.8.9.7:** 入力スレッドにMMCSS Games/Highと実行速度の電力スロットリング無効化を適用し、ホームのメニュー枠にアイコン色の控えめなglowを加えます。TenRiff独自のメニュー・結果音楽だけを従来の40%に下げ、BMS楽曲・キー音・試聴は元の音量を維持します。固定基準は1.8.7のままで、物理入力遅延の改善は未確認です。 [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 入力とゲーム進行/HUDの短い待機に再利用する高分解能タイマーを使い、RawInputはメッセージで起床します。比較版Bの方式を適用し、判定幅・R1–R5・リプレイ・固定基準1.8.7を維持します。実プレイの感触は回ごとに異なり、すべての引っ掛かりの解消を保証しません。 [Scope / 검증](release-1.8.9.6-gate.md).

**2026-10-13 · 1.8.9.5:** R5でLN終端・自動判定を時刻順に処理し、音声の先行書き込みと判定を分離。入力・表示時刻とBPM/STOP境界の表示位置を合わせます。ロビーは暗いカードと色付きアイコン、音声設定はマスター音量を先頭に戻します。R1–R4と固定基準1.8.7を保持。[変更と検証](release-1.8.9.5-gate.md)。

**固定基準: [1.9.0](baseline-1.9.0.ja.md)**（2026-10-14の明示的な依頼）。Git tag `1.9.0`を使い、`main`や`latest`に合わせて自動更新しません。旧1.8.7基準と既存リリースは履歴として保持します。

**2026-10-13 · 1.8.9.4:** リザルトの大きなランクとスコアの配置・詳細精度を整理し、Gauge Shift CLEARの4色を区別します。BGM・キー音の音量をAudioの先頭に移し、新規設定では両方100%にします。韓国語スキン名称、明記されたobjクレジットとBPM!変速表示、同じ譜面時刻の同一音源の重複再生補正を含みます。譜面作者の送信は任意で、既存の記録IDを維持します。[変更・互換性・検証範囲（韓国語）](release-1.8.9.4-gate.md)。検証とゲーム・ウェブ公開の状態は別途記録し、固定基準は1.8.7です。

**2026-10-13 · 1.8.9.3:** リザルトの文字サイズ、ロビーの色と控えめなBloom、スキンのレーン色に従うキー設定、曲一覧のレベル色を調整しました。インジケーター背景と対象間隔・ビジュアルプリセット項目を削除し、対象レーンをノート色の直上に移動しました。[変更と検証範囲（韓国語）](release-1.8.9.3-gate.md)。固定基準1.8.7と1.8.9.2のBMS補正・リプレイ互換性を維持します。

**2026-10-13 · 1.8.9.2:** BMSの同じレーン・時刻に重複した通常ノートとロングノートの始点を、Native・変換の前に1本のロングノートへ補正します。原本、LNの長さとキー音を保持し、旧リプレイは当時の配置を維持します。[動作・互換性の制限（韓国語）](release-1.8.9.2-gate.md)。固定基準は1.8.7です。

**2026-10-13 · 1.8.9.1:** 連続キー設定、全変換器のキー音・ロングノート保持、FAST/SLOW・タイミング背景切替、ゲージ拡大、プレビューのギア移動と判定文字の枠・背景除去を追加しました。既存リプレイは元の変換を維持します。[動作・検証・互換性の制限（韓国語）](release-1.8.9.1-gate.md)。固定基準は1.8.7です。

**2026-10-12 · 1.8.9:** ロビーの大きなBEST数値・キーモード色・白い共通外枠、右上情報・左下点数・右下横向きriff mapのStudio HUD、大きな結果文字・分析タブ・中央に揃えた操作を追加します。S+の色・書体を維持し、Judge Easy/Hardの選択を新しいセッションへ渡します。[動作・検証・残る範囲](release-1.8.9-gate.md)、[Judgeの原因と互換性](judge-session-handoff-1.8.9.ko.md)（韓国語）を参照してください。

**1.8.9の性能範囲:** 最終6回比較のp50差は従来の+0.03ms目標内ですが、ghost p95が+0.05125ms（約+1.94%）増加し、厳格なtail非増加基準は満たしませんでした。合成CPU描画呼び出しの測定であり、物理遅延・GPU処理量や一般的な性能向上は示しません。[数値・初期回帰・診断と制限](release-1.8.9-gate.md#성능-측정과-잔여-한계)（韓国語）を参照してください。

**2026-10-12 · 1.8.8:** 結果分析、自由なウィンドウサイズとF9 PNG案内、長いLN Mixに加え、表示の読みやすさ、難易度表URL・フィルター、スキンのレーンショートカット、キー数削減時のキー音保全、ソロ開始・即時再試行を改善します。現在の検証・パッケージ・公開状況は[1.8.8リリース案内](release-1.8.8-gate.md)を参照してください。固定基準は1.8.7を維持します。

**分析と拡張計画:** レーン別分布・時間帯別精度・単ノート/LN開始・離鍵・維持失敗を区別し、測定不能のミスを0ms標本にしません。長いLN Mix v2はローカル記録・リプレイ専用で、従来BEST・Sites送信から除外します。プラグインは今後の計画で、1.9.0には含みません。[分析基準](result-analysis-1.8.8.ko.md)、[画面・メニュー・キー音](release-feedback-1.8.8.ko.md)、[開始・再試行操作](gameplay-start-controls.ko.md)（韓国語）。

**2026-10-09 · TenRiff 1.8.7:** 入力補正と読み込み時の音声処理を改善し、LR2ソロのスタジオデッキ、750msのFAST/SLOW表示、キー再割り当て、設定統合と曲名の自動サイズ調整を追加します。 [1.8.7 release](release-1.8.7-gate.md).

**2026-10-08 · TenRiff 1.8.6:** Nativeソロプレイ用のスタジオデッキHUDを追加。左右のShiftをプレイキーに設定でき、F5/F6でHi-Speedを半分/2倍、F11でチャットを開きます。クラシックHUDも選択できます。 [1.8.6 release](release-1.8.6-gate.md).

**2026-10-06 · TenRiff 1.8.5:** VSync OFFのプレイ中に対応ノート画像を描画順にまとめ、CPUの描画呼び出しを削減します。非対応スキン・ロングノートとVSync ONは従来の経路を維持し、ロビー・メニューとR1～R4の判定・リプレイルールを保持します。 [1.8.5 release](release-1.8.5-gate.md).

**2026-10-05 · TenRiff 1.8.42:** 高密度フレームが描画予算をわずかに超えた際の余分な待機を修正し、性能オーバーレイOFF時の統計収集・ソートを省略します。ノートの画質・表示数とR1～R4の判定・リプレイルールは維持します。 [1.8.42 release](release-1.8.42-gate.md).

**2026-10-05 · TenRiff 1.8.41:** Judge HardをEASY基準PG/GR/GD ±17.5/55.714286/98.571429msに調整します。BADは全BMS RANKでHard ±225ms・Easy ±210msに固定。新しい記録はR4を使い、R1/R2/R3リプレイと既存のウェブ記録を維持します。 [1.8.41 release](release-1.8.41-gate.md).

**2026-10-05 · TenRiff 1.8.4:** ポーズのマウス操作と設定クリックを改善し、リアルタイムPacemaker、練習用Auto Scratch、設定の一括移行を追加。ゴースト対戦のプレイヤー幅、LN不透明度、HUD走査とフレームキュー制限を修正します。 [1.8.4 release](release-1.8.4-gate.md).

**2026-10-04 · TenRiff 1.8.3:** 全画面スキンプレビュー、ノートの霧、FAST/SLOWの個別配置、シンプルな四角スキンを追加します。Normalize OFF時の隠れた圧縮と一時停止からの復帰表示を修正し、曲の終了方法、LN解放時の即時判定、BMS RANKに応じたR3判定を導入します。 [1.8.3 release](release-1.8.3-gate.md).

**2026-10-03 · TenRiff 1.8.2:** オプションのアイコンを10色にし、選択枠の色と丸み、FAST/SLOWバーの表示、スキンエディターのプレビューを更新しました。 [1.8.2 release](release-1.8.2-gate.md).

- 1.8.1: 機能別アイコン18種とPNGスキンを追加し、オプションと選曲ロビーを黒と無彩色に統一しました。項目別ヘルプ、記録画面のGPT Sitesランキング、左右で4K～16Kを選べるキーテストを追加しました。 [Design / validation](menu-studio-1.8.1.ko.md).
- 1.8.0: 項目別のパステルカラー、多様な解像度、プレイと共通のスキンプレビューとゴースト、補助キー、マウス設定、タイトル音楽の選択、ruleset-2 判定を追加します。 [Verification](release-1.8.0-gate.md).
- 1.7.10 は ALL SONG の削除済みキャッシュ項目、統合進捗、完了状態の通知を改善します。[検証と未再現の症状](release-1.7.10-gate.md)。
- 1.7.8 は新しい標準メニュー、4〜16キー対応 Luma Keys、日本語 UI とウェブ/オフラインスキン編集を追加します。メニューとゲームプレイの設定は `native` と `gameplay.native` に分けます。[検証範囲](release-1.7.8-gate.md)。

固定基準は **1.9.0**、この作業本は **1.9.0.1リリースライン** です。通常/HardはR5、EXはR6を使い、過去の記録を維持します。[変更範囲と検証制限](release-1.9.0.1-gate.md)（韓国語）。日付付きの項目は過去の公開版の履歴です。

[ローカル 1.7.1 r2 レポート](local-1.7.1-r2.ko.md)の 749/739 件は以前のビルドの記録です。新リリースの結果として再利用しません。[曲管理](library-management.md)、[基準 BPM](reference-bpm.md)、[スキンプリセット](skin-presets.md)、[ASIO](asio-audio.md)を参照してください。

この文書は、次のエージェントや新しい作業者が最初に読むべき current-state 文書です。目的は、「このプロジェクトは今どういう状態で、どこを見ればよく、何がまだ未検証か」を素早く把握できるようにすることです。

## Baseline
- ユーザー固定基準は `1.9.0`、現在のソースは `1.9.0.1`
- XInputのデジタルボタン・トリガーはKeymap/プロフィールに割り当てます。専用ワーカーがシステム照会を行い、既存InputThreadが内部キューからRawInput/Polling共通の入力キューへ渡します。ワーカーの寿命は入力開始・終了に従います。4台の統合と切断・フォーカス動作は[1.9.0.1案内](feedback-20261015.ja.md)を参照。実機パッドと模擬入力の検証は区別します。
- 1.7.1 は最大8人の HUD・結果、同点順位・スコア待機状態、P-GREAT 専用演出、判定・コンボの独立位置、10個の設定カード、音量ノーマライズ、選曲の難易度表カードを提供します。[変更詳細](gameplay-polish-followup.md)と[検証範囲](release-1.7.1-gate.md)を参照。
- 1.7.0 refreshes native Home, Song Select, Result and shared settings. See [UI implementation](menu-visual-polish.md) and [release verification](release-1.7.0-gate.md). The prism remains only in the custom-skin path; result reveal timing is unchanged.
- 基本結果画面はスコア・ランク・精度を中心に表示し、プリズム演出はカスタムスキン側に残します。既存の2.2秒の表示演出、Spaceによるスキップ、操作解禁条件を維持します。
- UI-r2 Song Select は top tab、7-row jacket library、大きな selected artwork、best-record card、chart/mode panel、実動する START action を使用し、Collection/Store/currency/global ranking の仮 UI は表示しない
- Song Select の Visual Latency、Hi-Speed、Gauge、Random cell は左 click で増加/次、右 click で減少/前を適用して即時保存する。current chart の key count は欠けず、best record は score・accuracy・max combo を同時に表示する。
- Replay evidence v3 は chart SHA-256、canonical ruleset、result-to-replay SHA-256 を結合し、input trace を headless engine で再実行する。official local best には再計算済みの verified result だけを使用し、legacy・custom-ruleset・assist record は unverified history として保持する。
- 1.3.1 は外部 `.osu` parser・index 経路・settings toggle を削除して chart surface を再び BMS 専用に固定。BMSTable 選択時は hash matching のため Fast から Safe へ自動切替し、Aery live server と CG901B MD5→`⑤LEVEL 13` match で確認
- Profile Setup は任意の local PNG/JPG avatar path を profile ごとに保存し、Song Select の profile card から編集できる
- direct-IP multiplayer と preview r5 の input-backend lifecycle 修正は `1.1.8 stable` に統合
- `1.1.8` は 1.1.7 の visual refresh に osu!mania OD8 補助スコア、最初の native `BAD` で終了する `Sudden Death (1 MISS)`、決定的な `LN Mix 10%～90%` を追加
- `1.2.0` は BMS channel `04/07` と osu!mania 背景を gameplay sample timeline に接続し、FHD 未満の画像背景を Windows ML 上の LunaSR で非同期補間
- `1.2.1` は LunaSR `basic_v2` へ切り替え、gameplay BGA と Song Select の選択 BGI を補間し、single-player の Esc pause menu、polygon note shape、LN tail cap toggle を追加します。
- `1.2.2` は procedural 円・多角形 skin を bar と同じ 100% 幅に補正し、FFmpeg fallback 付き MPG/MPEG 動画 BGA を追加し、LunaSR を必須 200 FPS benchmark gate 付き staged32 RGB FP16 model に切り替えます。
- `1.2.3` は当時の LunaSR 固定 RGB x2 performance gate を緩和。
- `1.2.4` は権利範囲が不明確な LunaSR ONNX と model 固有 metadata を公開配布から除外し、既定値 `off` の user-supplied model opt-in integration だけを残します。
- `1.2.5` は model 固有名の integration を generic External ONNX Upscaler に置換し、Graphics Settings の file 選択/.onnx drop、profile 別 model path、model 別 WinML session を追加。
- `1.2.6` は playable chart を BMS family のみにし、native/LR2 skin だけを維持。ONNX upscaler の手動有効化と実験的 NPU 優先、Song Select の Rate 調整、中央 indexing progress、local JSON 難易度表、BMS keysound late-input hotfix を追加。
- `1.2.7` は External ONNX Upscaler の FP16 binding、float boundary INT8 QDQ 検出、high-performance DirectX GPU default、動画 BGA one-in-flight backpressure を修正。
- `1.2.8` は LN 全体の一瞬の点滅修正、gameplay BGA toggle、BMSTable HTML/header link import、長い settings 画面の scrollbar UX 改善をまとめた配布前候補。
- `1.2.9` は 12K/14K と scratch-aware key conversion、R-Random/DP Flip/Note Add、Song Select preview、画像付き詳細 Result、profile nickname、video BGA 揺れ防止、正確な `DirectXMinPower` 表記を追加。
- `1.2.92` は standalone BMS key converter に既定の Krrcream と決定論的 `nK2 Native 50/50` の選択を追加。
- `1.2.93` はゲーム内 Mode Settings に `Key Converter` を追加し、`Krrcream`/`KeyWeaver nK2` の選択を設定・replay metadata に保存して runtime key-mode 変換へ適用。
- `1.2.95` では `OSU Charts` で osu!mania 4K～10K `.osu` の index/play を一時的に復元しましたが、この経路は 1.3.1 で再び削除されました。
- 後続作業の基準文書は [`docs/baseline-1.9.0.ja.md`](baseline-1.9.0.ja.md)
- Windows GUI ビルドが主対象
- Linux は `Baepoks-Linuxs/TenRiff-0.5.0-linux-preview` レベルの preview のみ
- 対応 chart surface は BMS family（`.bms/.bme/.bml/.pms`）専用
- `1.2.4 stable` の gameplay 入力は RawInput を優先しつつ、同じ `InputThread` で bound-key polling shadow を常時動作させる。起動失敗または message pump の予期しない終了時も queue / pressed state を reset せず、その producer を Polling に切り替える
- menu と gameplay の入力収集は foreground process 境界を維持し、MenuWindow UI は root-window 境界も確認する。focus を失うと key state を初期化し、background の lane/Esc 入力を破棄する。RawInput の起動失敗、process-global 登録先の消失、hidden message window の終了を検知すると、ユーザー入力を待たず Polling に切り替える。
- 確認済み fallback は profile を書き換えず、そのアプリ実行中の menu と後続 gameplay に維持する。アプリ再起動または `Options -> Input Settings -> Backend` の明示変更で再試行する。

## Core Architecture
- `MenuApp`
  - menu state machine の中心
  - Song Select、Options、Keymap、Result、Gameplay 起動への遷移を管理
  - `MenuNavigator` が current screen と nested Back history を単独で所有
  - Options と全 settings family の typed controller が selection/dirty/mutation state を所有し、`MenuApp` adapter が保存、file dialog、restart、reindex effect を実行
  - `MenuScreenDescriptor` が固定 title、skin background/fallback、snapshot/generic-view routing を exhaustive table で定義
  - 最近の保守リファクタで Song Select の record/keymap/render/state 境界が専用 `.cpp` に分離された
  - Song Select の最高 score card と record list は `MenuAppRecords` の共通 saved-Result open path を使い、Result の Replay button へつながる
  - open-source source package でもローカル `10k-calc` なしでコアテストが動くよう、optional な Python-reference チェックは skip 可能
- `SongIndexerThread`
  - 譜面インデックス専用のバックグラウンドスレッド
  - Song Select に進行状況を送る
- `AudioThread`
  - オーディオのマスタークロックとミキシングを担当
- `InputThread`
  - RawInput / polling 入力を収集し、キューへ渡す
  - gameplay は RawInput と bound-key polling shadow を単一の `InputThread` state tracker で dedupeし、`GameSession` では logical edge を source 単位で再フィルタリングしない
- `RenderThread` + `MenuWindow`
  - D3D11 + Direct2D/DirectWrite ベースの menu / gameplay HUD レンダリング
  - immutable `MenuRenderData` snapshot のみを消費し、settings controller や mutable app state を参照しない
- `GameSession`
  - 譜面ロード、gameplay audio prep、HUD snapshot、gameplay 実行境界を担当

## What Works Now
- BMS parser / normalizer / timeline パイプラインは実パック互換性重視で強化済み
- BMS explicit key headers:
  - `#4K`
  - `#6K`
  - `#8K`
  - `5+1 SP`
  - `7+1 SP`
  - header があるか SP パターンが検出されると、そのキー数に合わせて compact lane mapping を適用
- BMS keysound:
  - `follow`
  - `autoplay`
  - `ignore`
  - 遅れて届いた real-time input も元の sample timestamp で判定し、可聴 keysound の開始だけを現在の writable-buffer 境界に固定
- BMS long notes:
  - LN channels (`51`-`55`, `61`-`65`)
  - `#LNOBJ`
  - `#LNMODE 2` charge notes は tail release timing 判定を使用
  - 通常 BMS LN は最後まで保持すると自動クリアされる
- BMS audio decode:
  - WAV native first
  - OGG は内蔵 `stb_vorbis` を優先し、失敗時は Windows Media Foundation
  - MP3 は Windows Media Foundation を使用
  - MF に失敗した場合は `ffmpeg.exe` fallback
- Song Select:
  - cache-first loading
  - `F5` forced reindexing
  - mouse-wheel navigation
  - `Sort / Filter > Key Filter` でキー数を選択
  - external folder / BMS drag-and-drop
  - recent source の保存と再オープン
  - difficulty / title sorting
  - search 入力中でなければ `-`/`+` で現在の play Rate を調整
  - indexing 中は stage、percentage、processed/total、ETA、song count、progress bar を header 下部中央に表示
  - 選曲中央下部の難易度表カードで URL 編集・ローカル JSON・標準 LV 復帰。Filters も同じ取込経路を使い、変更時に MD5/SHA-256 照合と再索引を実行。
- BMS key mode:
  - キーモードごとの separate keymaps
  - 対応 key count の chart difficulty calculation
  - `mode.key_mode` は N2NC スタイルの lane remap でキー数を変換
  - ゲーム内 Mode Settings の `Key Converter` で `Krrcream`、内蔵の決定論的 `KeyWeaver nK2`、または `KeyWeaver NK3 ONNX` を選択し、設定と replay metadata に保存
  - 個別の `Conversion Note Add` 設定は削除。Krrcream は元 note の再配置のみを行い、nK2 は key count 拡張時に変換後の target layout へ安全な support note を直接生成する。
  - nK2 preset は既定の `Native (12%)`、`Transform (35%)`、`Remaster (65%)` から選択する。`Remaster` は budget を上げつつ anchor を固定して原曲の配置を残し、LN 区間を同じ長さの LN で埋める。3 つとも上限であり、実際の追加量は原曲の密度と safety window で決まる。Krrcream では row を lock し、standalone converter GUI の Krrcream Max/Min/Speed/Seed も変更不可。
  - 1.9.0.1 公式 build/Windows ZIP は standalone BMS key-converter CLI/GUI を build・同梱しない。top-level CMake option は既定 `OFF` で、source は開発 regression 用のみ維持
  - NK3 は同梱 P64 と host beam32 を常に組み合わせる。10K 以外の source を 10K に変換するときだけ generalized pattern MLP を追加し、10K→10K とその他すべての target は P64 のみを使う。既定の `AUTO` backend は ncnn Vulkan で P64 と MLP を AMD/NVIDIA GPU 上に実行し、任意の OpenVINO compatibility path を fallback として維持する。`TENRIFF_NK3_BACKEND` と `TENRIFF_NK3_VULKAN_DEVICE` で強制選択できる。
  - `mode.key_mode=none` は元のキー数と基本パターンレイアウトを維持
- Native difficulty:
  - BMS の LV/CR 計算では long-note Head/Tail の miss-ms だけを 0.5倍で評価し、`300ms`を`150ms`として緩和する。実際の gameplay 判定 window は変更しない
- Lane transform:
  - Random は `Off / Mirror / Random (Scratch Fixed) / SR` に対応。通常 Random は scratch を固定し、play ごとに新しい key-lane 配置を生成して実際の seed を replay に記録
  - 新しい `LN Mix 10%～90%` v2はbase BPM×Rate基準で長い2・4拍60% / 中間1拍20% / 短い1/4・1/2拍20%を目標に混合。既存LN・元の開始位置とキー音を維持し、同一レーンの次の開始まで50msを確保。空間不足では長さを段階的に下げ、連打は結合しない。旧リプレイはv1で再現し、v2はローカル記録・リプレイのみで従来BEST・Sites送信から除外。[追加条件と検証（韓国語）](release-1.8.8-gate.md)を参照
- Skins / gameplay feel:
  - 1.8.9 Studio DeckはNative/LR2ソロで右上情報・左下点数・右下64区間横向きriff mapと拡大ゲージを使用。狭い余白では全体縮小・反対側移動を行い、極端なフィールドとPerformance HUDが両側上部を塞ぐ場合は上部情報だけを非表示。ゴースト・マルチ・他の画像スキンはClassic維持。[配置と検証範囲](release-1.8.9-gate.md)
  - 標準 `10+2 DP` chart は scratch 2 lane を除く実際の 10 key に `10K` lane-color palette を順番に適用し、native 12K palette とは独立して保持
  - `rect / triangle / pentagon / hexagon / circle` note shape。procedural 円・多角形は 100% で rect bar と同じ全幅を使用
  - note border on/off
  - 判定 X/Y とコンボ X/Y を個別に調整・保存。FAST/SLOW は該当方向のみ表示し、P-GREAT では省略。
  - P-GREAT のみ黄色・虹色のきらめき・220ms のポップを使います。GREAT は水色、GOOD は灰色。下位判定と FAST/SLOW 文字は静止し、コンボ数字の150ms ポップは独立して維持します。
  - judge line / lane width / lane spacing / note & field size / divider width / 16K center gap / note height / LN body width adjustment
  - `Note & Field Size` は中央を固定したまま playfield、lane/divider、note、隣接 gauge を 50%～140% でまとめて拡大・縮小し、100% では隣接 note 間の合計 gap が既定で 24px
  - Black Playfield は lane spacing を含む player/ghost playfield 全体を完全な黒で表示
  - キーモードごとの lane-width 配列と inter-lane spacing 配列が保存され、preview / live gameplay / ghost field の同じレイアウト計算に適用される
  - 対応 skin route は `native`、TenRiff `skin.json` v1、LR2 playskin。Skins で TenRiff/LR2 folder を選択または drop すると既存 install を上書きせず active profile へ移植
  - executable 横の `skins/` にある完成 TenRiff skin を既定一覧へ統合し、同名なら active profile の `skins/tenriff/` copy を優先
  - 標準 `LR2files` または `Theme` root を選ぶと、`IIDX` folder と IIDX asset に依存する theme を除いた直下の theme を別 skin として一括 install し、sibling-theme path を維持する。symlink は skip し、既存 folder は上書きしない
  - LR2 note/LN image、lane gap、destination size を反映し、`play/Gear` 下部 frame は field size に連動して拡大し、aspect ratio を維持したまま判定線の下に clip する単一 overlay として表示する。`#CUSTOMFILE` wildcard include の default 選択を解決し、Gear がない場合は falling note / LN head を receptor に再利用しない
  - native skin は各 lane 下部に digital piano key を表示し、入力を hold している間だけ key が沈む。打鍵時は短い cyan/magenta glitch pulse を表示し、process が foreground を失うと hold visual も reset
  - `skin.lr2_resolution_mode` は `auto / sd / hd / fhd` を保持
  - LR2 auto-detect は asset 名ではなく playskin `#DST_NOTE` の座標範囲を使う
  - フィールド上端からの future-note entry easing
  - 最後の判定ノート後はAudioの曲終了方式に従い、最後まで聴く（既定）か通常の結果待機時間後に後奏を自動スキップする。最後まで聴く場合もlane keyによる手動スキップを維持
- Judge:
  - 1.9.0リリースでは `Options > Key Mode Settings > Judge Window` から通常/Hard/EXを選択します。Easyは新しい選択肢から外し、過去のリプレイでは維持します。選択は実行中のみ有効で、再起動後は通常に戻ります。[EX判定と互換性](judge-ex-1.9.0.ko.md)（韓国語）。
  - 既定の `PG / GR / GD` 判定幅は `21ms / 65ms / 115ms` (BMS RANK EASY)
  - BMS `#RANK` EASY（未指定を含む）/NORMAL/HARD/VERYHARDのPGは`21/18/15/8ms`。NORMAL/HARD/VERYHARDはEASYのPG/GR/GD/BAD全体に`18/21`、`15/21`、`8/21`を掛け、自動ミス時刻は維持する
  - [最新の判定幅・RANK表・LN規則（韓国語）](judgement-windows.md)
  - `Judge Easy` はRANK適用後のPG/GR/GDとホールド許容幅を `1.35x` に拡大。EASYのPG/GR/GD=`28.35/87.75/155.25ms`。BADは全RANKで `210ms` 固定、maskは維持
  - `Judge Hard` はEASYのPG/GR/GD=`17.5/55.714286/98.571429ms`。RANK適用後にPGへ `17.5/21`、GR/GDへ `18/21` を掛ける。BADは全RANKで `225ms` 固定、ホールド許容幅は維持
  - 標準のNormal/Easy/Hardは未入力ノートが `340ms` を超えると自動ミス。BAD範囲外の遅い入力は前のノートをミスにして次を調べるため、判定幅が自動ミス期限まで広がることはない
  - 新しい通常/Hardは `tenriff-native-score-v2-ruleset-5`、EXは `tenriff-native-score-v2-ruleset-6` を記録します。過去のR1–R5・Easy・LN規則は維持し、カスタム判定は非公式です。
  - 同一 lane の pending note がすでに `BAD` で、直後の note が明確に `GOOD` 以上なら、pending note を miss として記録し、現在の press を次の note に割り当てて連続 `BAD` lock を防ぐ
  - `Judge Hard` では未入力の object を combo-breaking indirect `POOR` かつ OD8 `MISS` として記録し、その他の note-consuming failure は `BAD` のまま
  - かなり早い non-consuming press は LR2 スタイル `POOR` として扱われ、result / replay / UI に再表示される
  - 空打ち `POOR` は combo を維持し、Hard indirect `POOR` は combo を切る。両方とも score / accuracy には入らず、専用 `PR` gauge damage を使う
  - ゲージ選択は常時有効な Gauge Shift の開始段階 `EX / Hard / Normal / Easy`。旧 `shift` は EX 開始として解釈。
  - 選択段階と全下位段階をそれぞれ100%から計算。脱落時は次の生存段階へ移り、終了時の最上位生存段階が最終結果。
  - `Sudden Death (1 MISS)` は最初の OD8 換算 object `MISS` で即失敗する。native `BAD` timing だけでは発動せず、空打ちの `POOR` も無視し、Practice No-Fail とは排他的に動作する
  - OD8 換算は Sudden Death と既存 replay 互換用の内部統計として維持し、Gameplay / Result UI は TenRiff native score のみ表示
  - casual score は最大 10,000 点で、judgement weight は `PG 6 / GR 3 / GD 1 / PR 0 / FAIL 0`。LN head / tail は各 0.5 weight で 1 object を構成し、detail score は別体系を維持する
  - 通常の Accuracy は `PG / GR / GD / BD / PR = 100 / 80 / 50 / 20 / 0%` の判定加重平均で、判定区間内のタイミングを反映する詳細精度とは別。通常の Accuracy にタイミングのばらつきによる追加減点や99.5%の上限は適用しない
  - 1.9.0 リリースの生存完走ランクは、丸め前の通常精度で `99%以上 S++ / 98%以上 S+ / 95%以上 S / 93%以上 A+ / 90%以上 A / 85%以上 B / 80%以上 C / 80%未満 D`。75%未満の生存完走もD、失敗はF
  - 明示的な中断（`ABORTED`/`aborted`）と記録なしは `--`、自動演奏は `AUTO`。死亡と中断を区別する情報がない旧失敗記録はFを維持し、元の記録・得点計算・数字の順位は変更しない
  - live gameplay の `ClockSync` は大きな Windows QPC 絶対値ではなく centered anchor regression を使い、継続する clock discontinuity 後に自動 rebase する
  - stale backlog は QPC event age と `BAD` window で判定し、fresh input の sample mapping が現在の playback anchor から大きくずれた場合は anchor に fallback する
  - R3では通常LNと`#LNMODE 2` CNを早く離しすぎると即座にBAD・コンボ切れ・ゲージ減少が確定し、再入力で取り消せない。通常LNの保持による自動完了とCNの末尾解放判定は維持
  - 2 台のキーボードが同じキーを押しても、最後の入力ソースが離すまで論理 `Pressed` は維持される
- Graphics:
  - 従来の解像度プリセット、幅/高さの直接入力（各320–8192）、枠のドラッグによる自由なサイズ変更。16:9の内容比率・余白・クリック座標を維持し、ドラッグ終了サイズを保存。[操作と復帰条件](config.ja.md#graphics)を参照
  - ソロ結果の概要・分析タブ・マルチ結果の下部に小さな `F9` PNG / `screenshots/` 案内を表示。現在の作業フォルダー配下へ保存し、今回の実際の保存確認は[追加リリース検証（韓国語）](release-1.8.8-gate.md)で別途扱う
  - `refresh_hz` (`-1` = `Match Display`、`0` = `Unlimited`、実際の上限は 1500 FPS)
  - VSync off: Match Display は monitor Hz に従い、Unlimited は gameplay を 1500 FPS に制限しつつ menu cap `300` を維持
  - VSync on: present refresh は active monitor Hz に追従し、render pacing は `monitor_hz * 2` を目標にする（`1050` clamp）
  - `visual_offset_ms`
  - `performance_overlay` (gameplay FPS/frame time は成功した DXGI `Present()` 完了間隔を使用し、HUD update cadence は別の gameplay diagnostics として維持)
  - `bga_enabled=false` は gameplay image/video BGA と decoder/upscaler 処理を無効化し、Song Select preview は維持
  - `background_upscale_model_path` は Graphics Settings で選択/drop した互換 ONNX の path だけを保存。公開 model は同梱しない
  - BGA Upscaler は既定 `off`。user が明示的に on にして high-spec warning を確認する必要があり、自動 benchmark gate はない
  - 現在の契約は 960x540 RGB residual x2 で FP32/FP16 boundary と float boundary INT8 QDQ metadata を自動検出。model の権利・品質・性能は user が確認し、load・contract・decode・inference 失敗時は native scaling
  - 静止 image BGA は非同期 ONNX upscale を維持し、video BGA は遅延した過去の推論 frame が新しい frame を上書きしないよう native decode frame のみ表示
  - default は high-performance DirectX GPU。`background_upscale_prefer_npu=true` は legacy WinML `DirectXMinPower` request に過ぎず、NPU の明示選択・endpoint 検証は行わない
- Gameplay performance:
  - static playfield command-list cache
  - note head / tail bitmap cache
  - fixed-size HUD note transport
- Loading UX:
  - Song Select header 下部中央に indexing stage/percentage/processed/total/ETA/song count と progress bar を表示
  - gameplay chart-loading progress 表示
  - gameplay loading 中の `Esc` cancel
  - Song Select で同じ曲を750ms選択すると明示 preview または local audio fallback を再生
  - Result に song image と chart/key/BPM/LV/CR/difficulty-table 情報を表示
- Profile UX:
  - `Options -> Profile Setup` から現在の profile の初回 setup 画面を開き直し、language / audio / input / graphics / keymap を即時保存できる
  - 最大48 byte の nickname を編集し、保存 record と direct-IP multiplayer 表示名に使用
- Direct-IP multiplayer:
  - protocol v6 は固定 TCP coordinator 1台に最大8人まで接続（Windows、既定 `27301/TCP`）
  - multiplayer の対象は indexed BMS 系 chart のみで、全参加者の SHA-256 共通集合を使用し `.osu` は除外
  - lobby の `ROOM CHAT` は最大256 byte の UTF-8 message を送信し、session memory には最新32件だけを保持して message 数と local/leader 表示を行う
  - リーダーが共通Rateを0.50〜2.00倍（0.05単位）で選択し、変更時は全員のREADYを解除。判定・Gauge Shift・Random/Mods/Assist規則を維持し、各プレイヤーのローカルkey-mode conversionを許可
  - 選曲権は host から始まり、全員が Result を出た後に join 順の次の接続 player へ移動。leader 切断時は次へ進み、host 切断時は room を閉じる
  - 現在の leader だけが共通 BMS を選び、全員 Ready 後に START を要求可能。coordinator 承認と全員 load barrier 後に開始
  - 対戦中または9人目の join は拒否。範囲外の score claim も拒否するが、replay proof は未交換のため Result の相手結果は `UNVERIFIED CLAIM` と表示
  - chart 転送、NAT traversal、relay/matchmaking、暗号化/認証、anti-cheat、自動再接続は未対応

## Song Indexing Model
- song source が変わると、まず `profiles/<name>/.tenriff/song-index/<source-hash>.json` の profile-local cache を読む
- cache が無いか無効ならバックグラウンドインデックスを開始
- Indexing profiles:
  - `safe` が既定
  - `fast` は song list 用の最小 metadata（title/artist/key count/#PLAYLEVEL/BPM）のみ収集
  - Mode Settings の `Indexing` 行と `config.mode.song_index_profile` で制御
  - `fast` は file hash、背景/音声 preview 解決、difficulty-table matching、native LV/CR を省略
- Difficulty calculation:
  - Mode Settings の `Index Difficulty` と `config.mode.calculate_song_index_difficulty` で制御
  - 既定 `off` は BMS `#PLAYLEVEL` を保持し、native LV/CR 計算を省略
  - `on` は `safe` で Revive LV/Circus Rating を計算し、`fast` では常に省略
- Indexing stages:
  - `SCANNING FILES`
  - `BUILDING METADATA`
  - `WRITING CACHE`
- 大規模ライブラリ向けメモリ強化:
  - two-pass enumerate + small batch metadata build
  - `safe` profile は 1-worker 寄りの budget と頻繁な heap trim で RAM high-water を抑える
  - indexing 用 BMS parse は asset map / 不要 header / 非必須 command を省く低メモリ経路を使用
  - cache save は giant JSON tree ではなく streaming write
- 実測:
  - 46k-chart Windows benchmark library の safe full-index で `46,636` candidates / `46,602` indexed entries
  - peak memory はおよそ `working set 453MB`, `private 524MB`

- Cache schema:
  - `version = 15` (running-duration reference BPM; see [reference-bpm.md](reference-bpm.md))
  - optional `layout_label`
  - `minimal_metadata` で Safe/Fast cache を分離

## Runtime / Packaging Rules
- 新しい user profile は自動生成される
- このソースのP2P版は`TenRiff 1.8.9`。[リリース記録](release-1.8.9-gate.md)で互換性検査と実際のパッケージ・公開状況を確認
- distribution package には `Songs` を含めない
- distribution package には `Main Menu / Options / Song Selecte / Multiplayer Lobby / Clear / Failed` の `Mainmusic/` scene slot を含め、各 `Name.mp3` と `Name 2.mp3`～`Name 64.mp3` を自動検出して scene 再入場ごとに循環する
- distribution 更新には built artifact と必要な runtime asset だけを含める
- source-only / public handoff 前に include/exclude リストを確定する
- preview source branch と tag は stable release と分離して管理する
- public source package 更新時は docs/files を揃えるだけで終えず、その staged folder 自体で standalone configure/build/test が通ることも確認する

## Config / Profile Reality
- 実際の既定値は `config/config.json` にある
- runtime profile は `profiles/<name>/config.json`
- keymap は `profiles/<name>/keymap.json`
- `keymap.json` は `modes.{4k..10k,12k,14k,16k}` の per-mode binding 構造
- 古い profile は runtime migration により一部補正される
  - keysound policy
  - 削除済み osu field は保存しない

## High-Value Files
- `src/app/MenuApp.cpp`
- `src/app/GameSession.cpp`
- `src/app/SongIndex.cpp`
- `src/app/SongIndexerThread.cpp`
- `src/render/MenuWindow.cpp`
- `src/render/RenderThread.cpp`
- `src/app/ChartLoader.cpp`
- `src/chart/BmsParser.cpp`
- `src/config/Config.*`
- `src/config/Keymap.*`

## Validated Commands
- `cmake --build build --config Release --target tenriff`
- `cmake --build build --config Release --target bms_parser_tests`
- `cmake --build build --config Release --target bms_realworld_smoke`
- `ctest --test-dir build -C Release --output-on-failure -R bms_parser_tests`
- `cmake -S . -B build-check -G "Visual Studio 17 2022" -A x64`
- `cmake --build build-check --config Release --target bms_parser_tests`
- `.\build-check\Release\bms_parser_tests.exe`

MSVC ASan は決定的な単体コアを実行し、RawInput/localhost OS と NK3/OpenVINO の統合検査を除外します。除外一覧は `tests/unit/test_bms_parser.cpp`、CTest 引数は `CMakeLists.txt` が基準。全統合検査は通常 Release で実行します。

## Still Manual-Validation Heavy
- 実際の CJK-heavy library での Song Select fast-scroll crash 再現
- fast profile の長時間 full-index に対する RAM / commit 再確認
- gameplay low-FPS / 0.1% / 0.01% low の確認
- graphics 設定 live-apply 中の OBS / Discord / Game Bar 共存確認
- drag-and-drop / 外部 Korean-path source の GUI 確認
- 低電力 DirectX（実験）の on/off と GPU フォールバックを確認。旧 NPU 設定名は NPU 選択・実行の証拠ではありません。
- local 難易度表の変更後に hash matching、reindex、表示順を GUI 確認
- Linux はまだ実行可能ビルドではない

## Best Next Read
- runtime / config なら `docs/config.ja.md`
- menu / indexing / state machine なら `docs/menu.ja.md`
- play loop / audio / judgement なら `docs/core-loop.ja.md`
