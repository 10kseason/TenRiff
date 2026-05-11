# 低延迟实现计划

TenRiff 以最小输入到判定到声音的延迟（端到端 <20 ms）为优先目标。这份笔记汇总了来自四份开发手册的后续项，用来在保持“Raw Input → SPSC queue → Audio thread 判定”哲学不变的前提下收紧整个管线。

## 输入管线与时间戳
- 在加载时把 BMS/osu! 时间线标准化为 **sample positions (int64)**，让判定、keysound 和 mixer 都运行在同一条确定性时钟上。
- **按 profile 的输入偏移**：在 `profiles/<name>/config.json` 中加入 `input_offset_ms`（±10 ms 微调），用于抵消设备/驱动延迟。
- **游戏内 loopback 校准器**：提供自动模式，在按键时发出短 beep，通过麦克风 loopback 测量返回峰值，并给出建议的 `input_offset_ms`。
- **HUD 判定窗口**：显示当前 PG/GR/GD/BD 窗口（按 `window_base / rate` 缩放），方便玩家把主观延迟感和更快速度下更紧的窗口联系起来。
- **原始事件卫生处理**：
  - Windows：注册 `WM_INPUT` 时使用 `RIDEV_EXINPUTSINK | RIDEV_NOLEGACY`，通过 `GetRawInputData` 取数据，并用 64 位数学缓存 `QueryPerformanceFrequency`
  - Linux：允许切换 `EVIOCGRAB` 以独占设备，同时在 UI 中提示兼容性取舍
- **噪声/去抖过滤**：丢弃同状态重复边沿，但保留真实 Press/Release 转换，避免快速点击或 release 让按键卡在按下状态
- **多设备输入**：让键盘和手柄各自走独立线程（RawInput/DirectInput/XInput 或带 `poll`/`epoll` 的 evdev），再把事件合并进共享 SPSC queue，并统一时间戳
- **状态型输入跟踪**：为每个键维护 UP/DOWN 状态机，把 DOWN 状态下的重复 DOWN 和 UP 状态下的重复 UP 丢掉，但不吞掉有效的 down→up→down 节奏输入

## 音频与线程
- **替代音频 backend**：在 Windows 上提供 `--audio-backend=wasapi|asio`，在 Linux 上提供 `--audio-backend=alsa|jack`，以便专业接口能跑到更低 buffer。
- **自适应 buffer**：从 48 kHz / 128 frames ×3 periods 起步；如果出现 xruns，就自动提升到 192，再到 256，并记录变化。
- **CPU affinity 可见性**：在设置中显示当前线程到核心的映射，方便用户把 AudioThread 固定在 P-core 并与 render 分离。
- **隔离媒体键**：把媒体键 scancode 放到轻量 side thread（SDL/OS API）处理，避免它们卡住主 RawInput 路径。
- **播放缓冲位置**：把音频回调视为运行在 **playback buffer sample domain**。从设备时钟读取 `playhead_samples`，并计算 `buffer_start_samples = playhead_samples + padding`（例如 WASAPI 的 `GetCurrentPadding`）。所有混音都以 `buffer_start_samples` / `buffer_end_samples` 为参照，这样 keysound 才会在 buffer 真正播放时对齐。
- **音频线程中的输入消费规则**：在回调里弹出输入事件时，用 ClockSync 转成 `press_sample`，然后分支处理：
  - `press_sample < buffer_start`：late input - 仍然应用判定，但把 keysound 钉在 `buffer_start`，让它尽快播放
  - `buffer_start ≤ press_sample < buffer_end`：normal - 按计算出的偏移混入
  - `press_sample ≥ buffer_end`：future - 保持排队，或放进音频线程 staging buffer
