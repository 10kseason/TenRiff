**2026-10-17 · 1.9.5.1:** [判定线、FAST/SLOW 与文件选择](release-1.9.5.1.zh-CN.md). FAST/SLOW从最后一次最大判定范围外的输入起保留0.35秒。最大判定不会立即清除它，也不会延长显示时间；新的范围外判定会立即更新。常显刻度与点/条形标记的0.7秒寿命保持独立。

**2026-10-17 · 1.9.5:** 菜单和结果音乐也使用声音选项中选定的设备。FAST为蓝色，最高判定为绿色，LATE为红色；最高判定也会更新FAST/SLOW。选曲循环为OFF → 镜像 → 随机 → S随机 → 固定搓盘随机 → OFF。 [音乐输出统一与判定指示器](release-1.9.5.zh-CN.md).

**2026-10-17 · 1.9.3 local:** [指示器、输出设备与配置快捷键](input-feedback-20261017.zh-CN.md).

# TenRiff 配置模式（当前）

**1.9.3当前契约：** 使用安全加载默认值、空POOR准确率排除R7/R8、一般ONNX及受支持Safetensors。正式Sites v19接受符合条件的新Online记录，Guest及未标注来源的历史记录永久排除。不附带BGA权重，保持OFF/空路径及1.9.0基准。[1.9.3范围](release-1.9.3.zh-CN.md)优先于下方带日期历史。

**2026-10-15 · 1.9.1:** 用户授权的1.9.1包含此前音量、输入、尾奏修复，BMS TOTAL/RANDOM兼容、结果颜色及10形标志。主页语言按钮按EN → KO → JA → ZH循环并保存选择。新BMS血条/分支记录在服务器验证支持前仅存本地。固定基准仍为1.9.0，保留旧发行版、安装和配置。下方带日期的本地记录中不升版本、未发布等表述保留当时状态；当前以此1.9.1范围为准。 [变更与兼容性](feedback-1.9.1.zh-CN.md) · [发布验证流程](release-1.9.1-gate.md).

**2026-10-15 · BMS TOTAL、RANDOM与结果/标志后续修改 — 未发布、本地构建及CTest回归验证通过：** 本次本地修改基于1.9.0.2。结果页等级改用与选曲大厅相同的红色等级文字及键数符号颜色，标志改为提供的10形图案。新的普通BMS游玩根据#TOTAL及音符数使用LR2式恢复和伤害；谱面内#RANDOM分支使用独立于轨道随机的种子。旧录像保留原有行为，并包含此前的音量统一修改。已发布1.9.0.2、固定基准1.9.0及版本号均不变。 [公式、分支、兼容性与验证范围（英文）](bms-total-random-followup.en.md).

**2026-10-15 · BMS音量统一后续修改 — 未发布、本地构建及CTest回归验证通过：** 此1.9.0.2基础上的本地修改将伴奏、键音独立调节合并为BMS演奏音量（韩文显示：**BMS 연주 볼륨**）。新默认值为20%，保留已保存值。主音量10%、TenRiff音乐100%的默认值、其余音量1个百分点步长及大厅试听×0.75均保留。不更改版本号或固定基准1.9.0。下方带日期的条目保留当时记录。

1.9.1会显示难度表加载状态，并将debounce UI改为1ms步长、最大20ms（默认0ms）。停用旧的5→8ms自动改写，分别保留已保存的5ms和8ms。皮肤的韩文标签改为 `LN 끝 줄임`，外观和判定行为不变。普通单人模式开启听到结束后，最后音符及LN尾端处理完毕时按 `Esc` 会跳过尾奏直接进入结果，不弹出暂停菜单。

**2026-10-15 · 1.9.0.2 发布候选版 — 发布验证流程：**基于已发布的1.9.0.1，将Options改为绿色，仅在选择内置5K Aery难度表时显示绿色⑤，为已注册4/6/8/10键难度表的符号使用对应颜色，并将等级设为红色、分数设为白色，新增`lv:`等级搜索、新建配置debounce默认0ms和音量1%步长。将选曲的排序、过滤和难度表选择器移至Song Library旁的页眉，并增加专辑封面高度。主音量默认10%，固定基准仍为1.9.0。[变更与用法](feedback-1.9.0.2.zh-CN.md)。 公开状态与可下载文件以发布页面为准。[范围与验证](release-1.9.0.2-gate.md)。

**2026-10-15 · 1.9.0.1:** 包含默认主音量10%和大厅图标中心对齐。提供试听×0.75、个人档案指定F5/F6 -10/+10、Xbox/XInput按钮输入、文件选择框防重复，以及BMS BPM/STOP顺序修正与旧回放兼容。固定基准保持1.9.0。 [变更说明](feedback-20261015.zh-CN.md) · [发布验证范围](release-1.9.0.1-gate.md).

**1.8.8：**包含结果分析、自由调整窗口大小与直接输入宽高、结果页的 `F9` PNG 保存提示、长 LN Mix，以及菜单、键音和开始操作改进。当前验证与交付状态见[发布说明（韩语）](release-1.8.8-gate.md)。用户固定基准仍为 1.8.7，保留其现有标签和发布文件。

依据：[Config.h](../src/config/Config.h)、[Config.cpp](../src/config/Config.cpp)、[默认 JSON](../config/config.json) 与[键位](../src/config/Keymap.cpp)。省略字段使用代码默认值；以下范围描述加载、保存时的规范化规则。

这份文档整理当前 `config/config.json`、`profiles/<name>/config.json`、`profiles/<name>/keymap.json` 的实际配置结构。

## 加载顺序
1. 代码默认值
2. 全局配置：`config/config.json`
3. profile 配置：`profiles/<name>/config.json`
4. CLI
5. 菜单/运行时保存

如果 profile 不存在，首次运行时会自动创建。

## `config.json`

### `audio`

**此前1.9.0.2说明（音量统一前的历史记录）：**

