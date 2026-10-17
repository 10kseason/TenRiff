# TenRiff1.9.5.2 — ノート・フィールドサイズ10%

レビューでBMSエディターのLN本体が細い先端より太くなる表示を修正しました。本体は表示先端幅の80%以内に収め、50–140%での従来の座標は維持します。エディターは読みやすさのため先端の最小8pxを維持するので、非常に細いレーンでは厳密な比例縮小にはなりません。実際のゲームフィールドの縮小とは別です。

プレビューツールの実行中サイズ変更とエディターの幅・キー数の引き渡しも修正しました。実際の値とノート領域を記録し、10%・LNの画素検査とエディター入力検査を区別します。

GOMazk氏の貢献(PR #102)を含みます。Note & Field Sizeの範囲を50–140%から10–140%へ拡張しました。既定100%、保存済みの値とキーモード別設定を維持し、メニュー・プレイ・ゴースト・プレビューで同じ下限を使います。判定・スコア・入力・音声の計算と1.9.5.1の350ms FAST/SLOW・ファイル選択動作は維持します。

Contribution: [GOMazk / PR #102](https://github.com/10kseason/TenRiff/pull/102), commit `29bc6341588390b2299d335a248617d2cde6d0ef`. The original contribution remains in Git history.

The fixed baseline remains1.9.0. No replay/schema migration, website/database deployment or bundled BGA restoration weights. Existing releases, installations and profiles are preserved.

Verification scope: [release gate](release-1.9.5.2-gate.md). Publication and downloads: [1.9.5.2](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.2). Physical long play, other PCs and arbitrary imported skins remain unverified.
