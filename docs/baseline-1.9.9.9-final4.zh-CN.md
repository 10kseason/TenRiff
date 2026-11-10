# TenRiff 1.9.9.9-final4 固定基准线

[한국어](baseline-1.9.9.9-final4.md) | [English](baseline-1.9.9.9-final4.en.md) | [日本語](baseline-1.9.9.9-final4.ja.md) | [简体中文](baseline-1.9.9.9-final4.zh-CN.md)

2026-11-10 应用户要求,通用源码基准线从 1.9.6 提升为 **1.9.9.9-final4**。不自动跟随 main/latest。以下策略在基准线变更后继续有效。

基准线的提交、标签和发布文件哈希记录在[公开发布页](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9-final4)和工作区的 `BASELINE.md` 中。已公开的标签与发布文件、过去的源码、候选版本、档案和原始谱面均予以保留。

- [变更与兼容性](release-1.9.9.9-final4.zh-CN.md) · [发布关卡](release-1.9.9.9-final4-gate.md) · [此前的基准线 1.9.6](baseline-1.9.6.zh-CN.md)
- 判定:Normal PG 为 VEZ/EZ 22ms、NM 19ms、HD 15ms、VHD 10ms,GR/GD/BAD 保持原值。Hard = Normal NM,EX = Normal VHD。没有额外倍率或取整。[Normal / Hard / EX](judgement-baseline-20261018.zh-CN.md)
- 回放:R1–R14、custom 变体、BEST 世代和 peer 协议的既有行为不会改变。修改判定时使用新的规则集 ID。
- 评级:S+++ 99 / S++ 98 / S+ 97 / S 95 / A+ 93 / A 90 / B 85 / C 80 / D 70,存活且低于 70 为 E,血槽失败为 F。保留 AUTO 与 --。
- 在线:保留 Guest/custom 排除和 client_submitted 边界。导入的 LR2 通关灯仅用于显示,不会被提交。
- 发布:不包含 BGA 权重,保留 NK3 允许列表,默认值为 OFF/空/Auto。不包含用户的 LR2 数据库等个人数据。

## 与 1.9.6 的兼容性

1.9.6 之后新增的规则集(R11–R14)、LN Mix v3、键位转换版本以及 BMS 时序、血槽和 RANDOM 策略字段都以版本形式保存在记录中,验证器会重现过去的策略。已保存的档案、校准值和快捷键无需迁移即可读取。本次发布本身不改变判定、分数或回放计算。
