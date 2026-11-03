> Historical local-candidate design/evidence. The included release behavior and launch instructions are in [1.9.9.9](release-1.9.9.9.zh-CN.md); local-only/profile notices below describe that earlier candidate. The final Result layout is the restored layout.

# 选曲通关徽章、谱面信息与动态背景

2026-11-03 本地候选版，继承结算回退候选并保留其结算布局。

将曲目列表的数字分数和未游玩的 `--` 替换为现有 EZ/NM/HD/EX CLEAR 徽章，复用标志、字体、语义颜色和 EX 渐变。失败/辅助记录不会变成正式通关。保留文件夹曲目数量和右侧个人最佳分数。

信息区依次显示 BPM / 键数 / Lv / 音符数 / 判定难度，下方 NPS 条形图内显示 MIN / AVG / MAX。使用所选原始谱面的实际音符开始位置；AVG 是实际平均值，而非 MED 改名。未加载的数据不虚构。仅在大厅隐藏详细准确率，保留详细分数、普通准确率、结算与存储值。

NPS 从谱面时刻0计算到最后一个音符开始所在的1秒区间，空白区间按0计入。TAP/LN开始各计1次，排除尾端与地雷。MIN/算术AVG/MAX使用全部原始区间，仅图形压缩为最多96个时间区间的峰值。预览使用1.0倍速和固定分支，可能与随机游玩的分支不同。未加载、超过8MB或6小时以上的谱面不显示数值；保留原MED缓存与记录格式。

使用此前生成的12秒、1080p30、无声视频作为默认选曲背景。减少动态设置保持静止画面，用户明确指定的背景优先；离开选曲时释放独立解码器。

使用独立 `lobby-detail-local` 配置，保留原候选版、安装和记录，不更改判定、分数、回放、存储格式、公开版本或固定基准1.9.6。计算范围与验证限制见随附验证记录。
