# TenRiff 配置模式（当前）

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
- `keysound_volume` (double)

### `input`
- `backend` (string)
  - `polling | rawinput`
- `rawinput` (bool)
- `use_qpc` (bool)
- `grab` (bool)
  - 当前主要对应 Linux preview 语义
- `queue_size` (int)
- `polling_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - polling backend 读取键盘状态的频率
  - 默认值为 `1000`（`1ms`）
- `judgement_hz` (int)
  - `1000 | 2000 | 4000 | 8000`
  - 音频线程内部判定循环的 sub-step 频率
  - 默认值为 `4000`（`0.25ms`）
- `debounce_ms` (double)
  - 在运行时前过滤同一按键上极短暂的 up/down 抖动的输入去抖时间
  - 会被 clamp 在 `0..25` 范围内
  - 默认值为 `8ms`

### `judge`
- `pg`, `gr`, `gd`, `bd` (double, ms)
- 默认 `gd` 为 `75ms`
- 默认 `bd` 为 `340ms`
- `indirect_miss` (double, ms)
  - 在完全没有输入时将 note 自动判为 miss 的间接 miss 标准
  - 当前运行时里不管保存值是什么，都会固定折叠到与 `bd` 相同的值
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
- 不再有自动 gauge shift。所选 gauge 类型会一直保持到歌曲结束或失败。
- Hard / Normal / Easy 都从 `100%` 开始，并在到达 `0%` 时立即失败。
- `delta`
  - `hard`, `normal`, `easy`
  - 每个条目下包含 `PG`, `GR`, `GD`, `BD`, `PR`

### `graphics`
- `display_mode` (string)
  - `borderless | windowed | fullscreen`
  - `windowed` 是带标题栏的固定大小窗口，可拖动
- `resolution` (string)
  - `native | 720p | 1080p | qhd`
- `vsync` (bool)
- `refresh_hz` (int)
  - 会被 clamp 到 `60..1050`
  - 默认值为 `1050`
  - 只有在 `vsync=false` 时才充当直接 FPS 上限
  - `vsync=false` 时，menu 的有效上限为 `300`，gameplay 最多使用配置值到 `1050`
  - `vsync=true` 时，present refresh 以当前活动显示器 Hz 为准，render pacing 的目标是 `monitor_hz * 2`（上限 `1050`）
- `performance_overlay` (bool)

### `mode`
- `format` (string)
  - 默认也会与谱面过滤一起使用
  - `bms | osu | auto`
  - `auto` 实际上表示 `All`
- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
  - `none` 表示直接沿用谱面的原始键数
- `gauge` (string)
  - `normal | hard | easy`
- `random` (string)
  - `off | fr | sr`
- `random_seed` (int)
- `enable_osu_charts` (bool)
- `ghost_battle_enabled` (bool)
  - `true` 时会自动加载当前选中谱面的最佳兼容 replay 作为 ghost 对比
  - `false` 时普通游玩保持单场地显示
- `autoplay_enabled` (bool)
  - QA 用 assist 模式
  - `true` 时会自动处理可判定的按键输入，结果会带上 `ASSIST` clear status
  - 默认 ghost / replay 对比流程会将其视为非竞争性运行
- `practice_no_fail_enabled` (bool)
  - QA 用 assist 模式
  - `true` 时会禁止基于 gauge 的提前失败，但仍保留判定与结果导出直到谱面结束
  - 结果会带上 `ASSIST` clear status
- `song_index_profile` (string)
  - `safe | fast`
  - `safe` 是优先降低大型曲库 RAM high-water 的默认值
  - `fast` 是面向 32GB+ 环境、用更高 worker/batch budget 提升重扫速度的可选值

### `ui`
- `language` (string)
  - `en | ko`
  - 非法值会在加载时规范化为 `en`
  - 对应 Graphics Settings 中的 Language 行
- `result_tail_ms` (double)
- `require_enter_to_exit` (bool)
- `active_song_source` (string)
  - 最近一次打开的 song root
- `recent_song_sources` (array of string)
  - 最近使用过的外部/内部 song source 列表

### `skin`
- `source` (string)
  - `native | osu | lr2`
- `osu_skin_name` (string)
  - 导入的 osu!mania skin 名称
- `lr2_skin_name` (string)
  - 导入的 LR2 playskin 名称
- `lr2_resolution_mode` (string)
  - `auto | sd | hd | fhd`
  - LR2 playskin 的分辨率 override token
  - `auto` 会基于 LR2 playskin `#DST_NOTE` 的布局坐标，而不是 asset 文件名，来判断 SD/HD/FHD family
