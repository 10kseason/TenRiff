# TenRiff 1.1.2 Final Stable 基准

这份文档定义了今后 TenRiff 工作应继续叠加的 `1.1.2 final stable` 基准。目标是明确当前稳定契约、必须保留的行为，以及后续工作不应越界破坏的范围。

## Release Identity
- 基准发布线：`1.1.2`
- 发布名称：`final stable`
- 当前主要目标：Windows GUI 构建
- 默认产品表面：BMS-first
- 可选扩展表面：`.osu` osu!mania 4K-10K
- 打包路径：
  - Windows 包：`Baepoks/TenRiff-1.1.2`
  - 公开源代码包：`opensource-Tenriff-source/TenRiff-1.1.2-source`
- 打包构建来源：`build-dist/Release`

## 稳定契约
- 保留 `1.0.9` 中基于真实 playback head 的 gameplay 输入时序修正。
- gameplay live 输入采集继续为稳定性固定到 `Polling`。
- gameplay 会话继续保持不受 foreground 限制的 always-allow 输入 gate。
- menu 输入继续使用 foreground process/root-window 边界。
- 已保存的 `input.backend` / `input.rawinput` 不再因为 runtime fallback 被重写，应原样保留。
- Windows 发布包继续附带 `Mainmusic/`，确保菜单 BGM 开箱可用。
- 公开源代码包继续排除 `external/llama.cpp/`。

## 产品基础
- 菜单入口由 `MenuApp` 与 `MenuWindow` 组成。
- 默认用户流程为 `Title -> Song Select -> Gameplay -> Result`。
- Song Select 保留缓存优先加载、搜索/排序/过滤、外部文件夹拖放，以及 recent source 重新打开。
- `GameSession` 负责谱面加载、gameplay audio prep、HUD snapshot 与 gameplay 结束边界。
- 渲染基于 D3D11 + Direct2D/DirectWrite，音频基于 WASAPI，输入基于 RawInput 或高频 polling。

## 基准能力
- BMS parser/normalizer/timeline 管线应视为当前面向真实曲包兼容性的基准实现。
- BMS 显式 compact 布局（`#4K`、`#6K`、`#8K`）与 SP compact 布局（`5+1 SP`、`7+1 SP`）属于基准功能。
- BMS 长按支持包括 LN 通道、`#LNOBJ` 与 `#LNMODE 2` charge tail 判定。
- BMS 音频支持包含原生 WAV、OGG/MP3 fallback、`ffmpeg.exe` fallback 与 `follow/autoplay/ignore` keysound 模式。
- Song indexing 保留默认 `safe` 与可选 `fast` 两个 profile，但大曲库稳定性以 `safe` 为准。
- osu!mania 默认关闭，但在菜单/运行时中仍是可选支持的 4K-10K 表面。
- 结果画面、replay/result JSON 导出、本地记录以及 ghost 比较流程都属于基准功能。

## 基准默认值
- 默认判定窗：
  - `GOOD = 75ms`
  - `BAD = 340ms`
  - 当前运行时里 `indirect_miss` 会折叠为与 `BAD` 相同的值。
- 长按尾部默认值：
  - `hold_grace = 80ms`
  - `hold_break = 200ms`
- gameplay 前的 `3 / 2 / 1` 倒计时属于基准行为。
- 结果画面停留时间使用 `ui.result_tail_ms = 3000ms`。
- 三种 gauge（`Hard`、`Normal`、`Easy`）都从 `100%` 开始，并在 `0%` 立即失败。
- 自动 gauge shift 不属于当前基准。
- song index 默认 profile 为 `safe`。

## 基准 UX 与打包
- Song Select 左侧导航、搜索、排序、键数/谱面过滤、分页与鼠标滚轮导航属于当前 UX 契约的一部分。
- `F5` 重新索引、`F1` 帮助、`F9` 截图等公共快捷键继续保留。
- `Skins` 中用于调整判定线、note 大小、lane spacing 与 lane color 的现有流程应继续保留。
- 发布包继续采用 no-songs 结构。
- 新用户 profile 继续在首次启动时自动创建。

## 基准约束
- 后续变更应优先采用不破坏 `1.1.2 final stable` 契约的增量式修改。
- Linux 仍然只是 preview 级别，基准判断以 Windows GUI 路径为准。
- 当前代码与 `docs/current-state.md` 优先于旧设计文档。
- 只要 release/docs/packaging 规则变更，就应同步更新 baseline 与 current-state 文档。

## 后续工作必须保留的内容
- 基于 playback head 的 gameplay 输入时序修正
- `Polling` live capture 与已保存 backend 偏好分离保留的策略
- BMS-first 表面
- 大曲库 `safe` indexing 稳定性
- 菜单缓存优先加载
- 附带 `Mainmusic/` 的 no-songs Windows 打包
- 公开源代码包排除 `external/llama.cpp/` 的规则

## 建议搭配阅读的文档
- 当前实现状态：`docs/current-state.zh-CN.md`
- 配置/profile 结构：`docs/config.zh-CN.md`
- 实际游玩流程：`docs/gameplay-guide.zh-CN.md`
- 维护/扩展地图：`docs/developer-extension-guide.zh-CN.md`
- 中长期方向：`docs/roadmap.zh-CN.md`
