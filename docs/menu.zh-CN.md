# 菜单结构与操作

当前说明以客户端 1.7.1 为准。下方折叠的早期设计方案不是已实现功能清单。

## 当前页面与操作

- 主页：`Play / Multiplayer / Options / Exit`；未索引谱面时第一项为 `Add Songs Folder`。
- 选曲：顶部 `Songs / Sources / Records / Session Mix / Options`，中央底部 `Search / Sort·Filter / Difficulty Table`。旧版左侧 KEY/菜单栏不再是当前默认布局。
- 难度表卡片：名称与 URL 区域打开编辑器，File 选择本地 JSON，Reset 恢复原生 LV。Enter 应用，Esc 取消；无效地址保留当前难度表。Filters 中的项目使用相同导入路径。
- Options：5列×2行共10张卡片：Key Mode、Keymap、Skins、Graphics、Audio、Input、Calibration、Profile Setup、Mods、Key Test。
- 共用设置：方向键选择、调整；Enter 执行所选操作，Esc/Backspace 返回。长说明使用分页按钮，皮肤设置保留实时预览。
- 键位：支持 4K–10K、12K、14K、16K，成功绑定后立即保存。
- 结果：Space 跳过单人展示动画。可操作后 R/Left 重试、F1 回放、Enter/Esc/Backspace 返回。Session Mix 中 Enter 进入下一曲，Esc/Backspace 结束；多人结果返回大厅。
- F1/F2/F5 等功能键因页面而异，见[游玩指南](gameplay-guide.zh-CN.md)。

## 职责与数据流

- [MenuApp](../src/app/MenuApp.cpp) 处理 InputThread 的 RawInput/轮询键盘事件和窗口指针事件。[MenuNavigator](../src/app/menu/MenuNavigator.h) 管理当前页面及返回历史。
- 类型化设置 controller 管理选择与修改状态，MenuApp 执行保存、文件选择、重新索引和设备重启。[MenuScreenDescriptor](../src/app/menu/MenuScreenDescriptor.h) 定义页面元数据与 view 路由。
- 渲染读取不可变 snapshot。菜单音乐由 [MenuMusicController](../src/app/MenuMusicController.cpp) 管理，选曲试听状态由 [SongSelectScreen](../src/app/SongSelectScreen.h) 管理。
- [launch_gameplay](../src/app/MenuAppTail.inl) 停止菜单输入和试听后启动 GameSession。游戏内音频与输入生命周期由 GameSession 管理；从菜单起一直共用同一音频设备属于早期提案，不是当前契约。
- SongIndexerThread 的缓存位于 `profiles/<name>/.tenriff/song-index/<source-hash>.json`，见[当前状态](current-state.zh-CN.md)与[配置](config.zh-CN.md)。

## 维护

[菜单重构](menu-refactor-plan.md)的 Phase 0–6 已完成。扩展现有 controller 与显式页面路由，并使用相关测试与 [UI 检查表](ui-audit-checklist.md)。渲染职责见[菜单显示](menu-visual-polish.md)和 [1.7.1 后续变更](gameplay-polish-followup.md)。

<details>
<summary>早期设计方案 — 非当前实现契约</summary>

## 不可妥协的规则
- **保持菜单中的音频设备处于打开状态。** 在进入菜单时初始化音频 backend，并运行静音回调（零缓冲），这样在 gameplay 开始之前 `playhead_samples` / `buffer_start_samples` 仍然有效。开始歌曲时不要重新打开设备，以避免 warm-up 抖动。
- **菜单输入只能使用 InputThread + SPSC。** UI 动作必须来自同一条 RawInput/evdev 摄取路径。不要让 render/UI event loop 直接给输入打时间戳。
- **音频线程必须保持无分配/无 I/O/无锁。** 不要在菜单预览的音频回调里引入文件 I/O、堆分配或锁。
- **Render 只读。** 它只消费 snapshots，不能修改权威时序，也不能给输入打时间戳。
- **重工作必须卸载到后台。** 文件夹扫描、元数据解析、replay/result 保存都必须跑在后台任务上，UI 线程不能阻塞。

