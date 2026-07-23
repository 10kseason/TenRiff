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
  - 当前 `1.1.9` 发布线默认值为 `rawinput`
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
- EX-Hard / Hard / Normal / Easy 都从 `100%` 开始，并在到达 `0%` 时立即失败。
- `delta`
  - `ex_hard`, `hard`, `normal`, `easy`
  - 每个条目下包含 `PG`, `GR`, `GD`, `BD`, `PR`

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
  - 会被 clamp 到 `60..1050`
  - 默认值为 `300`
  - 只有在 `vsync=false` 时才充当直接 FPS 上限
  - `vsync=false` 时，menu 的有效上限为 `300`，gameplay 的 render pacing 会安全限制到 `min(configured target, max(300, monitor_hz * 2))`
  - `vsync=true` 时，present refresh 以当前活动显示器 Hz 为准，render pacing 的目标是 `monitor_hz * 2`（上限 `1050`）
- `performance_overlay` (bool)
  - 默认值为 `false`；它位于右上角，可能会与放在同一角落的 Discord Voice widget 重叠

### `mode`
- `format` (string)
  - 默认也会与谱面过滤一起使用
  - `bms | osu | auto`
  - `auto` 实际上表示 `All`
- `key_mode` (string)
  - `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
  - `none` 表示直接沿用谱面的原始键数
- `gauge` (string)
  - `normal | hard | ex_hard | easy`
- `random` (string)
  - `off | mirror | fr | sr`
- `random_seed` (int)
  - FR/SR、强制 key-mode 变换和 LN Mix 目标选择使用的固定 seed；Mirror 变换本身不使用
- `mods` (string array)
  - Note Structure 可在 `full_long_notes`、`ln_mix_10` 到 `ln_mix_90`、`full_short_notes` 中选择一个
  - LN Mix 仅把既能形成至少 50ms hold、又能在同 lane 下一音符前保留 50ms 间隔的 tap 作为候选，并按指定比例四舍五入后转换为普通 hold
  - 已有 hold 会保留，与同 lane 已有 span 重叠的 head 会被排除，相同 `random_seed` 会选择相同 tap
- `enable_osu_charts` (bool)
- `ghost_battle_enabled` (bool)
  - 默认值为 `false`
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
- `one_miss_fail_enabled` (bool)
  - `true` 时首次出现 osu!mania OD8 对象 `MISS` 就会把 gauge 归零并立即失败
  - 仅原生 `BAD` timing 不会触发，空键输入产生的 `POOR` 也不会触发该模式
  - 在 Mode Settings 中启用后会自动关闭 `practice_no_fail_enabled`
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
  - 会被 clamp 在 `0.00..1.00`（0%～100%）
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
