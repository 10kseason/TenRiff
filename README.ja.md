# TenRiff

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | 日本語

TenRiff は Windows GUI ベースの BMS リズムゲーム runtime/launcher です。現在の stable 版は `1.5.0` で、譜面入力は BMS family（`.bms/.bme/.bml/.pms`）専用です。公開 package は BGA upscaler model を含まず、key-mode conversion 用の deterministic NK3 P64 graph と generalized pattern MLP を同梱します。license は MIT です。

この README は導入文書です。現在の挙動、`1.5.0` project state、`1.1.2 final stable` baseline、設定と設計文書は [`docs/README.ja.md`](docs/README.ja.md) から参照してください。

TenRiff のコードベースは、伝統的な長文設計書主導だけで積み上がったものではなく、高速な反復と実験を重視した `vibe coding` 的な性格を持つ作品でもあります。

## プロジェクト概要

- 主対象プラットフォーム: Windows
- 対応譜面: BMS family（`.bms/.bme/.bml/.pms`）専用
- グラフィックス経路: D3D11 + Direct2D/DirectWrite
- オーディオ経路: WASAPI
- 入力経路: RawInput または高ポーリング polling
- direct-IP multiplayer: 固定 host の TCP coordinator 方式で最大8人、multiplayer の選曲は BMS のみ（既定 `27300/TCP`、[利用案内](docs/multiplayer.md)）
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
  - gameplay ロード中の `Esc` キャンセル
  - display offset
  - performance overlay
  - note head/tail ビットマップキャッシュ + static playfield command-list キャッシュ
  - Ghost Battle は新規または key のない設定で既定 `OFF`、既存の明示的 opt-in は維持
- オプション / スキン
  - Hi-Speed、Rate、gauge、audio、input、graphics 設定
  - `Skins` 画面での判定線位置、ノートサイズ、レーンカラー編集
  - `5K`-`10K` レーンカラー編集とライブプレビュー
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
3. [`docs/baseline-1.1.2.ja.md`](docs/baseline-1.1.2.ja.md)
   - 後続作業が参照すべき `1.1.2 final stable` ベースライン文書
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
