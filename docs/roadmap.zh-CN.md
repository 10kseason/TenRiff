# TenRiff 开发路线图（分阶段）

**固定基准：[1.8.7](baseline-1.8.7.zh-CN.md)**（2026-10-10）。使用 Git tag `1.8.7`、commit `6b060ad47b950ad9e0dbc516fbb40189e8e21347`，不随 `latest` 或新版本自动更新。

> 文档范围：包含早期设计、改进提案与当时分析。未完成的提案和历史测量值不代表当前行为、默认值或性能保证。请核对[当前状态](current-state.zh-CN.md)与[配置](config.zh-CN.md)。

这份路线图记录了继续构建游戏循环时推荐的高层顺序，同时避免范围膨胀。每个阶段都会先固定方向，再往上叠加额外功能。

## 当前基线
- Windows GUI/runtime 是主要受支持路径。
- 固定基准为 `1.8.7`，此源码版本为 `1.8.8`；公开包附带 NK3 P64 与 generalized pattern MLP inference 模型，但不附带 BGA upscaler 模型。选择兼容且权利已厘清的 upscaler ONNX 只会保存路径，BGA Upscaler 会一直保持 off，直到用户手动开启并确认 high-spec warning；不存在自动 benchmark gate。
- 当前菜单/runtime 仅支持 BMS family（`.bms/.bme/.bml/.pms`），并支持 native、bundle/profile TenRiff `skin.json` 与 LR2 skin。
- 关于当前已发布的行为，请先看 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)；这份路线图主要讲方向和剩余工作。

## 1.8.8 分析与 1.9.0 稳定化计划（2026-10-12）

当前源码为 **1.8.8**，包含分析、窗口大小、长LN Mix，以及显示、菜单、键音和开始操作的改进。验证与公开状态见[发布说明](release-1.8.8-gate.md)。插件运行环境、API 和 SDK 尚未实现，`1.9.0` 是未来的稳定化目标。当前固定基准 `1.8.7`、已有标签和发布文件保持不变。此计划规定后续开发优先级；下方早期阶段列表不会自动授权增加尚未完成的核心功能。

| 阶段 | 完成条件 |
| --- | --- |
| 1.8.8 | 按[发布条件](release-1.8.8-gate.md)验证轨道FAST/SLOW、分段准确度、单键/LN事件、分布及样本数、自由窗口、结果PNG、长LN Mix、显示、菜单输入、键音保留与单人开始/重试 |
| 1.9.0 之前的试验版 | 通过有限的只读插件 API，在不修改核心源码的情况下完成并验证另一种结果分析界面和个人选曲过滤器 |
| 1.9.0 稳定化之后 | 保持核心功能范围，以错误、安全、兼容性修复和插件扩展为主 |

- **分析原则：** 只显示当前判定规则下有意义的 LN 项目，不为分析修改判定、分数、连击或血条规则。普通 LN 的自动完成不属于松键时机样本。无法测量的漏击单独计数，绝不作为 `0 ms` 样本。平均值同时显示分布、样本数和离散程度，避免把交替出现的 `−20 ms / +20 ms` 误认为准确击打。参见[分析标准和详细计划（韩语）](result-analysis-1.8.8.ko.md)。
- **核心与插件边界（计划）：** 输入、音频时钟、判定、分数、官方记录保存/验证及多人规则/会话状态由核心负责。首版 API 仅开放只读数据、独立个人设置/分析结果存储，以及获准的结果界面、选曲排序/过滤、HUD 区域和菜单扩展。
- **运行隔离（计划）：** 外部代码在独立运行环境中处理。游戏不等待插件计算完成，输入/音频路径不直接调用外部代码。分别设置权限限制和执行量/时间限制，隔离崩溃、无限循环和过量输出。具体运行技术与预算在试验版验证后决定。
- **多人游戏（计划）：** 由核心阻止外部插件执行及其结果应用，并以默认规则重建受插件影响的游戏状态，不依赖插件自行停用。保留个人键位和音频偏移。如果未来允许单人插件修改判定、谱面或输入，则把这些记录与默认记录分开。
- **API 固定条件（计划）：** 如果每次制作上述两个样例都需要修改引擎，API 就尚不适合固定。提供小型 SDK，包含 API 文档、可直接运行的样例、错误日志及不加载插件的安全启动，并验证用户和 AI 能仅凭 API 与样例制作模组。
- **版本政策（计划）：** `1.9.0` 固定功能范围与兼容性方向，并非永久冻结版本号或文件。区分核心修订版、插件与公开 API 的兼容性版本，并验证现有模组和记录的兼容性。未来更改固定基准需另行批准并同步文档。

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
