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
  - 控制菜单 BGM 和谱面背景音的开关
- `volume` (double)
  - master volume
- `bgm_volume` (double)
- `normalize_audio` (bool)
  - 对游戏内立体声混音进行联动 RMS 音量调整，位于 limiter/master volume 之前；默认 false，不改变菜单音乐与选曲试听。
- `keysound_volume` (double)

### `input`

- `backend` (string)
  - `polling | rawinput`
  - 当前 `1.7.1` 发布线默认值为 `rawinput`
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
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 默认 `pg / gr / gd` 分别为 `20ms / 65ms / 115ms`
- 默认 `bd` 为 `210ms`
- `Judge Easy` 沿用现有 `1.25x` 倍率（`bd=262.5ms`），`Judge Hard` 使用 `bd=340ms`；Hard 不会收紧 PG/GR/GD 与长按尾部判定窗
- `indirect_miss` (double, ms)
  - 在完全没有输入时将 note 自动判为 miss 的间接 miss 标准
  - timing 与 `bd` 对齐；在 `Judge Hard` 下，未输入 note 会记为断 combo 的间接 `POOR` / OD8 `MISS`，而不是 BAD
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

### `gauge`

Gauge Shift 始终启用。`mode.gauge` 的 `ex_hard / hard / normal / easy` 选择从 EX 到 Easy 的起始档位。所选档位及以下档位均从 100% 独立并行计算，当前档位淘汰后转至下一存活档位。全部可用档位淘汰后才发生血条失败。旧 `shift` 值表示从 EX 开始。

- `delta`: `ex_hard`, `hard`, `normal`, `easy` → `PG`, `GR`, `GD`, `BD`, `PR`.

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - 默认值为 `borderless`；Discord、OBS、Game Bar 等外部 overlay 也推荐使用此模式
  - `windowed` 是带标题栏的固定大小窗口，可拖动
  - `fullscreen` 是 DXGI 独占全屏，当前 Discord Game Overlay 不会显示
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
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

| 字段 | 类型、范围、默认值 | 行为 |
| --- | --- | --- |
| `profile_nickname` | string; UTF-8 ≤48 bytes | 显示名称；为空时使用配置 ID，并规范化空白与控制字符。 |
| `profile_avatar_path` | string; UTF-8 ≤2048 bytes | 本地 PNG/JPG 头像路径。 |
| `language` | `en`, `ko`; `en` | UI 语言；无效值规范化为 en。 |
| `result_tail_ms` | double; `500` ms | 判定完成后结果切换的额外等待时间，同时考虑谱面音频结束时刻。 |
| `require_enter_to_exit` | bool; `true` | 为读写兼容保留；当前 Windows 结果输入路径不使用此值自动退出。 |
| `show_cursor_in_gameplay` | bool; `true` | 游戏中显示鼠标指针。 |
| `active_song_source`, `recent_song_sources` | string / string[] | 当前与最近使用的歌曲文件夹。 |
| `session_mix_lr2_course_path` | string | 所选 LR2 课程文件路径。 |
| `favorite_chart_keys` | string[] | 收藏谱面的内部标识键。 |
| `collections` | object: name → string[] | 按集合名称分组的谱面键列表。 |
| `song_collection_filter` | string; `all` | 全部、收藏或集合筛选；即时保存。 |
| `song_key_filter` | int: `0..16`; `0` | 键数筛选；0 为全部。UI 可选 4K–10K、12K、14K、16K。 |
| `song_level_min_filter`, `song_level_max_filter` | int: `0..50`; `0` | 难度边界；0 表示不启用对应边界。 |
| `difficulty_table_path` | string | 本地 header JSON 或下载到配置缓存的 header；变更时重新索引，选择表时切换至 safe 索引。 |
| `difficulty_table_url` | string | 原始 HTTP(S) BMSTable 页面或 header URL；解析 meta 并缓存 header/data，选择本地 JSON 时清空。 |
| `online_records_server_url` | string | 记录、排名与聊天 API URL；失败不阻止本地游玩或记录。 |
| `tenriff_main_server_url` | string | F10 主服务器 API URL；默认值为 Config.h 中的 `kTenRiffMainApiUrl`。 |
| `private_server_url` | string | F10 私有 API URL；远程需 HTTPS，仅 localhost 可使用 HTTP。 |
| `account_server_mode` | `main`, `private`; `main` | 上次选择的账户服务器。 |

难度表 header 使用 `name`、`symbol` 与本地相对 `data_url`；data 项按 `md5` 或 `sha256` 及 `level` 匹配。远程导入保存在配置的 `difficulty_tables` 缓存中。

### `skin`

下表是配置文件 `config.json` 中的 skin 设置。皮肤包 `skin.json` 的规范见[皮肤格式](skin-format.md)。

| 字段 | 类型、范围、默认值 | 行为 |
| --- | --- | --- |
| `source` | string: `native`, `tenriff`, `lr2` | 皮肤来源。 |
| `tenriff_skin_name`, `lr2_skin_name` | string | 导入的皮肤文件夹名称。 |
| `scratch_position` | `left`, `right`; `left` | 仅改变 7+1 皿键的显示顺序，不改变输入与判定轨道。 |
| `lr2_resolution_mode` | `auto`, `sd`, `hd`, `fhd`; `auto` | LR2 坐标分辨率；auto 使用 `#DST_NOTE` 坐标，而非文件名。 |
| `visual_preset` | `classic`, `neon`, `minimal`, `tenriff`; `tenriff` | 在菜单选择预设时重设对应的视觉选项组合。 |
| `note_shape` | `rect`, `triangle`, `pentagon`, `hexagon`, `circle`; `rect` | 程序绘制的音符形状。 |
| `note_image_aspect` | `stretch`, `contain`, `width`; `stretch` | 拉伸填充 / 保持比例完整容纳 / 固定宽度并按比例计算高度。 |
| `preserve_note_image_aspect_ratio` | bool; `false` | 旧版兼容字段；显式 `note_image_aspect` 优先，非 stretch 模式保存为 true。 |
| `note_border_enabled`, `show_lane_dividers`, `show_judgement_line` | bool; `true` | 分别显示音符边框、轨道分隔线与判定线。 |
| `note_divider_gap_px` | double: `0..40`; `12` px | 音符每侧边缘与分隔线的间距；0 表示扩展至分隔线。 |
| `show_gear_boundary_line` | bool; `false` | 显示轨道面板边界线。 |
| `show_timing_feedback` | bool; `true` | 显示 FAST/SLOW 文字与时机历史；判定等级独立保留。 |
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
| `lane_background_opacity` | double: `0..0.45`; `0.18` | 轨道背景不透明度。 |
| `black_playfield_enabled` | bool; `true` | 将包括轨道间隙在内的整个区域设为黑色。 |
| `visual_opacity` | double: `0.20..1`; `0.96` | 音符、接收器与按键标签的共用不透明度倍率。 |
| `note_outline_opacity` | double: `0..1`; `0.78` | 音符轮廓不透明度。 |
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
