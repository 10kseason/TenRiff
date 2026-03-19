# 核心播放循环（初期实现）

这份文档整理当前已经实现的 **核心播放循环** 的构成与数据流。

## 核心流程
1. **InputThread** 收集 RawInput 或 polling 输入 → 交给 `SPSCQueue`
2. **AudioThread** 回调里用 `ClockSync` 把输入时间戳转换为 sample time
3. **GameplayEngine** 消费输入/时间线，更新判定、gauge 与统计
4. 音频缓冲区目前做的是 **静音(fill 0)** 处理（keysound/音源混音留待后续）

## 主要组件
- `config/Config.*`
  - 用 **SimpleJson** 读取 `config.json`
  - 反映 `audio/input/judge/speed/gauge/ui/offsets` 各 section
- `config/Keymap.*`
  - 读取 `keymap.json` 并生成默认 keymap
  - 使用 `KeycodeMap` 把按键字符串转换为 keycode
- `gameplay/GameplayChart.*`
  - 将 BMS/OSU 时间线转换为 **以 sample time 为基础的 note event**
  - 在应用 `rate` 时，把时间表按 `t' = t / rate` 进行缩放
- `gameplay/GameplayEngine.*`
  - 应用判定窗口（PG/GR/GD/BD）与 mask（30ms）
  - POOR 发生时应用 lane mask
  - **Hold 规则**：过早 release 判定为 BAD
  - **Hold Tail 规则**：只有 osu!mania hold 与 BMS `#LNMODE 2` charge note 才把 release timing 当作普通判定窗口处理（head/tail 50:50）
  - 普通 BMS long note 只要保持到结束，就由 tail 自动处理，不使用 tail release timing 判定
  - 收集结果统计（combo、判定计数、平均/标准差）
- `app/GameSession.*`
  - CLI 参数 → 应用设置 → 载入谱面 → 启动输入/音频线程
  - 在音频回调中消费输入队列并更新判定

## 判定相关的初始策略（待确认）
- **Hold Tail 判定** 仅适用于 osu!mania hold 和 BMS `#LNMODE 2` charge note；过早 release 视为 BAD

## 后续将连接的部分
- 菜单状态机（Title/SongSelect/Gameplay/Result）
- SongIndexerThread + 缓存
- key remap UI + NKRO 测试画面
- ✅ Result 画面 + replay/result JSON 保存
- 启动器扩展以及日志/环境诊断
