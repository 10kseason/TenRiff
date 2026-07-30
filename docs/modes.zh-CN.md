# 模式系统（Key / Gauge / Random / Mods）

这份文档概述当前已经实现的模式系统、lane transform/随机规则（Mirror/FR/SR）以及 note structure mod。

## 设置位置
- 全局：`config/config.json` 的 `mode` section
- profile：`profiles/<name>/config.json` 的 `mode` section

```json
"mode": {
  "key_mode": "none",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe"
}
```

## 模式含义
- 谱面输入仅支持 BMS family（`.bms/.bme/.bml/.pms`）；`format` 与 `enable_osu_charts` 设置已经移除
- `key_mode`：`none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`：`normal | hard | ex_hard | easy | shift`
- `random`：`off | mirror | fr | sr`
- `random_seed`：FR/SR、强制 key-mode 变换和 LN Mix 目标选择使用的固定 seed（`0` 也视为固定值）
- `mods`：由 Mod Manager 规范化并保存的 mod token 数组
- `ghost_battle_enabled`：`false | true`
  - 默认值为 `false`
  - `true`：自动加载当前选中谱面的最佳兼容 replay 进行 ghost 对比
  - `false`：保持普通单场地游玩
- `autoplay_enabled`：自动处理可判定 note，并把结果标为 `ASSIST`
- `practice_no_fail_enabled`：阻止 gauge 导致的提前失败，继续游玩到谱面结束
- `one_miss_fail_enabled`：首次 OD8 换算对象 `MISS` 即失败的 `Sudden Death (1 MISS)`
  - 仅原生 `BAD` timing 不会触发，空按产生的 `POOR` 也不会触发
  - 在 Mode Settings 中与 Practice No-Fail 互斥
- `song_index_profile`：`safe | fast`
  - `safe`：优先降低 large-library RAM high-water 的默认值
  - `fast`：面向 32GB+ 环境，追求更快重索引的选项

`Rate` 保存在 `speed.rate` 而不是 `mode`。可在 Mode Settings 中调整；未进行搜索文字输入时，也可在 Song Select 中用 `-` / `+` 直接改变下一次游玩的值。

## Lane Transform / 随机规则
- **Mirror**：在 key-mode 变换完成后，确定性地反转最终 lane
  - 10K/16K 不交换两个 player field，而是在各自 half 内独立反转
  - Mirror 本身不使用 `random_seed`，但先执行的强制 key-mode 变换仍可能使用 seed
- **FR（Full Random）**：把整条 lane 替换为随机 **permutation**
- **SR（Super Random）**：按 note 级别随机摆放
  - 选择候选 lane 时，确保同一 lane 上 **不重叠**（包括同一时刻）
  - **Long note 会保持 head/tail 在同一 lane**
  - 如果没有可用候选 lane，就保持原始 lane 并记录警告

## Note Structure Mod
- **Full LN**：把可转换 tap 变成在同 lane 下一 note 前结束的普通 hold
- **LN Mix 10%～90%**：保留已有 hold，并排除与同 lane 已有 span 重叠的 head。使用 `random_seed` 从按 base BPM 计算的 1/8-note hold 能在下一同 lane note 前至少 50ms 结束的 tap 中选择指定比例，并在所有 Mix 档位中把长度确定性分配为 70% 1/16-note、20% 1/8-note、10% 交替的 1/24 与 1/32-note
- **Full Tap**：移除所有 hold tail，把 hold 转换为 tap
- 三项属于同一个 `Note Structure` category，因此只会启用一个；同一谱面和 seed 会复现相同的 LN Mix 结果

## Key mode 处理
- `none` 表示直接使用谱面的原始 lane 数和基础 pattern 布局
- `auto` 作为 legacy alias 保留，当前行为与 `none` 相同
- key-mode 变换先于 Mirror / FR / SR 执行
- `4k..16k` 会通过基于 N2NC 的 lane remap 来匹配目标键数

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
