# TenRiff 1.9.9.8 — 普通LN GREAT松键热修复

普通LN提前松键现在允许到GREAT边界。超过PG但在GR以内判为GREAT；超出GR后从GOOD区间起立即BAD、断连、扣槽和OD8 MISS，重新按键不能撤销。保持到尾部的自动PG、CN松键及单键/头部判定窗口保持不变。

Normal/Hard使用R13，EX使用R14并支持在线提交。R9～R12仍可提交到独立排行榜，R11/R12回放保留原PG规则。BEST第4代与peer v9区分不同规则的记录和对战，不新增JSON字段或迁移数据库。

Sites: client_submitted; no server replay re-verification. Existing eligibility exclusions remain. Fixed baseline: 1.9.6. Physical feel, long sessions and real two-PC play are unverified.

[Verification](release-1.9.9.8-gate.md) · [Downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.8)
