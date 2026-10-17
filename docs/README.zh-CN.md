**2026-10-17 · 1.9.5.1:** [判定线、FAST/SLOW 与文件选择](release-1.9.5.1.zh-CN.md). FAST/SLOW从最后一次最大判定范围外的输入起保留0.35秒。最大判定不会立即清除它，也不会延长显示时间；新的范围外判定会立即更新。常显刻度与点/条形标记的0.7秒寿命保持独立。

**2026-10-17 · 1.9.5:** 菜单和结果音乐也使用声音选项中选定的设备。FAST为蓝色，最高判定为绿色，LATE为红色；最高判定也会更新FAST/SLOW。选曲循环为OFF → 镜像 → 随机 → S随机 → 固定搓盘随机 → OFF。 [音乐输出统一与判定指示器](release-1.9.5.zh-CN.md).

**2026-10-16 · 1.9.3发行范围：** 包含空POOR准确率排除(R7/R8)、BMS判定标记、游戏内等级与小节线、FAST/SLOW常显修复、安全加载默认值、残差/完整图像ONNX以及受支持架构的外部Safetensors转换。运行包与源码包均不附带BGA权重，默认OFF、路径为空。保留访客上传排除、固定等级及1.9.0基准。[变更与兼容性](release-1.9.3.zh-CN.md) · [验证流程](release-1.9.3-gate.md)。发布状态及下载以[1.9.3发行页](https://github.com/10kseason/TenRiff/releases/tag/1.9.3)为准，下方带日期说明保留当时历史。

> 先前1.9.2：[访客/在线、原始分辨率放大与UI变更](release-1.9.2.zh-CN.md)。

> 2026-10-16: [FFmpeg去块与外部ONNX x2](deblock-lunasr-20261016.zh-CN.md)

> 2026-10-16: [先以原始分辨率修复BGA，再适配显示区域](native-upscale-20261016.zh-CN.md)

# TenRiff 文档地图

**2026-10-16 · 1.9.1.1 UI:** [主页、图形、加载与固定评级 — 1.9.1.1后续候选版](ui-feedback-20261016.zh-CN.md).

**2026-10-15 · 1.9.1.1 BMS在线支持：** 将新BMS血条、时间计算及RANDOM分支记录提交到Sites社区排行榜，并按规则和选中的分支内容隔离排名。需要新版客户端及对应网页部署；Sites仍是`client_submitted`记录，独立Ranked验证服务器的限制保持不变。下方1.9.1仅限本地的说明保留该版本历史；保留已发布文件及固定基准1.9.0。 [范围、兼容性与验证](feedback-1.9.1.1.zh-CN.md)。

**2026-10-15 · 1.9.1:** 用户授权的1.9.1包含此前音量、输入、尾奏修复，BMS TOTAL/RANDOM兼容、结果颜色及10形标志。主页语言按钮按EN → KO → JA → ZH循环并保存选择。新BMS血条/分支记录在服务器验证支持前仅存本地。固定基准仍为1.9.0，保留旧发行版、安装和配置。下方带日期的本地记录中不升版本、未发布等表述保留当时状态；当前以此1.9.1范围为准。 [变更与兼容性](feedback-1.9.1.zh-CN.md) · [发布验证流程](release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL、RANDOM与结果/标志后续修改 — 未发布、本地构建及CTest回归验证通过：** 本次本地修改基于1.9.0.2。结果页等级改用与选曲大厅相同的红色等级文字及键数符号颜色，标志改为提供的10形图案。新的普通BMS游玩根据#TOTAL及音符数使用LR2式恢复和伤害；谱面内#RANDOM分支使用独立于轨道随机的种子。旧录像保留原有行为，并包含此前的音量统一修改。已发布1.9.0.2、固定基准1.9.0及版本号均不变。 [公式、分支、兼容性与验证范围（英文）](bms-total-random-followup.en.md).

**2026-10-15 · BMS音量统一后续修改 — 未发布、本地构建及CTest回归验证通过：** 此1.9.0.2基础上的本地修改将伴奏、键音独立调节合并为BMS演奏音量（韩文显示：**BMS 연주 볼륨**）。新默认值为20%，保留已保存值。主音量10%、TenRiff音乐100%的默认值、其余音量1个百分点步长及大厅试听×0.75均保留。不更改版本号或固定基准1.9.0。下方带日期的条目保留当时记录。 [设置与兼容说明](config.zh-CN.md#audio).

本次未发布的后续修改还会显示难度表加载状态，并将debounce UI改为1ms步长、最大20ms（默认0ms）。停用旧的5→8ms自动改写，分别保留已保存的5ms和8ms。皮肤的韩文标签改为 `LN 끝 줄임`，外观和判定行为不变。普通单人模式开启听到结束后，最后音符及LN尾端处理完毕时按 `Esc` 会跳过尾奏直接进入结果，不弹出暂停菜单。

**2026-10-15 · 1.9.0.2 发布候选版 — 发布验证流程：**基于已发布的1.9.0.1，将Options改为绿色，仅在选择内置5K Aery难度表时显示绿色⑤，为已注册4/6/8/10键难度表的符号使用对应颜色，并将等级设为红色、分数设为白色，新增`lv:`等级搜索、新建配置debounce默认0ms和音量1%步长。将选曲的排序、过滤和难度表选择器移至Song Library旁的页眉，并增加专辑封面高度。主音量默认10%，固定基准仍为1.9.0。[变更与用法](feedback-1.9.0.2.zh-CN.md)。 公开状态与可下载文件以发布页面为准。[范围与验证](release-1.9.0.2-gate.md)。

**2026-10-15 · 1.9.0.1:** 包含默认主音量10%和大厅图标中心对齐。提供试听×0.75、个人档案指定F5/F6 -10/+10、Xbox/XInput按钮输入、文件选择框防重复，以及BMS BPM/STOP顺序修正与旧回放兼容。固定基准保持1.9.0。 [变更说明](feedback-20261015.zh-CN.md) · [发布验证范围](release-1.9.0.1-gate.md).

**2026-10-14 · 1.9.0 / 新固定基准：** 将已检查的本地候选版提升为指定正式版本，包括BMS/菜单音量分离、大厅/筛选/皮肤操作、等级与SUBARTIST、韩文模式界面、EX判定和上传状态。普通/Hard使用R5，EX使用R6；保留旧R1–R5记录及已保存设置。[固定基准](baseline-1.9.0.zh-CN.md) · [验证范围](release-1.9.0-gate.md)。下方带日期的条目保留当时发布历史。

**2026-10-14 · 1.8.9.7:** 输入线程请求MMCSS Games/High并关闭执行速度电源节流，主页菜单边框使用与图标一致的颜色和柔和缓存辉光。仅将TenRiff自带菜单、结果音乐降至原来的40%，BMS曲目、键音和试听保持原有音量比例。固定基线仍为1.8.7，物理输入延迟改善尚未证实。 [Scope / 검증](release-1.8.9.7-gate.md).

**2026-10-13 · 1.8.9.6:** 输入和游戏推进/HUD的短等待使用可复用的高分辨率计时器，RawInput由消息唤醒。采用对比版本B的方式，保留判定范围、R1–R5、回放和固定基线1.8.7。手动试玩感受随轮次变化，不能保证消除所有卡顿。 [Scope / 검증](release-1.8.9.6-gate.md).

**2026-10-13 · 1.8.9.5:** R5按时间顺序处理LN尾部和自动判定，将判定与音频提前写入分离，并统一输入/显示时间及BPM/STOP边界位置。恢复深色大厅卡片与彩色图标，主音量移至顶部。保留R1–R4及固定基线1.8.7。[范围与验证](release-1.8.9.5-gate.md)。

**固定基准：[1.9.0](baseline-1.9.0.zh-CN.md)**（2026-10-14明确请求）。使用Git tag `1.9.0`，不随`main`或`latest`自动更新；旧1.8.7基准和既有发布版本保留为历史。

**2026-10-13 · 1.8.9.4:** 调整结果页面的大等级与分数布局，显示详细准确率，并区分四种Gauge Shift CLEAR颜色。将BGM和键音音量移到音频设置顶部，新设置均默认为100%。包含韩文皮肤名称调整、明确标注的obj谱师与BPM!变速提示，以及同一谱面时刻相同音源的重复播放修正。谱师上传为可选项，保留现有记录ID。[变更、兼容性及验证范围（韩文）](release-1.8.9.4-gate.md)。验证和游戏、网页发布状态分别记录，固定基准仍为1.8.7。

**2026-10-13 · 1.8.9.3:** 调整结果页面字号、大厅卡片颜色与轻微Bloom、跟随皮肤轨道颜色的按键设置，以及曲目列表等级颜色。移除指示器背景、目标间距和视觉预设选项，将目标轨道移到音符颜色上方。[变更及验证范围（韩文）](release-1.8.9.3-gate.md)。固定基准仍为1.8.7，保留1.8.9.2的BMS修正与回放兼容性。

**2026-10-13 · 1.8.9.2:** 在原生游玩或键数转换前，将BMS同轨同一时刻重叠的普通音符与长键起点合并为一个长键。保留原文件、长键区间与键音，旧回放维持原有排列。[行为与兼容限制（韩文）](release-1.8.9.2-gate.md)。固定基准仍为1.8.7。

**2026-10-13 · 1.8.9.1：** 增加连续按键设置、全部转换器的键音及长键保留、FAST/SLOW和时间提示背景开关、更大的血条、预览中的轨道拖动及无边框背景的判定文字。旧回放保留原转换方式。[行为、验证与兼容限制（韩文）](release-1.8.9.1-gate.md)。固定基准仍为1.8.7。

**2026-10-12 · 1.8.9：**提供更大的大厅BEST数字、键数模式配色与白色共用外框，采用右上信息／左下分数／右下横向riff map的Studio HUD，并加大结果文字、突出分析标签及居中操作。保留S+颜色与字体，修正Judge Easy/Hard选择未传入新会话的问题。参见[行为、验证与剩余范围](release-1.8.9-gate.md)及[Judge原因与兼容性](judge-session-handoff-1.8.9.ko.md)（韩语）。

**2026-10-12 · 1.8.8：**整合结果分析、自由窗口大小及F9 PNG提示、长LN Mix，并改善显示可读性、难度表URL与筛选、皮肤轨道快捷键、减少键数时的键音保留，以及单人开始和立即重试操作。当前验证、软件包及公开状态见[1.8.8发布说明](release-1.8.8-gate.md)。固定基准仍为1.8.7。

**分析与扩展计划：**区分各轨道分布、时间分段准确度和单键/LN起始、松键、保持失败，无法测量的漏击不作为0ms样本。长LN Mix v2仅保留本地记录与回放，不计入旧BEST或Sites上传。插件仍为后续计划，不属于1.9.0。参见[分析标准](result-analysis-1.8.8.ko.md)、[显示/菜单/键音变更](release-feedback-1.8.8.ko.md)、[开始与重试操作](gameplay-start-controls.ko.md)（韩语）。

**2026-10-09 · TenRiff 1.8.7:** 改善输入校准和谱面加载音频，支持LR2单人Studio Deck、750ms FAST/SLOW保留、按键重新分配、选项整合及完整曲名自动缩放。 [1.8.7 release](release-1.8.7-gate.md).

**2026-10-08 · TenRiff 1.8.6:** 为Native单人游玩新增Studio Deck HUD。左右Shift可绑定为游玩按键；F5/F6将Hi-Speed减半/加倍，F11打开聊天。仍可选择经典HUD。 [1.8.6 release](release-1.8.6-gate.md).

**2026-10-06 · TenRiff 1.8.5:** 在关闭VSync的游玩中按绘制顺序批量提交受支持的音符图像，以减少CPU绘制调用。不兼容的皮肤、长音符和开启VSync时保留原有路径。大厅、菜单以及R1–R4判定和回放规则保持不变。 [1.8.5 release](release-1.8.5-gate.md).

**2026-10-05 · TenRiff 1.8.42:** 修复高密度帧稍微超出渲染预算时的额外等待，并在性能叠加层关闭时跳过统计收集与排序。保留音符画质、显示数量以及R1–R4判定和回放规则。 [1.8.42 release](release-1.8.42-gate.md).

**2026-10-05 · TenRiff 1.8.41:** 调整Judge Hard为EASY基准PG/GR/GD ±17.5/55.714286/98.571429ms。所有BMS RANK的BAD固定为Hard ±225ms、Easy ±210ms。新记录使用R4，保留R1/R2/R3回放及现有网页记录。 [1.8.41 release](release-1.8.41-gate.md).

**2026-10-05 · TenRiff 1.8.4:** 改善暂停鼠标操作和设置点击，新增实时Pacemaker、练习用Auto Scratch及完整设置迁移。修复Ghost Battle玩家区域尺寸、LN不透明度、HUD扫描和帧队列限制。 [1.8.4 release](release-1.8.4-gate.md).

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

如果你已经先读过根目录的 [`README.zh-CN.md`](../README.zh-CN.md)，那么这份文档就是下一步的详细文档索引。这里同时包含设计文档与当前状态文档，所以想快速建立上下文时，最有效的阅读顺序如下。

这套代码库也应被明确理解为一个 `vibe coding` 作品，它是在快速迭代与实验中逐步发展出来的。

## 文档依据与近期变更

当前行为说明冲突时，先核对**同一提交的实现代码与回归测试**，再同步修正 `current-state`、`config` 及翻译。`baseline-*`、`release-*` 记录各自版本；早期设计和路线图提案不是当前实现或性能保证。

- [最新判定范围、RANK表与LN规则（韩语）](judgement-windows.md)
- [1.8.3 release](release-1.8.3-gate.md)
- [1.8.3 audio / LN / RANK](audio-ln-rank.ko.md)
- [Skin timing controls](timing-feedback-controls.ko.md)
- [Note fog / song ending / pause resume](skin-fade-audio-resume.ko.md)
- [1.8.2 release](release-1.8.2-gate.md)
- [1.8.1 release](release-1.8.1-gate.md)
- [1.8.1 Studio](menu-studio-1.8.1.ko.md)
- [1.8.0](release-1.8.0-gate.md)
- [1.7.10](release-1.7.10-gate.md)
- [1.7.9](release-1.7.9-gate.md)
- [1.7.8](release-1.7.8-gate.md)
- [1.7.2 history](release-1.7.2-gate.md)
- [1.7.1 history](release-1.7.1-gate.md)
- [UI](menu-visual-polish.md)
- [Gameplay / Multiplayer](gameplay-polish-followup.md)

## 推荐阅读顺序
1. [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)
   - 当前产品状态、核心子系统、已验证命令和仍待手动验证的项目
2. [`docs/baseline-1.9.0.zh-CN.md`](baseline-1.9.0.zh-CN.md)
   - 规定当前工作应从哪里继续叠加的 `1.9.0 fixed stable baseline` 基准文档
3. [`docs/gameplay-guide.zh-CN.md`](gameplay-guide.zh-CN.md)
   - 面向真实游玩的启动方式、选歌、操作、HUD、判定、结果说明
4. [`docs/config.zh-CN.md`](config.zh-CN.md)
   - 实际配置、profile 与 keymap 结构
5. [`docs/localization.zh-CN.md`](localization.zh-CN.md)
   - 当前英语/韩语/日语 UI 结构，以及以后新增更多语言时需要修改的文件边界
6. [`docs/menu.zh-CN.md`](menu.zh-CN.md)
   - 菜单/状态机/选歌流程
7. [`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
   - 播放循环与数据流
8. [`docs/roadmap.zh-CN.md`](roadmap.zh-CN.md)
   - 中长期工作方向
9. [`docs/developer-extension-guide.zh-CN.md`](developer-extension-guide.zh-CN.md)
   - 新增 mode/mod 的开发者指南，说明应该改哪些 C++ 文件

## 哪些文档算作权威来源
- [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)
  - 当前实现状态的总结文档
- [`docs/baseline-1.9.0.zh-CN.md`](baseline-1.9.0.zh-CN.md)
  - 后续工作必须保持的 `1.9.0 fixed stable baseline` 基准文档
- [`docs/config.zh-CN.md`](config.zh-CN.md)
  - 以实际的 `config/config.json`、`profiles/<name>/config.json`、`keymap.json` 为准

## 历史/设计文档
- [`docs/menu.zh-CN.md`](menu.zh-CN.md)
  - 菜单/状态机/低延迟方向性的设计文档
- [`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
  - 播放循环的初期设计与数据流说明
- [`docs/localization.zh-CN.md`](localization.zh-CN.md)
  - 当前 UI 本地化结构与未来新增语言时的参考文档
- [`docs/latency.zh-CN.md`](latency.zh-CN.md)、[`docs/modes.zh-CN.md`](modes.zh-CN.md)、[`docs/gap-analysis.zh-CN.md`](gap-analysis.zh-CN.md)、[`docs/roadmap.zh-CN.md`](roadmap.zh-CN.md)
  - 按功能拆分的设计、分析与中长期方向文档
- [`docs/developer-extension-guide.zh-CN.md`](developer-extension-guide.zh-CN.md)
  - 面向开发者的 mode/mod 扩展指南

## 致谢
- 感谢 OpenAI Codex、ChatGPT、Claude Code、Gemini，以及帮助一起验证项目的各位 guest testers。

## 翻译覆盖
- 根目录 `README.md` 还提供 [`README.en.md`](../README.en.md)、[`README.zh-CN.md`](../README.zh-CN.md) 与 [`README.ja.md`](../README.ja.md)。
- `docs/` 下的主要文档以 `.en.md`、`.zh-CN.md`、`.ja.md` 后缀并排提供翻译版本。
- 如果翻译内容与韩文原文冲突，应优先以当前代码、`docs/current-state.md`、`docs/config.md` 为准。

## 实用规则
- 在确认当前行为时，优先看 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)。
- 在确认应当在哪个基准上继续叠加工作时，要同时看 [`docs/baseline-1.9.0.zh-CN.md`](baseline-1.9.0.zh-CN.md)。
- 由于较旧的设计文档和当前代码可能不同，若出现冲突，请优先按当前代码、[`docs/current-state.zh-CN.md`](current-state.zh-CN.md)、[`docs/config.zh-CN.md`](config.zh-CN.md) 的顺序解释。

## 音频与游玩设置

参见[原生 ASIO](asio-audio.md)、[基准 BPM](reference-bpm.md)、[皮肤预设](skin-presets.md)、[曲库与难度表](library-management.md)和 [LR2 血条比较](lr2-gauge-audit.ko.md)。
