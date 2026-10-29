> **Historical local candidate; superseded by [1.9.9.7](release-1.9.9.7.zh-CN.md).** Its local-only online restriction no longer describes the released client.

# 普通LN提前松键修复

普通LN的提前松键容差从GOOD范围（最多约115ms）缩小为当前PG范围。Normal按BMS RANK为10/15/19/22ms，Hard19ms，EX10ms。恰好在边界成功，再早一个采样即BAD、断连和扣槽。保持到尾部仍自动PG，重新按键不能撤销失败。

CN松键判定保留原PG/GR/GD。No LN Release转成普通LN时及课程普通LN使用新PG容差。失败LN的OD8换算记录Miss，使全连与Sudden Death正确反映失败。单键/头部判定、音频、输入补偿与显示时间保持原样。

新记录使用R11/R12或custom11/12，不添加JSON字段。R1–R10及旧custom回放、验证、幽灵按原规则处理。BEST区分判定世代，不重写历史文件。peer v8保持数据结构并拒绝v7。公开Sites仍支持R9/R10，新记录在本地阻止提交，Ranked既有服务器支持限制保留。无服务器或网页部署。

这是基于1.9.9.6的未发布本地修复。保留旧发布、安装、配置和固定基线1.9.6。实体键盘手感及长时间游玩尚未验证。
