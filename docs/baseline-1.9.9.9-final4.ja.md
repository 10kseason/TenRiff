# TenRiff 1.9.9.9-final4 固定ベースライン

[한국어](baseline-1.9.9.9-final4.md) | [English](baseline-1.9.9.9-final4.en.md) | [日本語](baseline-1.9.9.9-final4.ja.md) | [简体中文](baseline-1.9.9.9-final4.zh-CN.md)

2026-11-10、ユーザーの要請により一般ソースのベースラインを 1.9.6 から **1.9.9.9-final4** に上げました。main/latest を自動追従しません。以下の方針はベースライン変更後も維持します。

ベースラインのコミット、タグ、配布ファイルのハッシュは[公開リリース](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9-final4)と作業スペースの `BASELINE.md` に記録します。公開タグと配布ファイル、過去のソース・候補・プロフィール・元の譜面を保持します。

- [変更と互換性](release-1.9.9.9-final4.ja.md) · [リリースゲート](release-1.9.9.9-final4-gate.md) · [以前のベースライン 1.9.6](baseline-1.9.6.ja.md)
- 判定:Normal PG は VEZ/EZ 22ms、NM 19ms、HD 15ms、VHD 10ms、GR/GD/BAD は元のまま。Hard = Normal NM、EX = Normal VHD。追加の倍率や丸めはありません。[Normal / Hard / EX](judgement-baseline-20261018.ja.md)
- リプレイ:R1〜R14、custom 変種、BEST 世代、peer プロトコルの既存動作は変更しません。判定を変える場合は新しいルールセット ID を使います。
- グレード:S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70、生存で 70 未満は E、ゲージ失敗は F。AUTO と -- は維持。
- オンライン:Guest/custom の除外と client_submitted の境界を維持します。取り込んだ LR2 ランプは表示専用で、送信対象ではありません。
- 配布:BGA の重みは含めず、NK3 許可リストを維持し、既定値は OFF/空/Auto。ユーザーの LR2 DB などの個人データは含めません。

## 1.9.6 との互換性

1.9.6 以降に追加されたルールセット(R11〜R14)、LN Mix v3、キー変換バージョン、BMS のタイミング・ゲージ・RANDOM 方針フィールドはすべて記録にバージョンとして残り、検証器が過去の方針を再現します。保存済みのプロフィール、補正値、ショートカットは移行なしで読み込みます。このリリース自体は判定・スコア・リプレイの計算を変更しません。
