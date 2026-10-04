# TenRiff 配置模式（当前）

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

- `play_to_end` (bool; default `true`): 最后一次判定后播放剩余音乐。false 时仅等待 `ui.result_tail_ms` 后正常进入结果。判定、分数、回放与手动跳过不变。

- `backend` (string)
  - `wasapi | asio`，默认为 `wasapi`。用于游玩和曲目试听；菜单和结果 BGM 保留独立的 Windows MCI 路径。
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
  - master volume
- `bgm_volume` (double)
- `normalize_audio` (bool)
  - 游戏内RMS音量调整，默认false。ON保留RMS→软限幅→主音量；OFF仅应用线性主音量与最终输出范围限制。菜单音乐及选曲试听不变。
- `keysound_volume` (double)

参见 [ASIO 设置](asio-audio.md)。ASIO 固定所选采样率并对谱面音频重采样。`frames` 为请求值，按驱动支持的大小协商。预设不覆盖 ASIO frames；`exclusive` 与 `periods` 仅用于 WASAPI。ASIO 失败时不会自动切换 WASAPI。

### `input`

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
  - 保留真实 Press/Release 转换，仅从 pressed-state tracking 中移除同状态重复 event
  - clamp 到 `0..25`
  - 默认值为 `8ms`
### `judge`
- BMS `#RANK`：`3/EASY` PG21ms、`2/NORMAL` PG18ms、`1/HARD` PG15ms、`0/VERYHARD` PG8ms。EASY的GR/GD/BAD保持65/115/210ms，其他难度分别缩小至18/21、15/21、8/21倍。缺失或不支持的值使用EASY。Judge Easy/Hard模组随后应用；RANK不改变长按容差与自动漏键时限。
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 默认 `pg / gr / gd` 分别为 `21ms / 65ms / 115ms`
- 默认 `bd` 为 `210ms`
- `Judge Easy` 将基础判定窗扩大为 `1.35x`：`pg/gr/gd/bd=28.35/87.75/155.25/283.5ms`。长按容差也使用同一倍率，`mask` 保持不变
- `Judge Hard` 保持PG/GR/GD和长按容差，将 `bd` 上限限制为 `180ms`。更小的自定义BAD窗口不会被扩大
- `indirect_miss` (double, ms)
  - 当前配置将此值保存并归一化为 `340ms`，自动漏键判定时限独立于BAD命中窗口
  - 默认Normal/Easy/Hard在未输入音符超过 `340ms` 后自动判漏键；Normal/Easy记BAD，Hard记断连的间接 `POOR` / OD8 `MISS`
  - BAD窗口外、自动判漏键前的迟到输入不会命中BAD，而是先将过期音符记为漏键，再检查下一音符
- 新游玩记录 `tenriff-native-score-v2-ruleset-3`。旧ruleset-1/2的回放、幽灵与验证恢复PG20ms、不应用RANK并保留旧长按松键行为。ruleset-1仍使用Easy1.25x、Hard BAD340ms和自动漏键=BAD。自定义判定不作为官方规则。
- `hold_grace` (double, ms)
  - 将 long note tail release 判为 `PG` 的专用宽限窗口
  - 默认值为 `80ms`
- `hold_break` (double, ms)
  - 允许 long note tail release 判到 `GR` 的最后窗口
  - 超出此范围即为 `BD`
  - 内部始终保持不低于 `hold_grace`
  - 默认值为 `200ms`
- `mask` (double, ms)

### `speed`
- `rate` (double)
- `hispeed` (double)
- `target_scroll_bps` (double)

- 基准 BPM 为累计进行时间最长的速度；相同 BPM 的区间合并计算，排除 STOP 等待。Hi-Speed 以此固定，保留显式 `#SCROLL`、停止和逆向效果。参见[计算规则](reference-bpm.md)。

### `gauge`

Gauge Shift 始终启用。`mode.gauge` 的 `ex_hard / hard / normal / easy` 选择从 EX 到 Easy 的起始档位。所选档位及以下档位均从 100% 独立并行计算，当前档位淘汰后转至下一存活档位。全部可用档位淘汰后才发生血条失败。旧 `shift` 值表示从 EX 开始。

