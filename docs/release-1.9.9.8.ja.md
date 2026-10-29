# TenRiff 1.9.9.8 — 通常LNのGREAT離鍵許容ホットフィックス

通常LNはPGだけでなくGREAT範囲まで早期離鍵を成功とします。PG外からGR境界まではGREAT、GRを超えるGOOD区間から即BAD・コンボ切断・ゲージ減少・OD8 MISSです。押し直しで失敗を取り消せません。保持完了の自動PG、CN離鍵と単ノート/始点の判定窓は維持します。

Normal/HardはR13、EXはR14でオンライン送信に対応します。R9～R12は別ランキングを保持し、R11/R12リプレイは元のPG規則を使います。BEST世代4とpeer v9で異なる規則の記録・対戦を分離します。JSON項目追加やDB移行はありません。

Sites: client_submitted; no server replay re-verification. Existing eligibility exclusions remain. Fixed baseline: 1.9.6. Physical feel, long sessions and real two-PC play are unverified.

[Verification](release-1.9.9.8-gate.md) · [Downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.8)