1.9.0.2候选版的音频UI将主音量、BMS曲目、TenRiff音乐、BGM和键音这5项统一为 **1个百分点步长（保存值0.01）**。前3项范围为0–100%，BGM和键音为0–200%。新建配置主音量仍默认10%，其余默认100%；已保存的音量保持不变。

**1.9.1音频：** 音频UI只保留主音量、BMS演奏音量、TenRiff音乐3项，范围均为0–100%，以 **1个百分点（保存值0.01）** 调整。新默认值依次为10%、20%、100%，明确保存的值保持不变。沿用 `gameplay_volume` 作为唯一的BMS专用音量倍率，主音量倍率继续生效。`bgm_volume` 与 `keysound_volume` 为兼容保留读写，但移除独立UI，并在游玩、所有试听和编辑器路径中忽略这些音量倍率。大厅试听仍额外衰减×0.75。实际听音、物理设备及长时间游玩尚未验证。

- `play_to_end` (bool; default `true`): 最后音符和LN尾端处理完毕后继续播放剩余音乐。普通单人模式在此尾奏阶段按 `Esc` 可直接进入正常结果，不打开暂停菜单（包括自动游玩和回放）。保留Session Mix、多人模式原有Esc规则。false时仅等待 `ui.result_tail_ms` 后进入结果，判定、分数及回放规则不变。

- `backend` (string)
  - `wasapi | asio`，默认为 `wasapi`。游戏、试听、菜单和结果音乐均使用选定WASAPI设备或ASIO驱动。
- `device_id` (string)
  - 默认为空（系统默认）。声音选项为游戏、BMS试听、菜单和结果音乐选择WASAPI设备。菜单使用共享模式，游戏保留独占设置。指定设备缺失时不会自动切换。短促的设置调整点击音仍使用Windows默认输出。
- `asio_driver` (string)
  - 已安装64位 ASIO 驱动的 CLSID。空值表示 Auto，使用按名称排列的第一个驱动。Audio 设置中选择，F5 刷新列表。
- `rate` (int)
  - 默认采样率
- `frames` (int)
  - 缓冲帧数
- `periods` (int)
  - period 数量
- `exclusive` (bool)
  - 是否尝试 WASAPI exclusive
- `use_mmcss` (bool)
- `affinity` (int)
  - `-1` 表示使用默认值
- `preset` (string)
  - `basic | high`
- `bms_keysound_policy` (string)
  - `follow | autoplay | ignore`
- `background_sound_enabled` (bool)
  - 控制菜单、结算和歌曲预览音乐；不关闭游戏中的谱面 BGM。
- `title_music` (string)
  - `none | default | random_bms | last_played`，默认 `default`。为标题和快速设置选择无音乐、默认音乐、随机 BMS 或最后游玩的歌曲。无法读取歌曲音频时回退到默认音乐。BMS 包含键音，异步合成最多 5 分钟并循环播放。
- `volume` (double)
  - 主音量。0–1，新默认值0.30（30%）；保留明确保存的值。
- `gameplay_volume` (double)
  - BMS演奏音量（韩文显示：BMS 연주 볼륨），共同调节伴奏、键音和试听。0–1，新默认值0.20（20%）；保留明确保存的值。字段名与UI音效的原有倍率不变。
- `menu_music_volume` (double)
  - 仅用于TenRiff菜单和结果音乐。0–1，默认1.0。基准为原音的10%，比1.8.9.7的40%基准再降低75%。独立于BMS曲目，不模拟Windows音量曲线。
- `bgm_volume` (double)
  - 兼容用旧字段。0–2，默认1.0；保留已有值的读写，但不再提供独立UI，并在游玩、试听和编辑器音量计算中忽略。
- `normalize_audio` (bool)
  - 游戏内RMS音量调整，默认false。ON保留RMS→软限幅→主音量；OFF仅应用线性主音量与最终输出范围限制。菜单音乐及选曲试听不变。
- `keysound_volume` (double)
  - 兼容用旧字段。0–2，默认1.0；保留已有值的读写，但不再提供独立UI，并在游玩、试听和编辑器音量计算中忽略。

参见 [ASIO 设置](asio-audio.md)。ASIO 固定所选采样率并对谱面音频重采样。`frames` 为请求值，按驱动支持的大小协商。预设不覆盖 ASIO frames；`exclusive` 与 `periods` 仅用于 WASAPI。ASIO 失败时不会自动切换 WASAPI。

### `input`

1.8.9.7仅为输入线程请求MMCSS `Games`/High并关闭执行速度电源节流。API失败记录到启动日志并保留原输入路径，退出时由原线程恢复电源状态及解除MMCSS，RawInput转Polling也保留作用域。不修改全局电源计划、注册表或音频优先级。物理输入延迟是否改善仍需实际游玩对比。

- `backend` (string)
  - `polling | rawinput`
  - 当前 `1.7.2` 发布线默认值为 `rawinput`
  - 可在 `Options -> Input Settings -> Backend` 或 `Options -> Profile Setup -> Input Backend` 中按 profile 选择
  - runtime fallback 不会把已保存值改写为 `polling`
  - 确认 RawInput 启动失败、注册目标丢失或 message window 退出后，本次应用运行期间 menu 与后续 gameplay 都会保持 Polling
  - 重启应用或在 Input Settings 中明确更改 Backend 后，会重试所选 backend
- `rawinput` (bool)
  - 与 `backend` 一起保存的辅助布尔字段
  - 为 `true` 时，menu/gameplay 优先使用 RawInput
  - gameplay 会在同一 `InputThread` 中持续用 bound-key polling shadow 监测 note/control key
- `use_qpc` (bool)
- `grab` (bool)
  - 当前主要用于 Linux preview
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - Polling backend 与 gameplay polling shadow 的采样频率
  - 默认值为 `1000` (`1ms`)
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - input config 中保留的兼容字段
  - 当前 runtime 不再用此值驱动独立的 audio-thread judgement sub-step loop
  - 默认值为 `4000` (`0.25ms`)
