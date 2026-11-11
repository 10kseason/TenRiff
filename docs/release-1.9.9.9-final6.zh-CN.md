# TenRiff 1.9.9.9-final6

[한국어](release-1.9.9.9-final6.ko.md) | [English](release-1.9.9.9-final6.en.md) | [日本語](release-1.9.9.9-final6.ja.md) | [简体中文](release-1.9.9.9-final6.zh-CN.md)

分开7+1与8K键位、转盘默认红色并修正16轨间隙。新增Unlimited的VSync处理、时序标记缓存和LR2oraja成绩导入，使用BMS作者等级及高分辨率BGA原始帧。

- 在选项 → 按键设置和 NKRO 测试中分别选择 7+1 和 8K。转盘和 1 至 7 号键均可设置主键和辅助键。
- 没有 7+1 设置的配置会沿用当前 8K 键位。用旧版本重新保存按键配置会丢失 7+1 专用设置。
- 转盘默认使用红色。配置和皮肤中保存的颜色优先。
- 14+2 DP 和 16K 的中央间隙保持空白。单键音符和长按音符绘制在各自区域内，此行为同样适用于幽灵回放。
- `Unlimited`会关闭VSync，游戏最高1500 FPS，菜单最高300 FPS。`Match Display`应用保存的VSync设置。
- 点与条码时序标记在连续帧中保持稳定时缓存位图，历史变化时立即绘制最新内容。
- 在选项 → 配置 → 导入成绩数据库中选择LR2oraja的`player/<profile>/score.db`。LN/CN/HCN分别保存，选曲显示来源、通关类型、EX、连击与BP，按LN、CN、HCN顺序采用第一条记录。
- DB的ghost保存逐音符判定类别。恢复毫秒分布需要另一个包含实际输入时间的回放输入日志。通关、连击与最小BP可能来自不同的最佳成绩游玩。
- 游戏内基本难度使用BMS的`#PLAYLEVEL`，并移除内部计算选项。旧计算缓存会在下次索引时更新。选中外部难度表时使用该表的等级。
- 高度超过480像素的BGA使用原始帧尺寸，图片ONNX处理适用于高度480像素及以下的来源。

报告者PC的帧率与撕裂、真实个人DB、物理输入、试听和长时间游玩仍需进一步验证。[成绩导入](lr2-clear-import.zh-CN.md)

[Release verification](release-1.9.9.9-final6-gate.md)