段位/Session Mix 使用跨曲继承的 LR2 参考血条，保留间接失误，HP 低于 2% 时失败。普通 LN 在完成/释放时只结算一次，不应用普通 `gauge.delta`。参见[比较与兼容范围](lr2-gauge-audit.ko.md)。

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - 默认值为 `borderless`；Discord、OBS、Game Bar 等外部 overlay 也推荐使用此模式
  - `windowed` 是带标题栏的固定大小窗口，可拖动
  - `fullscreen` 是 DXGI 独占全屏，当前 Discord Game Overlay 不会显示
- `resolution` (string)
  - `native`、旧别名 `720p | 1080p | qhd`，或 `宽x高`（例如 `1600x900`, `1366x768`, `1280x800`, `3440x1440`）。每轴支持320–8192px。
  - 图形设置包含常见尺寸和当前显示器的显示模式。按`F5`刷新列表。自定义尺寸在保存和重启后保留。
  - 布局保持1920×1080画布比例，其他比例显示留边。窗口模式按比例缩小，以容纳标题栏和任务栏工作区域。
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
  - `onnx | off`；旧 `lunasr` 值会迁移为 `onnx`
  - 默认值为 `off`；通过 Graphics Settings 中的 `BGA Upscaler` 明确切换 ON/OFF
  - 开启时必须确认高配置警告；不会自动运行性能 benchmark
- `background_upscale_model_path` (string)
  - 可在 Graphics Settings 的 `ONNX Model` 中选择，或把 `.onnx` 文件拖入该页面；选择只保存路径，不会自动开启 upscaler
  - 支持绝对路径，或相对于可执行文件/当前目录的路径；公开包不包含模型
  - 当前契约：float32 或 float16 NCHW `rgb_lr [1,3,540,960]` -> `rgb_residual_x2 [1,3,1080,1920]` residual x2；支持并检测外部边界保持浮点的 INT8 QDQ 模型
  - 加载、契约或推理失败时保持 native scaling
  - 用户需自行确认模型权利、质量与性能；详见 `tools/onnx_upscaler/README.md`
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
- `random` (string)
  - `off | mirror | rr | frns | sr` (`fr` = `frns`)
- `random_seed` (int)
  - RR/SR、强制 key-mode 变换和 LN Mix 目标选择使用固定 seed；普通 Random 每次游玩生成新的 session seed，并把实际值写入 replay
- `mods` (string array)
  - Note Structure 可在 `full_long_notes`、`ln_mix_10` 到 `ln_mix_90`、`full_short_notes` 中选择一个
  - LN Mix 仅把按 base BPM 计算的 1/8-note hold 能在同 lane 下一音符前至少 50ms 结束的 tap 作为候选，并把所选 hold 的长度分配为 60% 长 1/8-note、20% 中 1/16-note、20% 短且交替的 1/24 与 1/32-note
  - 已有 hold 会保留，与同 lane 已有 span 重叠的 head 会被排除，相同 `random_seed` 会选择相同 tap
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
  - `off | accuracy | score`，默认值为 `off`
  - Accuracy/Score 模式会运行到谱面结束，仅在达到所选 result target 时 clear
  - 启用 Pacemaker 会关闭 Practice 与 Sudden Death；replay playback 和 multiplayer 会强制关闭它
- `pacemaker_target_accuracy` (double)
  - `0..100`，默认 `90.0`；与标准 result Accuracy 比较
- `pacemaker_target_score` (int)
  - `0..10000`，默认 `8000`；与倍率应用后的最终显示 Score 比较
- `song_index_profile` (string)
  - `safe | fast`
  - `safe` 是优先降低大型曲库 RAM high-water 的默认值
  - `fast` 是跳过文件 hash、preview、难度表和原生 LV/CR 的可选最小 profile
- `calculate_song_index_difficulty` (bool)
  - 默认值为 `false`
  - `false` 保留 BMS `#PLAYLEVEL` 作为菜单 LV，并跳过 CPU 开销较高的原生 LV/CR 计算
  - `true` 仅在完整 `safe` 索引中计算 Revive LV/Circus Rating；`fast` 始终跳过
  - 修改设置后会区分缓存模式，并对当前 song source 执行完整重索引

### `ui`

`last_played_chart_path` 保存最近一次开始游玩的谱面本地路径，默认空字符串。回放和编辑器练习不会更新；重启后可供 `title_music=last_played` 使用。

