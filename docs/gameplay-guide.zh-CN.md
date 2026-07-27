# TenRiff 游玩指南

这份文档专注于“第一次启动 TenRiff 的人，如何实际选歌、开始游玩并查看结果”的用户指南。

更细的配置结构请参考 [`docs/config.zh-CN.md`](config.zh-CN.md)，当前实现范围请参考 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)。

## 1. 首次启动

Windows 下通常会使用下面两种方式之一：

```powershell
.\launch_win.bat
```

或者

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

如果是第一次启动，默认 profile 会自动创建。

## 2. 基本流程

TenRiff 的基础游玩流程如下：

1. 进入 Title 界面
2. 在 Song Select 里选歌
3. 如有需要，调整 Mode / Audio / Graphics / Skins / Keymap
4. 开始歌曲
5. 经过 `3 / 2 / 1` 倒计时后开始游玩
6. 查看 Result 界面
7. 用 `Enter` 或 `Esc` 返回 Song Select

## 3. 基本菜单操作

### Title
- `Up` / `Down`：移动菜单
- `Enter`：确认
- `Esc`：退出

### Song Select
- `Up` / `Down`：移动歌曲
- `PageUp` / `PageDown`：快速移动
- 鼠标滚轮：移动歌曲
- 左键：选择歌曲
- 双击：开始歌曲
- `Enter`：开始当前歌曲
- `Left` / `Right`：切换左侧菜单焦点
- `Esc`：返回上一个画面
- `F5`：重新索引曲库

### Song Select 中常用的设置页面
- `Mode`
  - 调整 Ghost Battle、Autoplay、Practice、Sudden Death、Key Mode、Gauge、Random、Mods、Rate、Hi-Speed、OSU Charts、Chart Filter
- `Audio`
  - 调整 Master/BGM/Keysound 音量以及 BMS keysound policy
- `Graphics`
  - 调整 VSync、Refresh Hz、Performance HUD、Display Offset
- `Skins`
  - 调整判定线位置、note 大小、LN body 宽度、lane color
- `Keymap`
  - 调整按键布局并做 NKRO 测试

## 4. 推荐的初始设置

刚开始时，可以从下面这样的配置入手：

- `Mode > Gauge`：`normal`
- `Mode > Sudden Death`：仅在需要首次 osu!mania OD8 `MISS` 即结束的挑战时打开
- `Mode > Rate`：`1.0x`
- `Mode > Hi-Speed`：先保持默认
- `Graphics > Display`：使用 Discord voice overlay 时推荐 `Borderless`
- `Graphics > Performance HUD`：只在需要时打开
- `Graphics > Display Offset`：从默认 `0ms` 开始
- `Audio > Keysound Mode`：BMS 推荐 `follow`

如果 note 看起来太慢或太快，先调整 `Hi-Speed`；如果判定感觉没问题，但画面看起来偏慢或偏快，则调整 `Display Offset`。

### Discord voice overlay

请在 Discord 的 `User Settings > Game Overlay` 中启用 overlay 和 Voice widget，并将 TenRiff 设为 `Graphics > Display > Borderless` 或 `Windowed`。当前 Discord Game Overlay 不会显示在 DXGI 独占全屏中。为了避免遮挡 gameplay 信息，建议把 Voice widget 固定在左下角，并关闭 TenRiff 的 `Performance HUD`。如果 Discord 没有自动识别 TenRiff，请在 `Registered Games` 中添加正在运行的 `TenRiff.exe`。

