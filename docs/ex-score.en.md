# EX Score transition

[한국어](ex-score.ko.md) | [English](ex-score.en.md) | [日本語](ex-score.ja.md) | [简体中文](ex-score.zh-CN.md)

Displayed scores, personal bests and rankings use **EX Score = 2 × P-Great + Great**. Good, Bad and Poor award zero. Counts follow existing judgement events, including long-note heads/tails. Accuracy, grades, gauge and timing rules remain separate and unchanged.

The highest clear lamp and highest EX record are retained independently. Song details and opening the best result use the actual highest-EX play, including its accuracy, grade and judgement counts. Improving EX with a weaker clear never lowers the existing lamp. The Records tab shows the selected individual play.

Old normalized scores and 5/3/1 detail scores are no longer current scoring metrics. Their fields and calculations remain for historical replay verification and file compatibility. Local records derive EX from stored judgement counts; missing information displays `--` and is excluded from EX best comparisons. Original files are not overwritten with estimates.

The web migration adds nullable EX and judgement-count columns. Historical rows without counts are retained outside EX rankings. An authenticated resubmission of the same replay and metadata can fill only its previously unavailable EX value. This is not server replay verification: records remain `client_submitted`. The server validates integer bounds, total counts and the EX formula.

Web submissions use `schema_version=2`, `score_version="ex-v1"`; multiplayer uses `protocol=2` with the same score version. Older clients require an update. Old rooms are isolated and expire under their existing short leases. Historical TCP protocols and replay rules are not reinterpreted.

Clear artwork keeps x=800..1034. Full images are stretched into short rows for a more elongated appearance, with runtime saturation multiplied by 1.2. EX uses the supplied galaxy PNG. Original images, installations and profiles are preserved.
