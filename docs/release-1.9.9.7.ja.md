# TenRiff 1.9.9.7 — 通常LNの早期離鍵修正とオンライン送信

通常LNの早期離鍵許容をGOODから現在のPG範囲に制限します。NormalはRANK別10/15/19/22ms、Hard19ms、EX10msです。境界ちょうどは成功し、より早い離鍵は即BAD・コンボ切断・ゲージ減少・OD8 Missになります。押し直しで取り消せません。最後まで保持すれば自動PGで、CNの離鍵判定と単ノート/始点の窓は維持します。

新規Normal/HardはR11、EXはR12です。Sitesは送信・表示を対応しR9/R10も別ランキングとして保持します。既存スコア・ハッシュ・ボード条件を書き換えません。R1–R10/customリプレイとゴーストは元の規則を保持し、JSONフィールド追加や移行はありません。BEST世代3は過去の結果ファイルを保存します。同じパケット構造のpeer v8はv7を拒否します。

Guest・custom・補助・LN Mix v2/v3・一時停止などの除外は維持します。Sitesはclient_submittedでサーバー側リプレイ再検証は行いません。別Rankedサーバーの未対応遮断を保持します。固定基準1.9.6と旧リリース/設置/プロフィール/譜面を保存します。実キーの感触・長時間・他PC・実2PC・実アカウント送信は未検証です。

[Verification](release-1.9.9.7-gate.md) · [Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.7).
