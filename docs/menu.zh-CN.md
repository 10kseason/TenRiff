# 主菜单低延迟蓝图

主菜单必须遵守与 gameplay 相同的低延迟哲学：音频作为主时钟，输入在后台线程上打时间戳，而渲染只消费 snapshots。这份蓝图记录规则与实现顺序，避免菜单工作重新引入输入延迟。

## 当前实现状态（Windows 菜单 UI）
- `MenuApp` 通过 **InputThread(轮询)** → **SPSC 队列** → **菜单状态机** → **RenderThread(D3D11 窗口渲染)** 这一流程运行
- `SongIndexerThread` 在后台生成曲目索引，并缓存到 `profiles/<name>/.tenriff/song-index/<source-hash>.json`
- 在菜单里调整 audio/graphics/input/mode 设置时，会保存到 profile 配置文件
- `Options -> Profile Setup` 会重新打开当前 profile 的首次设置页面，并立即保存 language/audio/input/graphics/keymap
- 开始游玩时，当前实现会停止菜单线程并单独运行 `GameSession`
- **Windows 菜单 UI 基于 D3D11 + Direct2D/DirectWrite**，会渲染标题/选歌（青色布局）以及其他设置页面（列表 UI）
- Skins 仅支持 native/LR2；选择或拖放 LR2 playskin folder 会复制到当前 profile 的 `skins`
- Song Select 在重新索引时会居中显示 stage / percent / ETA 与 progress bar
- Browse 可选择本地 BMS 难度表 header JSON，并重新索引当前 source，让 hash 匹配谱面获得表等级
- 选择 ONNX model 只保存路径；用户需明确开启 `BGA Upscaler` 并确认高配置警告，实验性 `优先 NPU` 也不能强制 Windows/driver 最终选择的实际设备
- 输入键摘要：
  - Title：`↑/↓` 移动，`Enter` 选择（PLAY/EDIT/OPTIONS/EXIT），`F2` 浏览 songs 文件夹，`F5` 重索引，`Esc` 退出
  - Song Select：`↑/↓` 移动歌曲，`←/→` 切换左侧菜单焦点，`Enter` 选择/开始，`- / +` 调整 Rate，`Esc` 返回
  - Settings/Mode：`↑/↓` 移动项目，`←/→` 改变数值，`Enter/Esc` 返回
  - Keymap：`↑/↓` 选择，`Enter` 捕获绑定，`Esc` 返回
  - Result：只能用 `Enter` 返回 Song Select
  - 共享功能键：`F1` 帮助，`F2` songs-folder browse，`F5` refresh/reindex，`F9` screenshot

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
- Browse 可选择或清除本地 BMS 难度表 header JSON。路径保存在 `ui.difficulty_table_path`；更改后会重新索引，让 MD5/SHA-256 匹配谱面获得表等级。
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
