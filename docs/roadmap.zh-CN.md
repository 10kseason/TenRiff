# TenRiff 开发路线图（分阶段）

这份路线图记录了继续构建游戏循环时推荐的高层顺序，同时避免范围膨胀。每个阶段都会先固定方向，再往上叠加额外功能。

## 当前基线
- Windows GUI/runtime 是主要受支持路径。
- 项目版本线是 `1.1.3`，是在 `1.1.2 final stable` 基准上继续叠加的后续版本线。
- 默认启用 BMS-first 的菜单/runtime，同时可以通过 config/menu 开关启用 4K-10K `.osu` 支持。
- 关于当前已发布的行为，请先看 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)；这份路线图主要讲方向和剩余工作。

## 0) 先修骨架与主时钟
- 把 **AudioThread** 当成所有时间敏感工作的主时钟。
- **InputThread** 应该从 RawInput/evdev 给事件打时间戳，然后放进 SPSC queue。
- 把谱面时间线标准化为 **sample positions (int64)**，让音频线程可以消费确定性时间戳。
- **Render** 只消费 snapshots 来绘制；判定/分数在音频侧最终结算。

## 0.5) 加固低延迟循环
- 端到端实现一个 backend（WASAPI/ALSA），暴露 device padding，并明确计算 `buffer_start_samples`，这样 mixer 就能在 playback buffer domain 内工作。
- 用离群值剔除、滑动窗口 EMA 更新、设备变化/underrun reset hook、monotonic clamp 来强化 ClockSync，避免 drift 或 spike 时回退。
- 在 AudioThread 中弹出输入、转换为 sample time，并按 late/normal/future 分支安排 keysound，这样晚到输入仍然能发声，而未来输入会被 staging。
- 把判定窗口表达为 sample，增加 callback budget / late inputs / xruns 的 HUD 计数，并在 rate 可变时按比例缩放窗口。
- ✅ 把 replay 记录成 sample-time 输入轨迹（lane/state/sample），并写出 JSON 导出以支持确定性复现。

## 1) 先做出一首歌可以完整游玩
- 在菜单中保持音频 backend 运行，并通过静音回调维持主时钟稳定，再进入 gameplay。
- ✅ 建立 UI 状态机（console）：**Title → Song Select → Play → Result**，并接入 InputThread/SPSC 输入。
- ✅ 已加入 Windows D3D11 菜单 UI（文本 + 背景 + 焦点样式）。
- ✅ 加入异步 SongIndexerThread + 缓存索引（mtime/hash），确保 Song Select 在扫描时依然响应。
- 做一个最小 BMS loader（只保留必要 channel）→ note scheduling → judgement → result screen。
- 通过音频引擎调度 preview audio（不要在 UI 线程直接播放），并在 Song Select 期间预加载 keysounds。

## 1.5) 同时支持 BMS 和 osu! 谱面
- 另外加入 osu! beatmap（mania）loader，与 BMS 共享规范化后的事件模型。
- 保持 scheduling/judgement 路径统一，让 chart format 的差异只停留在加载阶段。
- 在 song select 中暴露 format selection，并确保 replay/result 画面显示来源格式。

## 2) Key remap + 8K/10K modes
- ✅ 按“重映射 UI 流程”规格实现 key remapping UI（包括 NKRO 测试）。
- 这一项完成后，项目会变成一个很不错的个人练习工具。

## 3) 先加入两个随机模式
- ✅ 先实现 **Full Random（FR）** 和 **Super Random（SR）**；把 **AR** 留到后面。

## 4) 接入 launcher
- 处理文件夹检查、首次运行配置创建、错误码分类。
- 做完这一步后，游戏就能在本地 PC 上作为一个完整自洽的程序存在。
