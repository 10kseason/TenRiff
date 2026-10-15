# TenRiff 开发路线图（分阶段）

**2026-10-15 · 1.9.0.2 发布候选版 — 发布验证流程：**基于已发布的1.9.0.1，将Options改为绿色，仅在选择内置5K Aery难度表时显示绿色⑤，并将等级设为红色、分数设为白色，新增`lv:`等级搜索、新建配置debounce默认0ms和音量1%步长。主音量默认10%，固定基准仍为1.9.0。[变更与用法](feedback-1.9.0.2.zh-CN.md)。 公开状态与可下载文件以发布页面为准。[范围与验证](release-1.9.0.2-gate.md)。

**2026-10-15 · 1.9.0.1:** 包含默认主音量10%和大厅图标中心对齐。提供试听×0.75、个人档案指定F5/F6 -10/+10、Xbox/XInput按钮输入、文件选择框防重复，以及BMS BPM/STOP顺序修正与旧回放兼容。固定基准保持1.9.0。 [变更说明](feedback-20261015.zh-CN.md) · [发布验证范围](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 新固定基准：** 将已检查的本地候选版提升为指定正式版本，包括BMS/菜单音量分离、大厅/筛选/皮肤操作、等级与SUBARTIST、韩文模式界面、EX判定和上传状态。普通/Hard使用R5，EX使用R6；保留旧R1–R5记录及已保存设置。[固定基准](baseline-1.9.0.zh-CN.md) · [验证范围](release-1.9.0-gate.md)。下方带日期的条目保留当时发布历史。

**2026-10-14 · 1.8.9.7:** 输入线程请求MMCSS Games/High并关闭执行速度电源节流，主页菜单边框使用与图标一致的颜色和柔和缓存辉光。仅将TenRiff自带菜单、结果音乐降至原来的40%，BMS曲目、键音和试听保持原有音量比例。固定基线仍为1.8.7，物理输入延迟改善尚未证实。 [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 输入和游戏推进/HUD的短等待使用可复用的高分辨率计时器，RawInput由消息唤醒。采用对比版本B的方式，保留判定范围、R1–R5、回放和固定基线1.8.7。手动试玩感受随轮次变化，不能保证消除所有卡顿。 [Scope / 검증](release-1.8.9.6-gate.md).

**固定基准：[1.9.0](baseline-1.9.0.zh-CN.md)**（2026-10-14明确请求）。使用Git tag `1.9.0`，不随`main`或`latest`自动更新；旧1.8.7基准和既有发布版本保留为历史。

**2026-10-13 · 1.8.9.4:** 调整结果页面的大等级与分数布局，显示详细准确率，并区分四种Gauge Shift CLEAR颜色。将BGM和键音音量移到音频设置顶部，新设置均默认为100%。包含韩文皮肤名称调整、明确标注的obj谱师与BPM!变速提示，以及同一谱面时刻相同音源的重复播放修正。谱师上传为可选项，保留现有记录ID。[变更、兼容性及验证范围（韩文）](release-1.8.9.4-gate.md)。验证和游戏、网页发布状态分别记录，固定基准仍为1.8.7。

**2026-10-13 · 1.8.9.3:** 调整结果页面字号、大厅卡片颜色与轻微Bloom、跟随皮肤轨道颜色的按键设置，以及曲目列表等级颜色。移除指示器背景、目标间距和视觉预设选项，将目标轨道移到音符颜色上方。[变更及验证范围（韩文）](release-1.8.9.3-gate.md)。固定基准仍为1.8.7，保留1.8.9.2的BMS修正与回放兼容性。

**2026-10-13 · 1.8.9.2:** 在原生游玩或键数转换前，将BMS同轨同一时刻重叠的普通音符与长键起点合并为一个长键。保留原文件、长键区间与键音，旧回放维持原有排列。[行为与兼容限制（韩文）](release-1.8.9.2-gate.md)。固定基准仍为1.8.7。

**2026-10-13 · 1.8.9.1：** 增加连续按键设置、全部转换器的键音及长键保留、FAST/SLOW和时间提示背景开关、更大的血条、预览中的轨道拖动及无边框背景的判定文字。旧回放保留原转换方式。[行为、验证与兼容限制（韩文）](release-1.8.9.1-gate.md)。固定基准仍为1.8.7。

> 文档范围：包含早期设计、改进提案与当时分析。未完成的提案和历史测量值不代表当前行为、默认值或性能保证。请核对[当前状态](current-state.zh-CN.md)与[配置](config.zh-CN.md)。

这份路线图记录了继续构建游戏循环时推荐的高层顺序，同时避免范围膨胀。每个阶段都会先固定方向，再往上叠加额外功能。

## 当前基线
- Windows GUI/runtime 是主要受支持路径。
- 固定基准为 `1.9.0`，此源码为发布候选版 `1.9.0.2`；公开包附带 NK3 P64 与 generalized pattern MLP inference 模型，但不附带 BGA upscaler 模型。选择兼容且权利已厘清的 upscaler ONNX 只会保存路径，BGA Upscaler 会一直保持 off，直到用户手动开启并确认 high-spec warning；不存在自动 benchmark gate。
- 当前菜单/runtime 仅支持 BMS family（`.bms/.bme/.bml/.pms`），并支持 native、bundle/profile TenRiff `skin.json` 与 LR2 skin。
- 关于当前已发布的行为，请先看 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)；这份路线图主要讲方向和剩余工作。

## 1.9.0.2发布候选版验证范围

以下结果对应最终恢复Aery ⑤之前的候选版。开发构建与ZIP解压源码的独立构建均通过Release构建、CTest 3/3（各1,224项，integration skips 0）、编辑器35/35及native MenuApp/GameSession 307/307。native检查含22项搜索按键处理检查。恢复前的渲染通过6个fixture/360 Presents/86 geometry hit checks。恢复后的构建及Aery FHD/小窗口、默认Lv、自定义表的显示检查作为独立发布验证项确认。

首次打包检查中，现有皮肤轨道数字快捷键测试失败过一次。随后在未修改测试可执行文件的情况下，定向重试、完整CTest重试和独立构建检查均通过，但 **根本原因仍未确认**。保留首次失败，不将重试成功视为间歇性问题已修复。

本地软件包、独立构建证据及初始失败保存在单独的evidence中。最终发布包对应关系、PR/main CI和公开资产检查遵循[发布验证流程](release-1.9.0.2-gate.md)，本文不提前标记为通过。实际公开状态以发布页面为准。内部按键dispatch和geometry检查不能代替实体输入验证；设备、延迟、长时间游玩和其他PC仍未验证。固定基准保持1.9.0。

## 1.9.0.1之后的验证范围

此次仅涉及试听音量、Hi-Speed绑定、文件选择框、BMS时间和XInput按钮。实体手柄及长时间游玩需单独验证；时间版本2的网页支持属于后续任务。网页能区分两种时间之前，受影响的新记录保持本地保存。固定基准1.9.0与插件、结构拆分的既有边界保持不变。

## 1.9.0稳定范围与后续计划

1.9.0用于发布已检查的候选版并更新固定基准，范围为音量、设置、界面、署名、EX/R6和上传诊断，不保证所有PC的延迟均已解决。旧计划中的插件先行条件由此次明确发布范围替代。插件运行时/API/SDK及GameSession的大规模拆分仍未实现，属于后续独立工作。未来插件先考虑只读扩展，输入、音频、判定及正式记录仍由核心负责。继续修复缺陷、兼容性和安全问题；再次更改基准仍需明确请求和同步验证。


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

## 1.5) 加固 BMS-only 谱面支持
- 之前的 multi-format 方向在当前 release line 已 superseded；loading、indexing、replay、result 与 difficulty-table 都聚焦 BMS family。
- 不重新引入 archive 或其他格式 import path，继续强化真实曲包的 encoding、keysound、BGA、long note 与 lane layout 兼容性。
- optional integration 继续采用 user-supplied、明确启用前保持 disabled、失败时可安全回到 native behavior 的边界。

## 2) Key remap + 8K/10K modes
- ✅ 按“重映射 UI 流程”规格实现 key remapping UI（包括 NKRO 测试）。
- 这一项完成后，项目会变成一个很不错的个人练习工具。

## 3) Lane transform / Random mode
- ✅ 已实现 **Mirror**、**Full Random（FR）** 与 **Super Random（SR）**；**AR** 延后到行为定义明确之后。

## 4) 接入 launcher
- 处理文件夹检查、首次运行配置创建、错误码分类。
- 做完这一步后，游戏就能在本地 PC 上作为一个完整自洽的程序存在。

## 5) 在公开排名前先建立可信记录
- 按 [`ranked-integrity-plan.en.md`](ranked-integrity-plan.en.md) 分阶段完成统一 eligibility reason、Local Records 页面、只读 Online Records、shadow submission，最后才开放 verified ranking。
- 服务器必须通过已批准的 BMS chart SHA-256 与 replay evidence 重新计算结果，不能把客户端 score claim 当作可信来源。
- `.osu` chart 以及 osu 派生 import、ruleset、scoring、conversion 一律不可注册 ranked；native BMS 的辅助 OD8 统计仅作为本地 metadata。

## 6) 随规模增长重构 menu architecture
- `1.6.0` 已完成 Phase 0-6：显式 navigation、所有 settings family 的 typed controller 与 exhaustive screen descriptor 现在构成维护基线。
- 按照 [`menu-refactor-plan.md`](menu-refactor-plan.md) 的分阶段设计与 phase gate 执行，不要从整套 menu 的一次性重写开始。
- 当新增 screen 必须同时修改 input dispatcher、render-data builder 和多个共享 cursor/return flag，或 screen behavior 已无法用 focused unit test 覆盖时，启动这项工作。
- 先补 navigation、settings persistence、pointer/keyboard parity 的 characterization test，再在不改变 visible behavior 的前提下，将 `MenuApp` 按 screen family 逐步拆成局部 state/controller。
- 用明确的 navigation stack/back policy 替代共享 return-screen flag；包括 slider 在内的 adjustable row 应使用 typed value range 与 persistence callback，而不是 row index 条件分支。
- 将 render DTO 构建与 input/state mutation 分离。每次 extraction 都通过 focused test 和正常 client build 后再开始下一组，不做一次性全量 rewrite。