- `debounce_ms` (double)
  - `0ms`关闭按时间抑制再次按下的功能，保留原有快速连打；仍会移除同状态重复事件。
  - 大于`0ms`时，同一按键、同一物理输入源在Release后未满设定时间到达的下一次Press会被抑制；恰好达到时间边界的Press允许通过。
  - 时间过滤器不延迟或丢弃真实Release。即使是有意的快速再次按下，也可能因落在设定时间内而被抑制。
  - RawInput/Polling镜像事件不能绕过抑制，由RawInput识别的不同物理键盘独立处理；Polling本身不区分物理设备ID。XInput保留原有独立GamepadTracker路径，不使用此键盘时间过滤器。
  - 限制在 `0..20`，UI以1ms步长调整
  - 默认值为 `0ms`；保留范围内的已保存值
  - 停用的旧规则：按值执行 `5ms → 8ms` 自动改写。移除该规则，避免改写新UI中明确选择的5ms；已保存的5ms和8ms分别保持原值

### `judge`

1.9.0发布版在 `Options > Key Mode Settings > Judge Window` 中提供默认/Hard/EX。Easy从新选择中移除，但保留旧回放的判定。选择仅在本次运行有效，重启后恢复默认。[EX判定与兼容范围](judge-ex-1.9.0.ko.md)（韩语）。

[最新判定范围、RANK表与LN规则（韩语）](judgement-windows.md)

- BMS `#RANK`：`3/EASY` PG21ms、`2/NORMAL` PG18ms、`1/HARD` PG15ms、`0/VERYHARD` PG8ms。EASY的GR/GD/BAD保持65/115/210ms，其他难度分别缩小至18/21、15/21、8/21倍。缺失或不支持的值使用EASY。Judge Hard/EX模组随后应用；RANK不改变 `hold_grace`/`hold_break` 设置与自动漏键时限；尾端PG/GR/GD判定会应用RANK。
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 默认 `pg / gr / gd` 分别为 `21ms / 65ms / 115ms`
- 默认 `bd` 为 `210ms`
- `Judge Easy` (仅用于旧回放) 将RANK之后的PG/GR/GD和长按容差扩大为 `1.35x`。EASY的PG/GR/GD=`28.35/87.75/155.25ms`；BAD在所有RANK固定为 `210ms`，mask不变
- `Judge Hard` 的EASY基准PG/GR/GD=`17.5/55.714286/98.571429ms`。RANK之后PG乘 `17.5/21`，GR/GD乘 `18/21`；BAD在所有RANK固定为 `225ms`，长按容差不变
- `Judge EX` 的EASY基准PG/GR/GD=`14/44.571429/78.857143ms`（Hard的80%），保留RANK比例。BAD对所有RANK固定为250ms，独立的自动漏键时限仍为340ms，分数倍率与Hard相同为1.10。
- `indirect_miss` (double, ms)
  - 当前配置将此值保存并归一化为 `340ms`，自动漏键判定时限独立于BAD命中窗口
  - 默认Normal/Easy/Hard在未输入音符超过 `340ms` 后自动判漏键；Normal/Easy记BAD，Hard记断连的间接 `POOR` / OD8 `MISS`
  - BAD窗口外、自动判漏键前的迟到输入不会命中BAD，而是先将过期音符记为漏键，再检查下一音符
- 新普通/Hard使用`tenriff-native-score-v2-ruleset-7`，EX使用`tenriff-native-score-v2-ruleset-8`。仅排除空POOR的准确率贡献，不改数值判定窗口。历史R1–R6/custom保留原规则，新custom路径使用对应7/8 ID。
- R5保持原判定时间范围，按时间顺序处理自动事件，并保留R1–R4的原处理顺序。[修正范围](release-1.8.9.5-gate.md)。
- `hold_grace` (double, ms；默认 `80ms`)
  - 为配置兼容性及 `hold_break` 下限保留；不是当前native尾端PG/GR的判定边界
- `hold_break` (double, ms；默认 `200ms`)
  - CN尾端仍未松键时，从尾端时刻到自动BAD的等待时间。Judge Easy下为 `270ms`，不受RANK影响
  - 内部始终保持不低于 `hold_grace`
- LN松键立即按当前PG/GR/GD窗口判定，超出GD即为BAD。普通LN按住至尾端可自动完成。`No LN Release` 也不会忽略中途松键
- `mask` (double, ms；默认 `30ms`)
  - 暂时忽略被屏蔽轨道上的额外按下；不受RANK或Judge模组影响

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

- 基准 BPM 为累计进行时间最长的速度；相同 BPM 的区间合并计算，排除 STOP 等待。Hi-Speed 以此固定，保留显式 `#SCROLL`、停止和逆向效果。参见[计算规则](reference-bpm.md)。

### `gauge`

**1.9.1 BMS行为：**新的普通BMS游玩中，Normal/Easy恢复采用 `TOTAL / N`，Hard/EX采用固定恢复及TOTAL、音符数伤害修正。`N`为有效音符起点数，每条LN只算一个。Hard血量低于32%时判定伤害乘0.6。保留TenRiff的100%初始血量、0%失败、血条切换、判定分类、LN头尾各0.5权重及地雷伤害。组曲保留原组曲血条，对战保留v1。现有 `gauge.delta` 保留用于旧录像，不用于新的BMS v2计算。

Gauge Shift 始终启用。`mode.gauge` 的 `ex_hard / hard / normal / easy` 选择从 EX 到 Easy 的起始档位。所选档位及以下档位均从 100% 独立并行计算，当前档位淘汰后转至下一存活档位。全部可用档位淘汰后才发生血条失败。旧 `shift` 值表示从 EX 开始。

段位/Session Mix 使用跨曲继承的 LR2 参考血条，保留间接失误，HP 低于 2% 时失败。普通 LN 在完成/释放时只结算一次，不应用普通 `gauge.delta`。参见[比较与兼容范围](lr2-gauge-audit.ko.md)。

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - 默认值为 `borderless`；Discord、OBS、Game Bar 等外部 overlay 也推荐使用此模式
  - `windowed` 带标题栏，可移动并拖动边框调整大小。拖动结束时把实际client尺寸保存到配置；最大化、还原及程序调整大小不会覆盖已选值
  - `fullscreen` 是 DXGI 独占全屏，当前 Discord Game Overlay 不会显示
