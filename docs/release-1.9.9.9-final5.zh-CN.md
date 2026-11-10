# TenRiff 1.9.9.9-final5

[한국어](release-1.9.9.9-final5.ko.md) | [English](release-1.9.9.9-final5.en.md) | [日本語](release-1.9.9.9-final5.ja.md) | [简体中文](release-1.9.9.9-final5.zh-CN.md)

减少 LR2 记录导入中的哈希与谱面查找开销，修复原始 RANK 缺失或超出范围时错误授予 EX 的问题，并修复 Andro 的空白尾部小节使 BPM 显示为 1 的问题。固定基准线保持 final4。

- 只计算 LR2 所需的 MD5，EX 检查不再查找音频或背景文件。避免重复遍历重叠文件夹，找到全部目标谱面后即停止。
- 使用 512 条合成记录和 15,569 个本地谱面，在同一 PC 上顺序测量，从 91.139 秒缩短至 9.852 秒。该样本的灯标和 EX JSON 完全一致，不保证个人 DB 或其他 PC 获得相同速度。
- 原始 RANK 缺失或超出范围时保留基础 LR2 灯标，不再通过游戏的 EASY 回退错误授予 EX。原始 DB 只读，取消或失败保留先前导入结果。
- 参考 BPM 不统计最后一个非零事件小节之后仅定义长度或全为 00 的空小节。检查的 20 个 Andro 谱面及 Safe/Fast 索引均为 165，音符时刻、数量和总时长保持不变。索引缓存 v22 会重建一次旧派生元数据。
- 导入仅用于显示，不改变 TenRiff 成绩、BEST、上传资格、判定、血槽或既有 R1–R14 回放策略。不分发个人 DB 或原始 Andro 谱面；源码只包含两个合成测试数据库。
- 本地候选版本通过 CTest 4/4（1,547 项、跳过 0）及原生 747/747。打包、仅源码 ZIP 独立构建、CI 和公开下载验证作为发布关卡。物理输入、试听、长时间游玩、其他 PC 和个人 DB 完整导入耗时尚未验证。

[LR2 import](lr2-clear-import.zh-CN.md) · [Reference BPM](reference-bpm.zh-CN.md) · [Release gates](release-1.9.9.9-final5-gate.md)
