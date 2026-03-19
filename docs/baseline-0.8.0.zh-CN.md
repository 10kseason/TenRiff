# TenRiff 0.8.0 基准

这份文档是后续继续推进 TenRiff 工作时应当遵循的 `0.8.0` 基准文档。目的在于快速固定“当前默认值是什么、哪些行为必须保留、下一步工作应在什么范围内叠加”。

## 发布标识
- 基准发布线：`0.8.0`
- 当前主目标：Windows GUI 构建
- 默认产品表面：BMS-first
- 可选扩展表面：`.osu` osu!mania 4K~10K
- 发布基准路径：
  - Windows 包：`Baepoks/TenRiff-0.8.0`
  - 公开源代码包：`opensource-Tenriff-source/TenRiff-0.8.0-source`
- 发布构建的真实来源：`build-dist/Release`

## 产品基底
- 菜单入口由 `MenuApp` + `MenuWindow` 组合承担。
- 默认用户流程为 `Title -> Song Select -> Gameplay -> Result`。
- Song Select 默认具备缓存优先加载、鼠标/键盘混合操作、外部文件夹 drag-and-drop、recent source 重新打开。
- Gameplay 由 `GameSession` 负责谱面加载、输入、HUD snapshot、谱面音频准备与结束边界。
- 渲染基于 D3D11 + Direct2D/DirectWrite，音频基于 WASAPI，输入基于 RawInput 或高轮询率 polling。

## 基准能力
- BMS parser/normalizer/timeline 以面向真实谱面的兼容性增强状态为准。
- BMS explicit compact layout（`#4K`、`#6K`、`#8K`）与 SP compact layout（`5+1 SP`、`7+1 SP`）都已作为基础功能支持。
- BMS long note 以当前实现为准，包含 LN channel、`#LNOBJ`、`#LNMODE 2` charge tail 判定。
- Song indexing 以适合大型曲库的 `safe` 默认 profile 作为基准行为。
- osu!mania 默认是关闭的，但在选项开启时，菜单/运行时支持 4K~10K，这一状态视为当前基线。
- 结果画面、replay/result JSON 导出、按谱面累积的本地记录都已经包含在基础功能中。

## 基准默认值
- 默认判定范围：
  - `BAD = 80ms`
  - `indirect_miss = 215ms`
- gameplay 开始前默认有 `3 / 2 / 1` 倒计时。
- 游戏内 Hi-Speed 保持 `F3/F4` 负责细调、`F5/F6` 负责粗调。
- `F3/F4` 长按时会连续生效，且游玩中调整的 HS 会保存回 profile，这视为当前默认行为。
- gameplay 结束后到结果画面的 tail 以 `ui.result_tail_ms = 3000ms` 为准。
- future note 会从 playfield 上方自然进入，note 渲染会在 playfield 内裁切，这一表现视为当前基准。

## 基准 UX
- Song Select 左侧按钮列保持 `LEVEL / TITLE / SOURCES / KEY / BROWSE / MOD / RECORDS` 结构。
- Song Select 的 `LEVEL`、`TITLE`、`KEY` 是主选歌界面可直接访问的快速筛选/排序轴。
- 左侧按钮列和主要菜单按钮应当可以通过鼠标左键立即选择/执行。
- 鼠标右键应当提供从当前按钮列回退到上一个按钮的辅助 UX，这也是当前基准。

## 基准约束
- 优先做局部修正，不要重设计已经稳定运行的路径。
- Linux 仍然只算 preview 级别，判断基准时以 Windows GUI 路径为准。
- 较旧的设计文档不优于当前代码和 [`docs/current-state.zh-CN.md`](current-state.zh-CN.md)。
- 后续工作应优先采用不破坏 `0.8.0` 基线的增量式修改。

## 后续工作中应保留的内容
- BMS-first 表面
- large-library safe indexing 的稳定性
- 菜单缓存优先加载
- 游戏玩法 HUD / 音频 / 输入分离结构
- 当前结果导出 / 本地记录路径
- UTF-8/Korean path 兼容
- `0.8.0` 打包规则与 no-songs 发布配置

## 推荐配套文档
- 当前实际实现状态：[`docs/current-state.zh-CN.md`](current-state.zh-CN.md)
- 配置/ profile 结构：[`docs/config.zh-CN.md`](config.zh-CN.md)
- 菜单/状态流：[`docs/menu.zh-CN.md`](menu.zh-CN.md)
- 播放循环/音频/输入边界：[`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
