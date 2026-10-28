# 曲一覧・フォルダー・難易度表の読み込み改善

1.9.9.5のインジケーター・リザルト候補をすべて継承するローカル候補です。選曲後のプレイ準備は対象外です。公開版と固定基準1.9.6は変更しません。

- 難易度表の切り替えでは、読み込み済みの曲ハッシュに新しい表だけを適用します。以前の曲キャッシュ・記録・リプレイ検証の再読み込みを省きます。同じURLの新しい内容は引き続き取得します。単一フォルダーとALL SONGに対応します。
- FastからSafeへの変更、空の一覧、実行中または未反映のスキャン結果がある場合は従来の更新処理を使います。F5とフォルダー更新の検証は維持します。外部で変更した曲・記録は必要に応じて明示的に更新してください。
- ファイル数を数える段階の重複したパス解決・サイズ・更新日時取得を省略します。実際の読み込みのパス処理、権限エラー、キャンセル、メモリー上限とメタデータ検証は維持します。
- キャッシュ解析結果はコピーせず移動し、選曲位置の維持は表示インデックスを参照します。ソート・検索・選択の規則は同じです。

保存形式、判定、スコア、入力、音声、リプレイ・オンライン契約、モデルは変更しません。専用library-loading-localプロファイルを使い、以前の候補・インストールを保持します。

測定は合成BMS2,000曲/200フォルダー、10,000件キャッシュ、ローカルHTTP応答、記録なしで実施します。OSキャッシュは消去しません。実際のライブラリー、コールドディスク、通信遅延、長時間利用や別PCの結果ではありません。候補のevidence/VERIFICATION.ko.mdとbenchmark-comparison.jsonを参照してください。

## Benchmark

Build the explicit `library_loading_benchmark` target, then run `build/Release/library_loading_benchmark.exe <new-output-directory>`. It refuses existing output paths and uses no real profile or network. The synthetic 10,000-entry cache includes copied metadata entries whose extra paths do not exist; only the first 2,000 charts are used to benchmark filesystem scans. This tool is excluded from default builds and end-user packages.

## 反復測定の中央値

| 処理 | 変更前(ms) | 変更後(ms) | 短縮率 |
|---|---:|---:|---:|
| フォルダー走査(2,000曲) | 1774.08 | 1320.67 | 25.6% |
| 保存済み一覧(10,000件) | 233.99 | 230.95 | 1.3% |
| 難易度表適用(10,000件) | 382.94 | 103.05 | 73.1% |
| 基本LVに戻る(10,000件) | 283.67 | 20.68 | 92.7% |

前後3組を順番に測定し、2組目は順序を逆にしました。走査/一覧は各9標本、難易度表/基本LVは各15標本です。保存済み一覧の1.3%差は小さく変動範囲内のため、体感改善とは主張しません。
