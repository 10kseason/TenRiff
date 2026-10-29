> **Historical local candidate; superseded by [1.9.9.7](release-1.9.9.7.ja.md).** Its local-only online restriction no longer describes the released client.

# 通常LNの早期離鍵修正

通常LNの早期離鍵許容をGOOD範囲（最大約115ms）から現在のPG範囲に制限します。NormalはBMS RANK別10/15/19/22ms、Hard19ms、EX10msです。境界ちょうどは成功し、1サンプルでも早いと即BAD・コンボ切断・ゲージ減少になります。最後まで保持すれば自動PGで、押し直しても失敗は取り消せません。

CNの離鍵判定は既存PG/GR/GDを維持します。No LN Releaseで通常LNになった場合とコースの通常LNには新しいPG許容を適用します。失敗LNのOD8換算はMissとし、全コンボとSudden Deathにも反映します。単ノート・始点の判定、音声、入力補正、表示時刻は維持します。

新規記録はR11/R12・custom11/12で、JSONフィールドは増やしません。R1–R10と過去customの再生・検証・ゴーストは元の規則を保持します。BESTは判定世代を区別し履歴ファイルを書き換えません。peer v8は同じパケット構造でv7を拒否します。公開SitesはR9/R10のまま、新しい記録の送信はローカルで除外します。Rankedの未対応遮断も保持し、サーバー・Web公開は行いません。

1.9.9.6からの未公開ローカル修正です。既存リリース・設置・プロフィール・固定基準1.9.6を保持します。物理キーの感触と長時間プレイは未検証です。
