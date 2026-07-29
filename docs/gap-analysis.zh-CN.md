# TenRiff 菜单手册对照差距分析（v0.1）

- 参考文档：`개발메뉴얼(v0.1)/개발지시사항.txt`、`개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`、
  `개발메뉴얼(v0.1)/UI 개발지시사항.txt`、`개발메뉴얼(v0.1)/RAW 인풋과 멀티스레드 활용과 최적화에 대한 개발 지시사항 메뉴얼.txt`
- 分析时间：2025-12-23
- 范围：`src/`、`docs/`、`launch_*.{bat,sh}`、`profiles/default/`（不含 Songs 文件夹）

## 严重度标准
- Critical：会阻断端到端游玩循环，或者违背音频主时钟/确定性这类核心原则的差距
- High：缺少核心功能/UX 要求，导致可用性大幅下降的差距
- Medium：重要但还没有到立即阻断程度的功能/验证/补强差距
- Low：质量/便利性层面的改进差距

## 摘要（以未解决项为准）
- Critical：0
- High：1
- Medium：8
- Low：1

## 目前已满足 / 部分满足的项目（摘要）
- 已搭建 BMS parser/normalize/timeline（sample time）
- 已实现基础 10K channel mapping
- 已反映 SpeedManager / GaugeManager 基本规格
- 已实现仅支持 BMS family 的谱面 loader 与 indexer
- 已实现 RawInput + InputThread + SPSCQueue + ClockSync scaffolding
- 已实现输入 polling（1000/2000/4000/8000Hz）与 RenderThread 分离
- 已有 WASAPI backend + AudioThread 骨架
- 已有 launcher scripts 基础 bootstrap

## 差距详情

### 1) 缺少基于音频主时钟的“播放循环”
- 要求：AudioThread 是主时钟，判定/气条/keysound 都在音频线程处理
- 当前状态：已在音频回调中实现输入消费与判定/气条更新循环
- 差距：keysound / 实际音频混音尚未实现
- 严重度：Medium
- 相关文件：`src/audio/AudioThread.*`、`src/input/InputThread.*`、`src/chart/BmsTimeline.*`
- 修复方向（摘要）：在 AudioThread 回调中消费 Timeline + InputQueue，并更新判定/keysound/气条的核心循环

### 2) 菜单状态机（Title/SongSelect/Gameplay/Result）未完成（音频复用）
- 要求：菜单状态机 + SongIndexerThread + cache + live audio clock 保持
- 当前状态：菜单状态机 + **Windows D3D11 菜单 UI** + SongIndexerThread + cache 已实现
- 差距：菜单到 gameplay 的 **音频设备复用** 尚未完成
- 严重度：Medium
- 相关文件：`src/app/MenuApp.*`、`src/render/MenuWindow.*`、`src/render/RenderThread.*`
- 修复方向（摘要）：在 Menu → GameSession 切换时连接音频设备保留（handoff）

### 3) 输入事件 → 判定的连接尚未缺失
- 要求：InputThread → SPSCQueue → AudioThread 中消费事件/做判定
- 当前状态：已在 AudioThread 回调中连接输入消费与判定/气条更新
- 差距：无（初始实现完成）
- 严重度：已解决
- 相关文件：`src/input/*`、`src/audio/AudioThread.*`
- 修复方向（摘要）：在音频回调中消费输入队列并应用判定/遮罩/连击规则

### 4) Rate/Hi-Speed 的适用范围尚不完整
- 要求：Rate 影响时间表/判定窗口，Hi-Speed 只影响视觉滚动速度
- 当前状态：SpeedManager 已存在，但与 timeline/schedule/实际判定尚未联动
- 差距：Rate 变化时没有真实的播放/判定时间缩放
- 严重度：High
- 相关文件：`src/game/SpeedManager.*`、`src/chart/BmsTimeline.*`
- 修复方向（摘要）：把 schedule sample time 按 rate 缩放，并通过 scaleJudgeWindow 应用判定窗口

### 5) Key Remap + profile 存储/重复警告 UI
- 要求：保存 keymap.json、防重复、NKRO 测试 UI
- 当前状态：**重映射 UI/保存/重复处理/测试画面都已实现**
- 差距：无
- 严重度：已解决
- 相关文件：`src/app/MenuApp.*`、`src/config/Keymap.*`、`docs/menu.zh-CN.md`
- 修复方向（摘要）：后续补充测试用例

