# TenRiff 1.9.9.9 — 选曲、记录反馈与结算动态背景

2026-11-03

- 使用提供的 `continue_result_bga_ko_1080p60.mp4` 作为默认结算背景：1920×1080、60fps、60秒无声循环。保留恢复后的结算布局、指标和字体，低不透明度合成避免背景文字遮挡数据。自定义背景优先；减少动态时静止，离开/最小化时释放解码器。
- 选曲使用10行列表，以现有EZ/NM/HD/EX CLEAR徽章替代行分数，未游玩留空，保留文件夹曲目数和右侧MY BEST。显示BPM/键数/Lv/音符数/判定难度及实际NPS MIN/AVG/MAX，加入12秒1080p30背景，仅在大厅隐藏详细准确率。
- NPS统计原谱面TAP/LN开始，包含空白的1秒区间，AVG为算术平均，仅图形压缩为最多96个时间区间的峰值。支持取消和空闲时异步更新，即使试听关闭也生效；固定分支可能与实际随机分支不同。
- 音频菜单可选择主菜单/通关/失败BGM及按钮/选项/滑块/开始/拖放/导入完成音效(MP3/OGG/WAV/FLAC)。修复文件选择线程初始化冲突。Options使用Main Menu 3，Clear使用原Main Menu 5，移除Main Menu 4。
- 分数提升至少1点时显示1.2秒分数效果和NEW RECORD，正式BEST仍按通关优先保存；仅通关提升不算分数纪录。包含升段/FULL COMBO效果和自动降档时连续的血条历史图。
- 可选且可移动的HUD与兼容BEST回放的同一时刻分数比较。S/S+/S++/S+++显示相对95/97/98/99%准确率的差值(%p)。无兼容BEST时为`--`，默认关闭。修复有效暂停本地BEST被排除以及仅比较值变化时不更新的问题。
- 包含文件夹曲目数/左右导航、10标志、隐藏输入后端文字、长记录标题和30帧1秒过渡。结算保留HTML改版之前恢复的布局。

[Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9) · [Validation](release-1.9.9.9-gate.md)

Extract into a new folder and run `launch_win.bat`. Keep existing installations, profiles and records. Existing scoring/judgement and R13/R14 replay/online contracts remain compatible. No website or database deployment. Fixed baseline: **1.9.6**. Physical input/listening, arbitrary skins/charts, long play and other PCs remain unverified.