- `note_shape` (string)
  - `rect | circle`
- `note_border_enabled` (bool)
- `judgement_line_position` (double)
  - gameplay 判定线的垂直位置比例
  - 会被 clamp 在 `0.55..0.86`
  - 默认值为 `0.82`
- `combo_position` (double)
  - gameplay field 内 combo 显示的垂直位置比例
  - 会被 clamp 在 `0.10..0.78`
  - 默认值为 `0.24`
- `lane_width_scales` (object)
  - 按 key mode 保存的单独 lane 宽度缩放数组
  - 每个 mode 的值都是按 lane 数量排列的 number array
  - 每个值都会被 clamp 到 `0.50..1.75`
- `note_width_scale` (double)
  - note head/tail 的横向缩放
  - 会被 clamp 在 `0.50..1.40`
- `lane_spacing_scales` (object)
  - 按 key mode 保存的 lane 之间空白间距缩放数组
  - 每个 mode 的值都是长度为 `(lane_count - 1)` 的 number array
  - 每个值都会被 clamp 到 `0.00..2.00`
- `note_height_scale` (double)
  - note head/tail 的纵向缩放
  - 会被 clamp 在 `0.50..4.00`
- `lane_divider_width_scale` (double)
  - 白色 lane 分隔线的共享宽度缩放
  - 会被 clamp 在 `0.00..2.00`
  - 会统一应用到所有 key mode
  - native skin 会乘到默认 `1px` divider 上；osu/lr2 skin 如果带有导入 divider 宽度，也会一起乘上这个值
- `lane_center_gap_scale` (double)
  - 16K 场地左右两半之间的中央间隔缩放
  - 会被 clamp 在 `0.00..2.00`
  - 当前只对 `16k` 布局生效
- `hold_body_width_scale` (double)
  - long note body 的横向缩放
  - 会被 clamp 在 `0.50..1.20`
  - 实际渲染计算以 `max(4.0f, note_width * 0.5f * scale)` 为准
- `note_width_scales` (object)
  - 按 key mode 保存的 `note_width_scale` override
- `note_height_scales` (object)
  - 按 key mode 保存的 `note_height_scale` override
- `lane_divider_width_scales` (object)
  - 旧版兼容字段
  - 当前运行时只使用共享的 `lane_divider_width_scale`
- `lane_center_gap_scales` (object)
  - 按 key mode 保存的 `lane_center_gap_scale` override
- `lane_colors` (object)
  - 按 key mode 保存的 lane 颜色调色板
  - 当前默认/保存对象为 `4k..10k`、`16k`
  - 每个 mode 的值都是按 lane 数量排列的 string array
  - 支持的 token：
    `ice`, `azure`, `gold`, `mint`, `rose`, `violet`, `orange`, `teal`

### `offsets`
- `input` (double)
- `visual` (double)
  - 会被 clamp 在 `-500..500`

## `keymap.json`

### 结构
- `layout` (string)
- `bindings`
  - 兼容旧版 10K 的 legacy block
- `modes`
  - `4k`, `5k`, `6k`, `7k`, `8k`, `9k`, `10k`
  - 每个 mode 下是 lane id -> key token

### 说明
- 旧的单布局 keymap 会在运行时迁移到 10K map。
- 运行时会根据最终谱面的 lane count 选择对应 mode 的 binding。
- 成功完成按键捕获后会立即写入 `keymap.json`，不再需要单独的最终保存步骤。
- 从 Song Select 打开 keymap 编辑时，会优先使用当前选中谱面的 lane count，其次回退到 `mode.key_mode`，最后才是 `10k`。

## 运行时迁移说明
- stale profile 的部分值会被自动修正。
- 尤其是 BMS-first default、osu key-mode mismatch、keysound policy 相关值，都会进入运行时迁移流程。
- 如果配置文件不存在，会先使用默认值启动，并立即保存 profile。
