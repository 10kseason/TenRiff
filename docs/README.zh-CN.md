# TenRiff 文档地图

**固定基准：[1.8.7](baseline-1.8.7.zh-CN.md)**（2026-10-10）。使用 Git tag `1.8.7`、commit `6b060ad47b950ad9e0dbc516fbb40189e8e21347`，不随 `latest` 或新版本自动更新。

**2026-10-13 · 1.8.9.3:** 调整结果页面字号、大厅卡片颜色与轻微Bloom、跟随皮肤轨道颜色的按键设置，以及曲目列表等级颜色。移除指示器背景、目标间距和视觉预设选项，将目标轨道移到音符颜色上方。[变更及验证范围（韩文）](release-1.8.9.3-gate.md)。固定基准仍为1.8.7，保留1.8.9.2的BMS修正与回放兼容性。

**2026-10-13 · 1.8.9.2:** 在原生游玩或键数转换前，将BMS同轨同一时刻重叠的普通音符与长键起点合并为一个长键。保留原文件、长键区间与键音，旧回放维持原有排列。[行为与兼容限制（韩文）](release-1.8.9.2-gate.md)。固定基准仍为1.8.7。

**2026-10-13 · 1.8.9.1：** 增加连续按键设置、全部转换器的键音及长键保留、FAST/SLOW和时间提示背景开关、更大的血条、预览中的轨道拖动及无边框背景的判定文字。旧回放保留原转换方式。[行为、验证与兼容限制（韩文）](release-1.8.9.1-gate.md)。固定基准仍为1.8.7。

**2026-10-12 · 1.8.9：**提供更大的大厅BEST数字、键数模式配色与白色共用外框，采用右上信息／左下分数／右下横向riff map的Studio HUD，并加大结果文字、突出分析标签及居中操作。保留S+颜色与字体，修正Judge Easy/Hard选择未传入新会话的问题。参见[行为、验证与剩余范围](release-1.8.9-gate.md)及[Judge原因与兼容性](judge-session-handoff-1.8.9.ko.md)（韩语）。

**2026-10-12 · 1.8.8：**整合结果分析、自由窗口大小及F9 PNG提示、长LN Mix，并改善显示可读性、难度表URL与筛选、皮肤轨道快捷键、减少键数时的键音保留，以及单人开始和立即重试操作。当前验证、软件包及公开状态见[1.8.8发布说明](release-1.8.8-gate.md)。固定基准仍为1.8.7。

**分析与扩展计划：**区分各轨道分布、时间分段准确度和单键/LN起始、松键、保持失败，无法测量的漏击不作为0ms样本。长LN Mix v2仅保留本地记录与回放，不计入旧BEST或Sites上传。插件与1.9.0稳定化仍为计划。参见[分析标准](result-analysis-1.8.8.ko.md)、[显示/菜单/键音变更](release-feedback-1.8.8.ko.md)、[开始与重试操作](gameplay-start-controls.ko.md)（韩语）。

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
2. [`docs/baseline-1.8.7.zh-CN.md`](baseline-1.8.7.zh-CN.md)
   - 规定当前工作应从哪里继续叠加的 `1.8.7 fixed stable baseline` 基准文档
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
- [`docs/baseline-1.8.7.zh-CN.md`](baseline-1.8.7.zh-CN.md)
  - 后续工作必须保持的 `1.8.7 fixed stable baseline` 基准文档
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
- 在确认应当在哪个基准上继续叠加工作时，要同时看 [`docs/baseline-1.8.7.zh-CN.md`](baseline-1.8.7.zh-CN.md)。
- 由于较旧的设计文档和当前代码可能不同，若出现冲突，请优先按当前代码、[`docs/current-state.zh-CN.md`](current-state.zh-CN.md)、[`docs/config.zh-CN.md`](config.zh-CN.md) 的顺序解释。

## 音频与游玩设置

参见[原生 ASIO](asio-audio.md)、[基准 BPM](reference-bpm.md)、[皮肤预设](skin-presets.md)、[曲库与难度表](library-management.md)和 [LR2 血条比较](lr2-gauge-audit.ko.md)。
