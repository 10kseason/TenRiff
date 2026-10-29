# TenRiff 1.9.9.7 — 普通LN提前松键修复与在线提交

普通LN的提前松键容差从GOOD缩小为当前PG范围。Normal按RANK为10/15/19/22ms，Hard19ms，EX10ms。恰好在边界成功，更早松键即BAD、断连、扣槽和OD8 Miss，重新按键不能撤销。保持到尾部仍自动PG，CN松键和单键/头部判定窗口保持原样。

新Normal/Hard使用R11，EX使用R12。Sites支持提交、查询和标签，R9/R10仍可提交到独立排行榜。不改写旧成绩、哈希或榜单条件。R1–R10/custom回放与幽灵按原规则运行，无新增JSON字段或数据迁移。BEST世代3保留历史文件。数据结构相同的peer v8拒绝v7，避免不同判定混战。

保留Guest、自定义、辅助、LN Mix v2/v3和暂停等既有限制。Sites记录client_submitted成绩，不进行服务器回放复核；独立Ranked服务器支持限制保留。固定基线1.9.6及旧发布/安装/配置/谱面保留。实体手感、长时间、其他PC、实际双机与真实账户上传未验证。

[Verification](release-1.9.9.7-gate.md) · [Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.7).