### 6) 结果画面（Result）与 replay 保存
- 要求：结果画面详细统计 + Enter 等待 + replay 保存
- 当前状态：**结果画面/统计显示/Enter 返回 + replay/result JSON 保存都已实现**
- 差距：无（初始实现完成）
- 严重度：已解决
- 相关文件：`src/app/MenuApp.*`、`src/app/GameSession.*`、`src/gameplay/Replay.*`、`src/gameplay/ResultStats.*`
- 修复方向（摘要）：未来为 replay 回放/验证增加 loader

### 7) 谱面输入范围
- 要求：仅加载和索引 BMS family
- 当前状态：只支持 `.bms/.bme/.bml/.pms`，osu loader 与导入路径已移除
- 差距：无
- 严重度：已解决
- 相关文件：`src/chart/BmsParser.*`、`src/app/ChartLoader.*`、`src/app/SongIndex.*`
- 修复方向（摘要）：保留 BMS parser、timeline 和真实谱面回归测试

### 8) 判定规则（窗口/遮罩/LN 处理）未实现
- 要求：PG/GR/GD/BD/PR 窗口、30ms lane mask、LN 维持/离开规则
- 当前状态：GameplayEngine 中已实现判定/mask/LN 规则
- 差距：无（初始实现完成）
- 严重度：已解决
- 相关文件：`src/gameplay/GameplayEngine.*`
- 修复方向（摘要）：增强测试并校准判定参数

### 9) config.json 读取与 CLI 优先级应用未实现
- 要求：CLI > config.json 优先级，反映 rate/HS/gauge 等
- 当前状态：全局（`config/config.json`）+ profile 设定已加载，并应用 CLI 优先级
- 差距：无（初始实现完成）
- 严重度：已解决
- 相关文件：`src/config/Config.*`、`profiles/default/config.json`、`config/config.json`
- 修复方向（摘要）：在菜单 UI 中强化实时更新反映

### 10) Launcher 功能扩展未完成
- 要求：显示二进制元信息、SDL2/VC++ 指引、按退出码输出日志尾部
- 当前状态：只实现了基础文件夹检查/基础文件生成
- 差距：诊断/指南功能不足
- 严重度：Medium
- 相关文件：`launch_win.bat`、`launch_linux.sh`、`개발메뉴얼(v0.1)/Tenriff 런쳐 개발 지시사항.txt`
- 修复方向（摘要）：扩展 launcher scripts + 加强运行日志摘要

### 11) Linux 输入/音频路径未实现
- 要求：支持 evdev + ALSA（或替代方案）
- 当前状态：只有 Windows 专用的 RawInput/WASAPI
- 差距：缺少 Linux 运行路径
- 严重度：Medium
- 相关文件：`src/input/*`、`src/audio/*`
- 修复方向（摘要）：增加 evdev 输入线程与 ALSA backend

### 12) 性能/延迟指标 HUD 与诊断日志未实现
- 要求：显示/记录 latency histogram、xruns、queue depth 等
- 当前状态：只有文档
- 差距：缺少采样/诊断工具
- 严重度：Medium
- 相关文件：`docs/latency.zh-CN.md`
- 修复方向（摘要）：收集音频回调/输入队列指标并暴露到 HUD

### 13) 单元测试覆盖不足
- 要求：key remap、判定/shift cooldown、Rate 窗口缩放等测试
- 当前状态：只有 parser/normalize/Speed/Gauge 的部分测试
- 差距：缺少核心新功能测试
- 严重度：Medium
- 相关文件：`tests/unit/*`
- 修复方向（摘要）：为新功能增加单元测试

### 14) 文档与代码同步不足
- 要求：README/Docs 反映最新进展
- 当前状态：README 只是摘要，缺少详细差距分析
- 差距：差距与优先级没有被明确文档化
- 严重度：Low
- 相关文件：`README.md`、`docs/*`
- 修复方向（摘要）：更新 README 摘要 + 维护 gap analysis 文档

### 15) Keymap 文件格式扩展（layout/meta）未反映
- 要求：在 keymap.json 中明确 layout/bindings
- 当前状态：已包含 layout 字段
- 差距：无（初始实现完成）
- 严重度：已解决
- 相关文件：`profiles/default/keymap.json`、`src/config/Keymap.*`
- 修复方向（摘要）：补充 key remap UI 与 NKRO 测试

## 需要确认的决策
- UI 实现框架的选择（例如 SDL + ImGui，或自定义渲染）
- Linux 音频 backend 的优先级（ALSA vs JACK）
- 菜单/游戏渲染的基准分辨率与缩放策略
- ✅ replay 格式（基于 sample time 的 JSON，`profiles/<profile>/replays/*.json` + `profiles/<profile>/results/*.json`）
