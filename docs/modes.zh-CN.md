# 模式系统（Format / Key / Gauge / Random）

这份文档概述当前已经实现的模式系统与随机规则（SR/FR）。

## 设置位置
- 全局：`config/config.json` 的 `mode` section
- profile：`profiles/<name>/config.json` 的 `mode` section

```json
"mode": {
  "format": "auto",
  "key_mode": "none",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "song_index_profile": "safe"
}
```

## 模式含义
- `format`：`auto | bms | osu`
- `key_mode`：`none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`：`normal | hard | easy`
- `random`：`off | fr | sr`
- `random_seed`：随机固定用 seed（`0` 也视为固定值）
- `song_index_profile`：`safe | fast`
  - `safe`：优先降低 large-library RAM high-water 的默认值
  - `fast`：面向 32GB+ 环境，追求更快重索引的选项

## 随机规则
- **FR（Full Random）**：把整条 lane 替换为随机 **permutation**
- **SR（Super Random）**：按 note 级别随机摆放
  - 选择候选 lane 时，确保同一 lane 上 **不重叠**（包括同一时刻）
  - **Long note 会保持 head/tail 在同一 lane**
  - 如果没有可用候选 lane，就保持原始 lane 并记录警告

## Key mode 处理
- `none` 表示直接使用谱面的原始 lane 数和基础 pattern 布局
- `auto` 作为 legacy alias 保留，当前行为与 `none` 相同
- `4k..16k` 会通过基于 N2NC 的 lane remap 来匹配目标键数

## 实现位置
- 模式解析：`src/gameplay/ModeSettings.*`、`src/app/ModeResolver.*`
- 模式应用：`src/gameplay/ModeApplier.*`