- **ClockSync 鲁棒性**：保留线性回归，但加上离群值剔除（MAD/Huber）、带 EMA 更新 slope/intercept 的滑动窗口、设备重置/underrun/backend 切换时的 reset hook，以及 monotonic clamp，确保转换后的音频时间不会倒退
- **以 sample 为单位的判定**：把 PG/GR/GD/BD 窗口转换成 sample（`round(window_ms * sample_rate / 1000.0)`），全程都在 sample domain 比较。如果允许可变播放速度，就把 chart event samples 按 rate 缩放，或者把 rate 固定到每首歌，以保持窗口稳定
- **keysound 确定性**：在 gameplay 前预解码/预加载所有 keysound，禁止 AudioThread 内出现分配/文件 I/O/锁，并保持 mixer buffer 采用适合 SoA/SIMD 的布局，在 callback 中零分配
- **Audio 内禁止分配**：`No allocations, no file I/O, no locks inside AudioThread.` 是硬规则；任何违反都应视为 bug
- **sample-domain 经验法则**：判定与 keysound 调度都以 playback buffer 的 sample domain 表达，而不是 wall-clock time；音频回调从设备 padding 推导 `buffer_start_samples`，并让所有事件相对它定位

## 渲染与帧 pacing
- **驱动侧预渲染下限**：记录一个“pre-rendered frames = 0/1”的选项（例如 NVIDIA 的 `__GL_MaxFramesAllowed=1`），与 VSYNC OFF 以及不启用 triple buffering 一起说明
- **厂商指引**：列出 NVIDIA “Maximum pre-rendered frames = 1” / “Low Latency Mode = Ultra”、AMD Anti-Lag+ 替代方案、Intel frame queue 切换，并在游戏内提供匹配的 “Frame queue mode” 设置，方便用户找到并关闭驱动侧预渲染
- **菜单时序一致性**：菜单/结果也要使用同一个 monotonic clock 和 input→audio 分离；不要让 render 代码在 gameplay 之外直接给输入打时间戳。菜单中也保持音频 backend 打开并使用静音回调，这样在 gameplay 开始前 `playhead_samples` / `buffer_start_samples` 仍然有效
- **Render 只读**：渲染只消费不可变的 gameplay snapshots；绝不能给输入打时间戳，也不能修改权威的音频/判定状态

## 面向延迟感知的 UI/UX
- **Latency HUD**：一个小型 overlay，显示 input→audio 延迟直方图、queue depth、xruns，并在 99.9 percentile >10 ms 时用红色高亮
- **音频回调预算**：显示每个 tick 的 `callback_time_ms / buffer_length_ms`，用于尽早发现 overrun；当处理超过 buffer 预算时明确提示
- **晚到输入计数器**：跟踪 `press_sample < buffer_start` 的输入百分比/数量，用于发现主机/OS 调度或设备问题
- **Lag toast**：如果测得的端到端延迟超过阈值（例如 25 ms），给出 toast 和缓解建议（ASIO/JACK、RawInput/evdev grab、VSYNC OFF）
- **输入 backend 切换**：设置中的 RawInput/evdev grab 复选框默认开启，并说明关闭会增加延迟，但可能帮助兼容性
- **NKRO 指引**：在 10-key NKRO 测试 UI 中提醒用户一次按很多键以验证真正的 NKRO，并在出现 ghosting 时建议使用机械键盘

## QA 与工具
- **性能日志导出**：可选 CSV 记录延迟、buffer 深度、xruns 与 core 使用率，便于离线分析
- **环境预检**：启动器或启动脚本应检测电源计划、CPU boost 与 USB polling rate，并在不理想时建议开启高性能设置
- **权限指引**：当 RT scheduling 或 evdev grab 因权限失败时，显示修复步骤（管理员重跑、`/etc/security/limits.conf` 提示）
- **内置 burn-in**：加入 `--burnin`，自动播放谱面数小时，同时监控输入队列、xruns 与延迟，以尽早暴露回归
- **sample-time replays**：把 replay 记录为 `{keycode, down/up, press_sample (int64)}`，这样就能在 sample timeline 上确定性地重放，复现延迟尖峰
