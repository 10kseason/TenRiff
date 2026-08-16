# 模式系统（Key / Gauge / Random / Mods）

这份文档概述当前已经实现的模式系统、lane transform/随机规则（Mirror/FR/SR）以及 note structure mod。

## 设置位置
- 全局：`config/config.json` 的 `mode` section
- profile：`profiles/<name>/config.json` 的 `mode` section

```json
"mode": {
  "key_mode": "none",
  "key_conversion_algorithm": "krrcream",
  "key_conversion_nk2_preset": "native",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe",
  "calculate_song_index_difficulty": false
}
```

## 模式含义
- chart input 仅支持 BMS family（`.bms/.bme/.bml/.pms`）；旧 osu 开关不再显示或保存
- `key_mode`：`none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
- `key_conversion_algorithm`：`krrcream | nk2 | nk3`（默认 `krrcream`；Krrcream 只重排原始 note，nK2 在扩展键数时生成安全辅助 note，NK3 使用随包提供的 P64 hybrid ONNX 与 host beam32 评估重排和辅助 note 候选）
- `key_conversion_nk2_preset`：`native | transform | remaster`（默认 `native`；选择 nK2 时可用 `Native (12%)` / `Transform (35%)` / `Remaster (65%)`，选择 Krrcream 时锁定该行）
- `gauge`：`normal | hard | ex_hard | easy | shift`
- `random`：`off | mirror | rr | fr | sr`
- `random_seed`：RR/FR/SR、强制 key-mode 变换、Note Add 和 LN Mix 目标选择使用的固定 seed（`0` 也视为固定值）
- `mods`：由 Mod Manager 规范化并保存的 mod token 数组
- `ghost_battle_enabled`：`false | true`
  - 默认值为 `false`
  - `true`：自动加载当前选中谱面的最佳兼容 replay 进行 ghost 对比
  - `false`：保持普通单场地游玩
- `autoplay_enabled`：自动处理可判定 note，并保存为 `AUTOPLAY`，但不计入正式 clear、best score 或 clear lamp
- `practice_no_fail_enabled`：阻止 gauge 导致的提前失败，继续游玩到谱面结束
- `one_miss_fail_enabled`：首次 OD8 换算对象 `MISS` 即失败的 `Sudden Death (1 MISS)`
  - 仅原生 `BAD` timing 不会触发，空按产生的 `POOR` 也不会触发
  - 在 Mode Settings 中与 Practice No-Fail 互斥
- `song_index_profile`：`safe | fast`
  - `safe`：优先降低 large-library RAM high-water 的默认值
  - `fast`：仅保留 title/artist/key count/#PLAYLEVEL/BPM，并跳过 hash、preview、难度表和原生 LV/CR 的最小索引
- `calculate_song_index_difficulty`：`false | true`
  - 默认 `false`：保留 BMS `#PLAYLEVEL`，跳过原生 LV/CR 计算
  - `true`：在完整 `safe` 重索引中计算 Revive LV/Circus Rating；`fast` 始终跳过

`Rate` 保存在 `speed.rate` 而不是 `mode`。可在 Mode Settings 中调整；未进行搜索文字输入时，也可在 Song Select 中用 `-` / `+` 直接改变下一次游玩的值。

## Lane Transform / 随机规则
- **DP Flip**：交换 DP 左右两个完整 player field，同时保持各 field 内部 lane 顺序
- **Mirror**：在 key-mode 变换完成后，确定性地反转最终 lane
  - DP layout 不交换两个 player field，而是在各自 field 内独立反转
  - Mirror 本身不使用 `random_seed`，但先执行的强制 key-mode 变换仍可能使用 seed
- **RR（R-Random）**：固定皿键，并按 seed 偏移旋转各 playable lane group；DP 左右区域独立处理
- **FR（Full Random）**：把整条 lane 替换为随机 **permutation**
- **SR（Super Random）**：按 note 级别随机摆放
  - 选择候选 lane 时，确保同一 lane 上 **不重叠**（包括同一时刻）
  - **Long note 会保持 head/tail 在同一 lane**
  - 如果没有可用候选 lane，就保持原始 lane 并记录警告

## Note Structure Mod
- **Note Add 10%～100%**：在已有 note 时刻确定性加入无声和弦 note，并避开皿键、hold body、同 lane 重复和过大和弦。结果会保留在 Records 中，但不会覆盖普通最佳记录
- **Full LN**：把可转换 tap 变成在同 lane 下一 note 前结束的普通 hold
- **LN Mix 10%～90%**：保留已有 hold，并排除与同 lane 已有 span 重叠的 head。使用 `random_seed` 从按 base BPM 计算的 1/8-note hold 能在下一同 lane note 前至少 50ms 结束的 tap 中选择指定比例，并在所有 Mix 档位中把长度确定性分配为 60% 长 1/8-note、20% 中 1/16-note、20% 短且交替的 1/24 与 1/32-note
- **Full Tap**：移除所有 hold tail，把 hold 转换为 tap
- 三项属于同一个 `Note Structure` category，因此只会启用一个；同一谱面和 seed 会复现相同的 LN Mix 结果

## Key mode 处理
- `none` 表示直接使用谱面的原始 lane 数和基础 pattern 布局
- `auto` 作为 legacy alias 保留，当前行为与 `none` 相同
- `4k..10k`、`12k`、`14k`、`16k` 会通过基于 N2NC 的 lane remap 匹配目标键数
- 强制转换 `5+1 SP` / `7+1 SP` 时只重新映射除皿键外的键盘部分；`follow` 皿键音效移至自动播放
- `10+2 DP` / `14+2 DP` 同样排除两个皿键，并独立转换左右键盘区域
- nK2 扩展会先把原始 note 布置到目标键位，再在同一目标 layout 中生成辅助 note；不会先向原始4K等谱面加 note 后再次转换
- 应用顺序：key-mode 变换（含 nK2 目标 layout 辅助 note 生成）→ DP Flip → Mirror/RR/FR/SR → Note Add → LN/Full Tap 结构变换

## Gauge 规则
- 固定 gauge（`ex_hard / hard / normal / easy`）从 `100%` 开始，在 `0%` 时立即失败且不会改变类型。
- `shift` 会让 EX-Hard / Hard / Normal / Easy 分别从 100% 开始独立并行计算；当前 tier 淘汰后选择已累计相同判定历史的下一档存活 tier，并以结束时最高的存活 tier 为最终结果。
- `ex_hard` 是挑战用 gauge，回复低于 Hard，`BAD` / `POOR` 损失更大。
- clear status 会区分固定 gauge 结果，以及最终 Shift tier 的 `GAUGE SHIFT EX-HARD / HARD / NORMAL / EASY CLEAR`。
- `Sudden Death (1 MISS)` 不是 gauge 类型，而是首次 OD8 换算对象 `MISS` 时把当前 gauge 置零并立即结束的独立失败规则。

## 实现位置
- 模式解析：`src/gameplay/ModeSettings.*`、`src/app/ModeResolver.*`
- 模式应用：`src/gameplay/ModeApplier.*`
- Mod registry / note structure 变换：`src/app/ModeManager.*`