| 字段 | 类型、范围、默认值 | 行为 |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | 显示名称；为空时使用配置 ID，并规范化空白与控制字符。 |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | 本地 PNG/JPG 头像路径。 |
| `language` | `en`, `ko`, `ja`; `en` | UI 语言；无效值规范化为 en。 |
| `menu_font_size` | `normal`, `large`, `extra_large`; `normal` | 配置档和首次设置中的菜单字号：100%、115%、130%。游戏内字体继续由皮肤控制。 |
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
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | 难度边界；0 表示不启用对应边界。 |
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
| `note_shape` | `rect`, `circle`, `triangle`, `pentagon`, `hexagon`, `square`, `diamond`, `arrow`; `rect` | 程序绘制的音符形状。`hex` 是 `hexagon` 的兼容别名。 |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | 拉伸填充 / 保持比例完整容纳 / 固定宽度并按比例计算高度。 |
| `preserve_note_image_aspect_ratio` | bool; `false` | 旧版兼容字段；显式 `note_image_aspect` 优先，非 stretch 模式保存为 true。 |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | 分别显示音符边框、轨道分隔线与判定线。 |
| `note_divider_gap_px` | double: `0..40`; `12` px | 音符每侧边缘与分隔线的间距；0 表示扩展至分隔线。 |
| `show_gear_boundary_line` | bool; `false` | 显示轨道面板边界线。 |
| `show_timing_feedback` | bool; `true` | 独立显示 FAST/SLOW 文字。 |
| `show_timing_bar` | bool; `true` | 显示时机条；旧配置缺少此字段时继承文字开关。 |
| `timing_feedback_override` | bool; `false` | 用户修改开关后，配置中的显示选项优先于皮肤。 |
| `timing_text_offset_x`, `timing_bar_offset_x` | double: `-600..600`; `0` | 文字和条的独立 X 偏移，叠加于原判定布局，以 1920x1080 像素为基准。 |
| `timing_text_offset_y`, `timing_bar_offset_y` | double: `-400..400`; `0` | 文字和条的独立 Y 偏移；正值向下。 |
| `show_hold_tail`, `hold_tail_taper_enabled` | bool; `false` | 分别控制长条尾部端帽与渐缩，不改变判定规则。 |
| `judgement_line_glow_enabled` | bool; `true` | 判定线周围发光。 |
| `key_pulse_brightness` | double: `0..1`; `1` | Hit Burst 亮度；0 为关闭。 |
| `key_pulse_enabled` | bool; `true` | 旧版开关兼容；false 或亮度为 0 时关闭。 |
| `hit_burst_style` | `prism`, `ring`, `spark`; `prism` | 内置 Hit Burst 样式。 |
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
| `hold_body_opacity` | double: `0.05..1`; `1` | 长条主体不透明度。 |
| `lane_width_scales` | object: mode → number[]; `0.50..1.75` | 每轨宽度数组，长度等于轨道数。 |
| `note_width_scale` | double: `0.50..1.40`; `1` | Note & Field Size：以中心为基准同时调整区域、轨道、音符与相邻血条。 |
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

新配置的 `audio.volume` 默认为 `0.7`，已有音量不变。`audio.mute_when_inactive` 默认为 `false`；开启后在其他窗口激活时只静音输出，保留播放位置。ASIO 缓冲区大小显示为每声道采样数，内部 `frames_per_buffer` 名称兼容。

按键设置采用横向 Key 1…，支持主键和辅助键。任一按键按下即可保持逻辑按键状态；× 或绑定时 Delete 可清除辅助键。`keymap.json` 的可选 `secondary_modes` 使用既有模式/轨道结构。选项初次编辑为 4K，从谱面打开时使用实际键数。

`skin.key_backdrop_enabled` 和 `skin.key_backdrop_opacity` (0–1) 控制按键背景色，与击打特效亮度分开。首次修改保存 `key_backdrop_override=true` 并优先使用用户配置。判定线粗细随音符高度变化。选择难度表时 ALL SONG 只显示表内谱面，Native LV 恢复所有谱面。画面和 BGA 使用实际音频播放位置，判定和音频调度保持原时钟。速度发生变化时播放短点击声。

按键背景RGB亮度使用 `skin.key_backdrop_brightness`（0–2，默认1），最大高度使用 `skin.key_backdrop_height`（0–1，默认1），以下边缘为基准。两项独立于透明度，在游戏、幽灵和皮肤预览中一致。带有 `−/+` 的设置可按住左右方向键连续调整。