Discord 客户端的设置方法请参考[官方 Game Overlay 指南](https://support.discord.com/hc/en-us/articles/217659737-Game-Overlay-101)。

## 5. 默认按键布局

默认 keymap 会根据谱面的键数自动选择。

- `4K`：`D F L ;`
- `5K`：`D F K L ;`
- `6K`：`S D F J K L`
- `7K`：`W E R M I O P`
- `8K`：`W E R V M I O P`
- `9K`：`A S D F Space H J K L`
- `10K`：`Q W E R V M I O P [`
- `16K`：`Q W E R A S D F U I O P J K L ;`

如果这些布局不符合你的习惯，可以在 `Options > Keymap` 中修改。

## 6. 开始游玩前要知道的事

### 谱面格式
- 默认过滤器以 BMS 为中心。
- 如果在选项里启用 osu!mania，就可以索引并游玩 `4K` 到 `10K` 的谱面。

### 加载
- 刚开始歌曲时，可能会显示谱面加载进度。
- 在加载过程中按 `Esc` 可以取消开始并返回 Song Select。

### 倒计时
- 加载结束后，会先看到 `3 / 2 / 1` 倒计时。
- 倒计时期间的输入不会计入得分判定。

## 7. 游玩中的操作

- 谱面按键输入：以当前 keymap 为准
- `Esc`：单人模式打开暂停菜单（继续 / 重新开始 / 退出）；多人模式中止游玩
- `F3`：降低 Hi-Speed
- `F4`：提高 Hi-Speed
- `F5`：大幅降低 Hi-Speed
- `F6`：大幅提高 Hi-Speed
- `F9`：保存当前画面的截图

Hi-Speed 只改变视觉滚动速度，不改变判定时机本身。
Rate 只改变歌曲播放速度和谱面时间轴；在相同 Hi-Speed 下，视觉滚动速度保持不变。

## 8. HUD 的读法

游戏内 HUD 一般会显示以下信息：

- 标题 / 艺术家
- BPM
- 当前 `Rate`
- 当前 `Hi-Speed`
- Gauge 数值与当前 Gauge 类型
- TenRiff 原生 Score
- 按 osu!mania stable OD8 / ScoreV1 换算真实输入 timing 的辅助 `OSU OD8` 分数
- Combo
- 最近判定（`PG / GR / GD / BD / PR`）
- 时间偏差（ms）

如果打开 `Graphics > Performance HUD`，还能看到帧率图、平均 FPS、低 FPS 以及 gameplay timing 调试信息。

## 9. 判定与 Gauge

### 判定
当前默认判定显示使用下列缩写：

- `PG`：Perfect Great
- `GR`：Great
- `GD`：Good
- `BD`：Bad
- `PR`：Poor / Miss

`OSU OD8` 是最高 1,000,000 的辅助比较分数，不会改变 TenRiff 的原生分数、排名或 clear 结果。

### Gauge
可选的基础 gauge 有以下四种：

- `ex_hard`
- `hard`
- `normal`
- `easy`

所选 gauge 会在歌曲开始时固定从 `100%` 起步。

- `ex_hard`：回复低于 Hard，`BAD` / `POOR` 损失更大；到 `0%` 立即 Game Over
- `hard`：到 `0%` 立即 Game Over
- `normal`：到 `0%` 立即 Game Over
- `easy`：到 `0%` 立即 Game Over

游玩过程中不会再发生自动 gauge 降级。
`Sudden Death (1 MISS)` 不是 gauge 类型，而是在首次 osu!mania OD8 对象 `MISS` 时把 gauge 置零并立即结束的规则。仅原生 `BAD` timing 不会触发，空按产生的 `POOR` 也不计入，并且不能与 Practice No-Fail 同时启用。

## 10. Result 画面

游玩结束后，可以在 Result 画面查看以下内容：

- Clear / Game Over 状态
- Rank
- TenRiff 原生 Score
- `OSU OD8` 辅助分数与换算判定统计
- Accuracy
- Max Combo
- PG / GR / GD / BD / PR 统计
- 平均时间偏差与方差
- 最终 Gauge 与 gauge 记录
- 保存的 replay/result 文件名

返回按键如下：

- `Left`：立刻重新开始同一首谱面
- `F1`：有保存 replay 时播放该 replay
- `Enter`：返回 Song Select
- `Esc`：返回 Song Select

## 11. 常见调整

### note 太密、难以读谱
- 提高 `Hi-Speed`
- 需要时再一起调整 `Skins > Note Width / Note Height / Judge Line`

### 判定感觉对，但画面看起来偏慢或偏快
- 调整 `Graphics > Display Offset`
- 正值会让 note 更早被画出来

### BMS 中的键音太大或太小
- 调整 `Audio > Keysound Volume`
- 可以和 BGM 分开调节

### 输入不稳定或怀疑有按键冲突
- 在 `Options > Keymap > NKRO Test` 里同时按多个键检查
- 必要时修改按键布局，避免手型重叠

## 12. 推荐的上手顺序

第一次开始时，下面的顺序最容易适应：

1. 在 `Song Select` 里先选一首简单的歌
2. 在 `Mode` 里确认 `Gauge=Normal`、`Rate=1.0x`
3. 先只调整 `Hi-Speed`
4. 如果还是不顺手，再调整 `Display Offset`
5. 如果手感不舒服，再改 `Keymap`
6. 最后用 `Skins` 调整 note 大小和判定线位置

## 13. 相关文档

- 当前实现状态：[`docs/current-state.zh-CN.md`](current-state.zh-CN.md)
- 配置结构：[`docs/config.zh-CN.md`](config.zh-CN.md)
- 菜单结构：[`docs/menu.zh-CN.md`](menu.zh-CN.md)
- 播放循环/引擎结构：[`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