- `resolution` (string)
  - `native`、旧别名 `720p | 1080p | qhd`，或 `宽x高`（例如 `1600x900`, `1366x768`, `1280x800`, `3440x1440`）。每轴支持320–8192px。
  - 图形设置包含常见尺寸和当前显示器的显示模式。按`F5`刷新列表。自定义尺寸在保存和重启后保留。
  - 分辨率`+/-`继续循环预设；按`Enter`或点击数值进入宽/高编辑，用键盘上排数字或数字小键盘输入。`Tab`/方向键切换，`Enter`应用、`Esc`取消、`Backspace`删除、`Delete`清空。每轴必须为320–8192整数；空值或越界值显示错误并保留原设置。不使用其他字符、粘贴或IME输入。
  - 内容和点击映射保持1920×1080比例，其他窗口比例显示留边。请求尺寸过大时保留选择值，只将实际窗口缩小到工作区域内；通常等比缩小，但实际client每轴至少320px，因此极端窗口比例可能改变。
  - 独占全屏不支持请求尺寸时回退到窗口模式并保存实际尺寸/模式。保留旧配置标记及默认值；新候选版的实窗、切换和保存验证见[追加发布条件（韩语）](release-conditions-1.8.8.ko.md#창-크기와-직접-해상도-입력)。
  - 皮肤设置使用实际游戏渲染器的等比缩小画面，反映区域移动、轨道/音符/面板尺寸、判定/连击位置和皮肤字体。
- `vsync` (bool)
- `refresh_hz` (int)
  - `-1` 表示 `Match Display`；`0` 是兼容旧 profile 的 `Unlimited` 选项，实际最高 1500 FPS
  - 默认值为 `-1`
  - 只有在 `vsync=false` 时才充当直接 FPS 上限
  - `vsync=false` 时，menu 的有效上限为 `300`；gameplay 在 `-1` 时跟随显示器 Hz，在 `0` 时应用 1500 FPS render pacing
  - `vsync=true` 时，present refresh 以当前活动显示器 Hz 为准，render pacing 的目标是 `monitor_hz * 2`（上限 `1050`）
- `performance_overlay` (bool)
  - 默认值为 `false`；它位于右上角，可能会与放在同一角落的 Discord Voice widget 重叠
  - gameplay frame pacing 测量成功 DXGI `Present()` 完成时间之间的间隔，不再把 HUD 更新节奏作为 FPS sample
- `bga_enabled` (bool)
  - 默认值为 `true`；设为 `false` 会关闭 gameplay 图片/视频 BGA 及其 decoder/upscaler 工作
  - Song Select 背景预览属于独立功能，因此仍会显示
- `background_upscale_mode` (string)
  - `onnx | off`，旧`lunasr`兼容为`onnx`，默认`off`。启用需确认高配置提示，不自动运行性能测试
  - 原图 → FFmpeg RGB去块 → 模型原生x1–x4推理 → 残差合成/完整图像 → 显示适配/裁剪，不预先缩至960×540
  - 动态模型处理整张原图，固定模型使用填充/上下文分块。输入/输出单轴上限4096/8192，每实例缓存3张/96MiB，视频AI禁用
- `background_upscale_model_path` (string)
  - 默认空，保留明确保存路径。选择`ONNX / Safetensors`或拖入`.onnx`/`.safetensors`；输出重置Auto，但不自动启用
  - 运行/源码包不附BGA权重。接受绝对路径或相对执行文件/工作目录的路径
  - 单个RGB NCHW FP32/FP16输入输出、任意名称、batch1/channels3、双空间轴固定或动态、统一整数x1–x4。输入0..1，支持浮点边界内部INT8 QDQ；拒绝部分动态、NHWC及多个输入输出
  - Safetensors限支持的LunaSR2或已安装Spandrel中可导出架构，需要外部Python+torch/safetensors/onnx/onnxruntime，不自动安装/下载。失败保留原图并记录原因
  - [契约、设置及限制](../tools/onnx_upscaler/README.md)。模型尺寸、算子、内存及质量限制仍存在
- `background_upscale_output_mode` (string)
  - `auto | residual | image`，默认`auto`。手动选择优先；换模型重置Auto并替换会话/缓存
  - Auto优先`tenriff.output_mode`，其次旧`rgb_lr`/`rgb_residual_x2`组合为残差，其他为完整图像。不会推断语义，未标记残差请手动选择
  - residual将输出加到去块原图的bilinear放大结果，image直接使用输出RGB；均限制0..1并保留原alpha
- `background_upscale_prefer_npu` (bool)
  - 默认值为 `false`，默认路径会请求高性能 DirectX GPU
  - Graphics Settings 的实验性 `Low-Power DirectX` 会请求 `DirectXMinPower`
  - legacy WinML 路径既不能明确选择也不能验证 NPU，因此该选项不能作为 NPU 执行证据
  - 创建低功耗 session 失败时回退到现有高性能 DirectX 路径

### `mode`
chart loader/indexer 仅支持 BMS family（`.bms/.bme/.bml/.pms`）。旧 `enable_osu_charts` 与 `format` 值即使被读取也会忽略，且不会再次保存。

- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
  - `none` 表示直接沿用谱面的原始键数
- `key_conversion_algorithm` (string)
  - `krrcream | nk2 | nk3`
  - 在游戏内 `Mode Settings > Key Converter` 中选择 `Krrcream`、`KeyWeaver nK2` 或 `KeyWeaver NK3 ONNX`
  - 默认值为 `krrcream`；NK3 在键数不变时也会 remaster，默认 `AUTO` 后端优先使用 ncnn Vulkan
  - Krrcream 只把原始 note 重排到目标 lane
  - nK2 在扩展键数时不会先向原始 pattern 加 note，而是在转换过程中直接向目标 layout 生成安全的辅助 note
  - NK3 始终使用 P64 与 host beam safety solver，仅在非 10K 源谱面转换为 10K 时加入 generalized MLP；通过 `TENRIFF_NK3_BACKEND=AUTO|VULKAN|NCNN_CPU|OPENVINO` 选择后端，`AUTO` 会优先尝试 ncnn Vulkan；多块 Vulkan GPU 可用 `TENRIFF_NK3_VULKAN_DEVICE=<index>` 选择
- `key_conversion_nk2_preset` (string)
  - `native | transform | remaster`；默认值为 `native`
  - 选择 nK2 的 `Native (12%)`、`Transform (35%)` 或 `Remaster (65%)`；Krrcream 下锁定该设置行
  - `Remaster` 在提高预算的同时保留原曲排布，并用等长长条填充 LN 区间的辅助 note
  - 三者均为上限，实际增加量会因原谱密度与安全窗口而更低
- `gauge` (string)
  - `normal | hard | ex_hard | easy | shift`
  - 默认`ex_hard`（EX）。保留已保存的起始档位。
- `random` (string)
  - `off | mirror | rr | frns | sr` (`fr` = `frns`)
- `random_seed` (int)
  - RR/SR、强制 key-mode 变换和 LN Mix 目标选择使用固定 seed；普通 Random 每次游玩生成新的 session seed，并把实际值写入 replay
- `mods` (string array)
  - Note Structure 可在 `full_long_notes`、`ln_mix_10` 到 `ln_mix_90`、`full_short_notes` 中选择一个
  - 新LN Mix v2以基础BPM×Rate为准，目标为长2/4拍60%、中1拍20%、短1/4或1/2拍20%。保留其他轨道的单键/短LN，不合并同轨道连续单键
  - 候选单键须容纳至少1/2拍，并在同轨道下一个起点前留出50ms；目标长度放不下时按4→2→1→1/2→1/4拍缩短，因此密集谱面的实际长LN比例可能低于目标
  - 保留已有LN/CN、原始起点与键音，排除与同轨道已有区间重叠的起点；相同谱面、`random_seed`、Rate和变换版本会重现相同结果
  - 旧回放缺少变换版本时按v1读取，保留原有8/16/24/32分音符政策。新v2只保留本地记录/回放，不计入旧的谱面级BEST或Sites上传。参见[长度政策、兼容性和验证状态（韩语）](release-conditions-1.8.8.ko.md#ln-mix의-요구-의미)
- `ghost_battle_enabled` (bool)
  - 默认值为 `false`
  - `true` 时会自动加载当前选中谱面的最佳兼容 replay 作为 ghost 对比
  - `false` 时普通游玩保持单场地显示
- `autoplay_enabled` (bool)
  - QA 用非竞争自动游玩模式
  - `true` 时会自动处理可判定的按键输入，并将结果保存为 `AUTOPLAY`
  - 不计入正式 clear、best score、clear lamp 或默认 ghost，但保留本地结果与 replay 历史
- `practice_no_fail_enabled` (bool)
  - QA 用 assist 模式
  - `true` 时会禁止基于 gauge 的提前失败，但仍保留判定与结果导出直到谱面结束
  - 结果会带上 `ASSIST` clear status
- `one_miss_fail_enabled` (bool)
  - `true` 时首次出现 OD8 换算对象 `MISS` 就会把 gauge 归零并立即失败
  - 仅原生 `BAD` timing 不会触发，空键输入产生的 `POOR` 也不会触发该模式
  - 在 Mode Settings 中启用后会自动关闭 `practice_no_fail_enabled`
- `pacemaker_mode` (string)
  - `off | accuracy | score` (default `off`)
  - 实时显示与准确率或分数目标的差值。保留正常血条失败、通关与计分规则，仅目标未达成不会导致通关失败；满足其他条件时仍可登记最佳记录和排名
  - 启用 Pacemaker 会关闭 Practice 与 Sudden Death；replay playback 和 multiplayer 会强制关闭它
- `pacemaker_target_accuracy` (double)
  - 0..100，默认90.0；以百分点显示当前标准Accuracy与目标的差值
- `pacemaker_target_score` (int)
  - 0..10000，默认8000；按已判定音符权重计算目标进度，显示当前分数与目标进度的差值
- `auto_scratch_hide_lanes` (bool)
  - 默认false。mods含auto_scratch时隐藏实际BMS转盘列，保留其他按键宽度和逻辑输入编号
  - `auto_scratch`：只自动处理实际BMS转盘。分数倍率0%，标为ASSIST，不计入正式最佳记录和排名
- `song_index_profile` (string)
  - `safe | fast`
  - 默认`safe`（安全），准备哈希和试听信息并支持难度表。原生LV/CR计算遵循下方的独立设置。
  - 保留已保存的选择。可选的`fast`（最小）跳过哈希、试听和难度计算。
- `calculate_song_index_difficulty` (bool)
  - 默认值为 `false`
  - `false` 保留 BMS `#PLAYLEVEL` 作为菜单 LV，并跳过 CPU 开销较高的原生 LV/CR 计算
  - `true` 仅在完整 `safe` 索引中计算 Revive LV/Circus Rating；`fast` 始终跳过
  - 修改设置后会区分缓存模式，并对当前 song source 执行完整重索引

### `ui`

主页语言按钮按 **EN → KO → JA → ZH → EN** 循环。使用已有的 `ui.language` 保存，重启后保留。缺少翻译的文字可能回退为英语。

`ui.skip_start_countdown` 和 `ui.instant_restart_key` 是本体功能，保存在配置档及 `.trprofile` 导出文件中。跳过的仅是加载后的初始 `3 / 2 / 1` 显示；音频准备、原有谱面开头的3秒空白及判定时间保持不变。从暂停继续时始终保留3秒恢复倒计时。多人游戏强制保留初始倒计时并禁用即时重新开始；Session Mix 也禁用即时重新开始。暂停或初始倒计时期间不会触发，必须松开按键后才能再次触发。轨道键、保留键冲突和编辑器练习起点等细节见[开始与重新开始说明（韩语）](gameplay-start-controls.ko.md)。

这些设置不会改变 Sites 记录提交规则。长 LN Mix v2 不计入旧 BEST 或 Sites 是独立的变换规则；[插件运行环境、API 和 SDK 仍是未来计划（韩语）](result-analysis-1.8.8.ko.md)。

`last_played_chart_path` 保存最近一次开始游玩的谱面本地路径，默认空字符串。回放和编辑器练习不会更新；重启后可供 `title_music=last_played` 使用。

| 字段 | 类型、范围、默认值 | 行为 |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | 显示名称；为空时使用配置 ID，并规范化空白与控制字符。 |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | 本地 PNG/JPG 头像路径。 |
| `language` | `en`, `ko`, `ja`, `zh-CN`; `en` | UI 语言；无效值规范化为 en。 |
| `menu_font_size` | `normal`, `large`, `extra_large`; `normal` | 配置档和首次设置中的菜单字号：100%、115%、130%。游戏内字体继续由皮肤控制。 |
| `skip_start_countdown` | bool; `false` | 跳过单人游玩开始时的 `3 / 2 / 1` 显示；适用于正常开始、菜单重新开始及结果页重试。 |
| `instant_restart_key` | string; `""`（未分配） | 从头重新开始正在进行的单人谱面的键名。新一轮始终跳过初始倒计时；中断的尝试不保存结果或回放。 |
| `hispeed_decrease_key` | string; `"F5"` | Note Speed ×0.5按键。在档案中绑定，空字符串禁用。 |
| `hispeed_increase_key` | string; `"F6"` | Note Speed ×2按键。在档案中绑定，空字符串禁用。 |
| `function_keys` | object; F1–F11 | 配置中指定F1–F11功能及提示按键。空值解除。 |
| `all_song_sources` | bool; `false` | 恢复 ALL SONG，合并已登记文件夹的缓存并去除相同谱面路径的重复项。 |
| `result_tail_ms` | double; `500` ms | 判定完成后结果切换的额外等待时间。`audio.play_to_end=true` 时同时考虑谱面音频结束时刻。 |
| `require_enter_to_exit` | bool; `true` | 为读写兼容保留；当前 Windows 结果输入路径不使用此值自动退出。 |
| `show_cursor_in_gameplay` | bool; `true` | 游戏中显示鼠标指针。 |
| `active_song_source`, `recent_song_sources` | string / string[] | 当前与最近使用的歌曲文件夹。 |
| `song_sources_initialized` | bool; `false` | 明确添加或移除曲目录后为 true，最后一项删除后保留空列表，重启不恢复。[曲库管理](library-management.md)。 |
| `session_mix_lr2_course_path` | string | 所选 LR2 课程文件路径。 |
| `favorite_chart_keys` | string[] | 收藏谱面的内部标识键。 |
| `collections` | object: name → string[] | 按集合名称分组的谱面键列表。 |
| `song_collection_filter` | string; `all` | 全部、收藏或集合筛选；即时保存。 |
| `song_key_filter` | int: `0..16`; `0` | 键数筛选；0 为全部。UI 可选 4K–10K、12K、14K、16K。 |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | 难度边界；0 表示不启用对应边界。选择外部难度表时使用表中显示的数字等级，启用边界时排除非数字标签。原生 LV 继续使用现有内置等级。见[数字及前缀处理（韩语）](release-feedback-1.8.8.ko.md#선택한-난이도표-기준의-최소최대-필터)。 |
| `difficulty_table_path` | string | 本地 header JSON 或下载的缓存；选表切换至 safe 索引。原生 LV 清空路径及有效缓存中的表信息。 |
| `difficulty_table_url` | string | 原始 HTTP(S) BMSTable 页面或 header URL；解析 meta 并缓存 header/data，选择本地 JSON 时清空。 |
| `online_records_server_url` | string | 记录、排名与聊天 API URL；失败不阻止本地游玩或记录。 |
| `tenriff_main_server_url` | string | F10 主服务器 API URL；默认值为 Config.h 中的 `kTenRiffMainApiUrl`。 |
| `private_server_url` | string | F10 私有 API URL；远程需 HTTPS，仅 localhost 可使用 HTTP。 |
| `account_server_mode` | `main`, `private`; `main` | 上次选择的账户服务器。 |

难度表 header 使用 `name`、`symbol` 与本地相对 `data_url`；data 项按 `md5` 或 `sha256` 及 `level` 匹配。远程导入保存在配置的 `difficulty_tables` 缓存中。

原生 LV 清空 `difficulty_table_path` 与 `difficulty_table_url`，有有效缓存时仅移除外部表信息，无须完整重扫。默认使用 BMS `#PLAYLEVEL`；开启难度计算的缓存使用原有计算 LV。

### `skin`

当前 `skin` 设置和活动皮肤素材可通过[便携 `.trskin` 预设](skin-presets.md)导出和导入。不包含音频设备、键位、账户、曲目录和时序校准。

下表是配置文件 `config.json` 中的 skin 设置。皮肤包 `skin.json` 的规范见[皮肤格式](skin-format.md)。

| 字段 | 类型、范围、默认值 | 行为 |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | 皮肤来源。 |
| `tenriff_skin_name`, `lr2_skin_name` | string | 导入的皮肤文件夹名称。 |
| `scratch_position` | `left`, `right`; `left` | 仅改变 7+1 皿键的显示顺序，不改变输入与判定轨道。 |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 坐标分辨率；auto 使用 `#DST_NOTE` 坐标，而非文件名。 |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | 在菜单选择预设时重设对应的视觉选项组合。 |
| `note_shape` | `rect`, `simple_rect`, `circle`, `triangle`, `pentagon`, `hexagon`, `square`, `diamond`, `arrow`; `rect` | 程序绘制的音符形状。`simple_rect` 使用直角和纯色表面，保留原有 `rect`。外部图片和明确指定的矢量精灵优先。`hex` 是 `hexagon` 的兼容别名。 |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | 拉伸填充 / 保持比例完整容纳 / 固定宽度并按比例计算高度。 |
| `preserve_note_image_aspect_ratio` | bool; `false` | 旧版兼容字段；显式 `note_image_aspect` 优先，非 stretch 模式保存为 true。 |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | 分别显示音符边框、轨道分隔线与判定线。 |
| `note_divider_gap_px` | double: `0..40`; `12` px | 音符每侧边缘与分隔线的间距；0 表示扩展至分隔线。 |
| `show_gear_boundary_line` | bool; `false` | 显示轨道面板边界线。 |
| `show_timing_feedback` | bool; `true` | 独立显示 FAST/SLOW 文字。 |
| `show_timing_bar` | bool; `true` | 显示时机条；旧配置缺少此字段时继承文字开关。 |
| `timing_bar_always_visible` | bool; `false` | true保留刻度和最新FAST/SLOW，最高判定也更新，0ms清除文字。标记0.7秒到期，条件条在存在标记时显示；show_timing_bar控制开关。 |
| `timing_marker_style` | `dot` / `barcode`; `dot` | 判定标记形状：圆点或竖线。 |
| `timing_feedback_override` | bool; `false` | 用户修改开关后，配置中的显示选项优先于皮肤。 |
| `timing_text_offset_x`, `timing_bar_offset_x` | double: `-600..600`; `0` | 文字和条的独立 X 偏移，叠加于原判定布局，以 1920x1080 像素为基准。 |
| `timing_text_offset_y`, `timing_bar_offset_y` | double: `-400..400`; `0` | 文字和条的独立 Y 偏移；正值向下。 |
| `timing_text_scale`, `timing_bar_scale` | double: `0.50..2`; `1` | FAST/SLOW文字与条的独立缩放，支持50–200%、5%步长或滑块。适用于Studio、Classic、导入皮肤与预览，不改变判定、连击大小或XY位置。 |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | 分别控制LN尾端端帽与渐缩。仅将渐缩的韩文标签改为 `LN 끝 줄임`，外观和判定规则不变。 |
| `judgement_line_glow_enabled` | bool; `true` | 判定线周围发光。 |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst 亮度；0 为关闭。 |
| `key_pulse_enabled` | bool; `true` | 旧版开关兼容；false 或亮度为 0 时关闭。 |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | 内置 Hit Burst 样式。 |
| `hud_layout` | `classic`, `studio`; `studio` | Native／LR2单人HUD，保留LR2音符、按键、边框素材和比例。缺失或未知值使用`studio`，可在Options › Skins选择Classic。1.8.9采用右上信息、左下分数、右下横向地图，较窄外侧空间会触发整体缩放或移至另一侧。极端场地与Performance HUD占满两侧上方时只隐藏信息卡。其他图像皮肤、幽灵与多人保留Classic。[布局与验证范围](release-1.8.9-gate.md)。 |
| `hud_riff_map_visible` | bool; `true` | 显示Studio Deck的64段横向密度图和已用／总时间。默认在右下，外侧空间不足时移动或缩小，极窄时省略时间文字。在Options › Skins › Studio Riff Map中修改并保存到配置和皮肤预设。隐藏后保留其他卡片位置和Classic HUD。 |
| `ui_font` | `default`, `malgun`, `bahnschrift`, `consolas`; `default` | 菜单字体；default 为 Segoe UI，标志、等级与连击保留专用字体。 |
| `key_label_position` | `bottom`, `top`, `off`; `bottom` | 轨道按键名称位置。 |
| `judgement_line_position` | double: `0..1`; `0.82` | 判定线纵向位置比例。 |
| `gameplay_field_offset_x` | double: `-720..720`; `0` | 以 1920×1080 为基准的面板横向偏移；另行限制以保持面板与 ↔ 手柄可见。 |
| `combo_position`, `judgement_position` | double: `0.10..0.78`; `0.24` | 连击与判定独立的 Y 位置；旧配置缺少判定位置时继承 `combo_position`。 |
| `combo_offset_x`, `judgement_offset_x` | double: `-600..600`; `0` | 以 1920×1080 为基准的连击与判定独立 X 偏移。 |
| `combo_font_scale`, `judgement_font_scale` | double: `0.50..2`; `1` | 连击与判定文字的独立倍率。在皮肤设置中以5%步长或鼠标滑块调整50–200%，导入的皮肤同样使用配置倍率。 |
| `lane_background_opacity` | double: `0..0.45`; `0.18` | 轨道背景不透明度。 |
| `black_playfield_enabled` | bool; `true` | 将包括轨道间隙在内的整个区域设为黑色。 |
| `visual_opacity` | double: `0.20..1`; `0.96` | 音符、接收器与按键标签的共用不透明度倍率。 |
| `note_outline_opacity` | double: `0..1`; `0.78` | 音符轮廓不透明度。 |
| `note_fade_in` | double: `0..1`; `0` | 顶部黑雾深度，音符逐渐出现。0为关闭。 |
| `note_fade_out` | double: `0..1`; `0` | 判定线上方黑雾深度，音符逐渐消失。0为关闭。按键与HUD仍可见。 |
| `hold_body_opacity` | double: `0..1`; `1` | 长条主体不透明度。 |
| `lane_width_scales` | object: mode → number[]; `0.50..1.75` | 每轨宽度数组，长度等于轨道数。 |
| `note_width_scale` | double: `0.10..1.40`; `1` | Note & Field Size：以中心为基准同时调整区域、轨道、音符与相邻血条。 |
| `lane_spacing_scales` | object: mode → number[]; `0..2` | 轨道间距数组，长度为 lane_count - 1。 |
| `note_height_scale` | double: `0.50..4`; `1.8` | 音符头尾高度倍率。 |
| `lane_divider_width_scale` | double: `0..2`; `1` | 所有模式共用的分隔线宽度倍率，也适用于导入的 LR2 分隔线。 |
| `lane_center_gap_scale` | double: `0..2`; `0` | 16K 左右区域的中央间距。 |
| `hold_body_width_scale` | double: `0.50..1.20`; `1` | 长条主体宽度倍率。 |
| `note_width_scales`, `note_height_scales`, `lane_center_gap_scales` | object: mode → number | 对应共用值的各模式覆盖项，使用相同范围限制。 |
| `lane_divider_width_scales` | object: mode → number | 旧版兼容字段；当前运行时使用共用 `lane_divider_width_scale`。 |
| `lane_colors` | object: mode → string[] | 每轨一个颜色标记的数组。 |
| `single_color` | string; `off` | 选择颜色标记时覆盖所有轨道，同时保留原 `lane_colors`。 |

各模式数组与覆盖项支持：`4k`, `5k`, `6k`, `7k`, `7+1`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`。颜色标记：`ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`。`7+1` 是皮肤调色板而非独立键位模式。旧 `expand_notes_to_dividers=true` 将间距初始化为 0，显式 `note_divider_gap_px` 优先。

不再应用覆盖整个BGA的暗色滤镜；黑色判定区域、轨道背景和面板仍按原配置保留。Visual Latency是皮肤设置中的第5项。

文字可读性改善统一应用于菜单、设置、歌曲列表、结果、帮助、聊天、账户、编辑器和游戏界面。浅色文字使用深色描边，深色文字使用浅色描边，并保留皮肤原有的文字颜色和透明度。

### `offsets`

- `input` (double)
- `visual` (double)
  - 会被 clamp 在 `-500..500`
- `sound` (double, ms)
  - clamp 到 `-500..500`，并显示在 `Audio Settings > Sound Offset` 与 `Calibration Wizard` 中
  - 正值延后 chart BGM/autoplay keysound，负值提前；判定、note/BGA timing 和按键触发的 `follow` keysound 不会移动

## `keymap.json`

Xbox/XInput按钮使用现有按键名称字段：`PadUp`, `PadDown`, `PadLeft`, `PadRight`, `PadStart`, `PadBack`, `PadLS`, `PadRS`, `PadLB`, `PadRB`, `PadA`, `PadB`, `PadX`, `PadY`, `PadLT`, `PadRT`。在Keymap捕获时按实际按钮；个人档案的重试、Hi-Speed操作也使用相同名称。LT/RT超过30/255即按下，LS/RS为摇杆按下。4个手柄的相同按钮共用一个逻辑输入；不包含轴与震动。[操作、连接与兼容范围](feedback-20261015.zh-CN.md)。

### 结构
- `layout` (string)
- `bindings`
  - 兼容旧版 10K 的 legacy block
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`, `12k`, `14k`, `16k`
  - 每个 mode 下是 lane id -> key token

### 说明
- 旧的单布局 keymap 会在运行时迁移到 10K map。
- 运行时会根据最终谱面的 lane count 选择对应 mode 的 binding。
- 成功完成按键捕获后会立即写入 `keymap.json`，不再需要单独的最终保存步骤。
- 从 Song Select 打开 keymap 编辑时，会优先使用当前选中谱面的 lane count，其次回退到 `mode.key_mode`，最后才是 `10k`。

## 运行时迁移说明
- stale profile 的部分值会被自动修正。
- 尤其是 BMS default 与 keysound policy 相关值会进入运行时迁移；旧 osu chart/skin 字段不再保存。
- 如果配置文件不存在，会先使用默认值启动，并立即保存 profile。

## 设置操作与音画同步

新配置的 `audio.volume` 默认为 `0.1`，已有音量不变。`audio.mute_when_inactive` 默认为 `false`；开启后在其他窗口激活时只静音输出，保留播放位置。ASIO 缓冲区大小显示为每声道采样数，内部 `frames_per_buffer` 名称兼容。

按键设置采用横向 Key 1…，支持主键和辅助键。任一按键按下即可保持逻辑按键状态；× 或绑定时 Delete 可清除辅助键。`keymap.json` 的可选 `secondary_modes` 使用既有模式/轨道结构。选项初次编辑为 4K，从谱面打开时使用实际键数。

`skin.key_backdrop_enabled` 和 `skin.key_backdrop_opacity` (0–1) 控制按键背景色，与击打特效亮度分开。首次修改保存 `key_backdrop_override=true` 并优先使用用户配置。判定线粗细随音符高度变化。选择难度表时 ALL SONG 只显示表内谱面，Native LV 恢复所有谱面。画面和 BGA 使用实际音频播放位置，判定和音频调度保持原时钟。速度发生变化时播放短点击声。

按键背景RGB亮度使用 `skin.key_backdrop_brightness`（0–2，默认1），最大高度使用 `skin.key_backdrop_height`（0–1，默认1），以下边缘为基准。两项独立于透明度，在游戏、幽灵和皮肤预览中一致。带有 `−/+` 的设置可按住左右方向键连续调整。

## BMS录像与结果兼容性（1.9.1）

新增三个已获用户批准的可选记录字段：`bms_gauge_version`、`bms_control_flow_version`、`bms_random_seed`，它们不是用户配置项。版本缺省为1，分支种子缺省时使用旧 `random_seed`，再缺省则为0。新的普通BMS记录血条v2；有分支的谱面还记录控制流v2及uint32分支种子。轨道RR/SR、键数转换和LN Mix使用的 `random_seed` 与谱面分支种子独立。音频设备重新加载时保留相同分支，录像按保存的种子和规则重现。组曲保留原血条并使用新分支解析器。protocol v6对战保留血条/控制流v1及分支种子0。Sites自1.9.1.1起接受符合条件的血条/分支记录，正式v19也以独立榜单接受新R7/R8。保留Guest永久排除及独立Ranked限制。新游玩使用R7/R8，旧记录保持原ID。本地BEST按规则版本组合优先，结果比较只匹配相同规则与分支种子。

[公式、分支、兼容性与验证范围（英文）](bms-total-random-followup.en.md).