## 状态机骨架
状态只负责渲染 UI，并消费已经打过时间戳的输入事件；重型工作交给后台任务。
- `TitleState`
- `SongSelectState`
- `GameplayState`（谱面播放）
- `ResultState`
- 后续：`SettingsState`、`KeymapState`、`LatencyToolsState`

### 流程
`Title → SongSelect → Gameplay → Result` 是最小可游玩的闭环。每次切换都应复用 live audio clock，并保持 InputThread 持续运行。

## 无卡顿的 Song Select
- **SongIndexerThread** 扫描 BMS-family 文件，提取 path/title/artist/BPM/key count/mode/preview audio。stage / percent / ETA 与 progress bar 会在 Song Select header 下方居中显示，交互保持响应。
- **缓存索引**（`song_index.json` 或 SQLite）通过 mtime/hash 检查避免全量重扫。首次运行可能较慢；后续应该接近瞬时。
- **预览音频** 通过音频引擎调度：UI 只负责 enqueue preview request，AudioThread 负责混音，确保时序一致。
- Empty-state 页面应提供持续可见的 `Add Songs Folder` 动作；外部文件夹和 BMS 文件也支持 drag-and-drop。
- 在 Browse > Difficulty Table 中，把 http(s) BMSTable 页面/header 链接复制到剪贴板后按 `Enter`，即可导入 profile cache。`Right` 选择本地 header JSON，`Left` 清除；更改后会重新应用 MD5/SHA-256 匹配等级。
- Song Select 的 `-` / `+` 会在未输入搜索文字时立即更改并保存 `speed.rate`。

## 设置：以延迟优先为中心
把这些放在第一页，让用户第一时间看到与延迟相关的开关：
- 音频 backend（wasapi/asio、alsa/jack）
- 采样率（推荐 48 kHz）
- buffer size（128/192/256），可选自适应提升
- RawInput/evdev grab 开关（关闭时给出警告）
- VSYNC 关闭 / 驱动 frame-queue 指引
- `input_offset_ms` 与独立的 `visual_offset_ms`
- HUD 开关（latency overlay/xrun/late counter）

## Key remap 与 NKRO 测试
- 通过 InputThread 的 **下一条输入事件** 捕获绑定按键；不要通过轮询 render loop 来阻塞等待。
- 为每个按键保持 UP/DOWN 状态机，这样 DOWN 状态下重复 DOWN 和 UP 状态下重复 UP 会被丢弃；保留真实 down→up→down 转换，避免快速点击或 release 被吞掉。
- 成功捕获后应立即保存，不再有单独的隐藏保存组合键。
- NKRO 测试仍保留为可见工具页面，但不再是隐藏快捷键。
- NKRO 测试应显示当前按下集合，并用相同的输入事件实时高亮 ghosting / missing keys。

## 进入 gameplay 时不要产生延迟尖峰
1) **预加载阶段（在菜单里）：** 把谱面加载/归一化到 sample positions；预解码/预加载 keysounds。
2) **Warm start（进入时）：** 在音频已经运行的情况下，把 `song_start_samples` 安排到比 `buffer_start_samples` 未来几个 buffer 的位置。
3) **开始：** 到达那个 sample time 时，render/judgement/keysound 路径开始挂接，这样第一颗 note 会感觉完全锁定。

## Result screen 的卫生标准
- 结果应立即显示；replay/log 保存要作为后台任务，并显示 “Saving…” 指示。
- Replay 记录 `{lane, state, sample}`，这样可以确定性地复现延迟 bug。

## 推荐实现顺序
1) 选择 UI framework（例如 SDL + ImGui，或自定义方案），确保输入/时序仍受现有管线控制。
2) 实现状态机和四个页面（`Title/SongSelect/Gameplay/Result`），让导航可以端到端工作。
3) 加上 SongIndexerThread + 缓存索引 + 响应式 SongSelect UI。
4) 暴露以延迟优先为核心的设置，并在可能的情况下实时应用；对必须重启的 backend 改动做明确标注。
5) 按输入管线规则补上 key remap + NKRO 测试。

</details>
