# TenRiff 当前状态

这份文档是下一位 agent 或新任务接手时应该最先阅读的当前状态文档。目标是快速说明“这个项目现在是什么、应该先看哪里、还有哪些内容尚未验证”。

## 基线
- 当前项目版本为 `1.1.2`
- 后续工作的基准文档是 [`docs/baseline-1.0.0.zh-CN.md`](baseline-1.0.0.zh-CN.md)
- Windows GUI 构建是主目标
- Linux 仅存在 [`Baepoks-Linuxs/TenRiff-0.5.0-linux-preview`](../Baepoks-Linuxs/TenRiff-0.5.0-linux-preview) 级别的 preview
- 默认表面是 BMS-first
- `.osu` 可以通过选项重新启用，并支持 4K~10K
- `1.1.2` 发布线保留了 `1.0.9` 基于真实 playback head 的 gameplay 输入时序修正，但为了稳定性把 live gameplay 输入采集固定到 `Polling`，同时保留 gameplay 会话不受 foreground 限制的 always-allow gate
- 同一条 `1.1.2` 发布线里，menu 输入仍然保持 foreground process/root-window 边界，而 restart 风格的 backend fallback 与 profile 输入后端持久化改写继续被移除，保存的 backend 偏好只作为配置保留

## 核心架构
- `MenuApp`
  - 菜单状态机的中心
  - 负责 Song Select、Options、Keymap、Result、Gameplay 启动入口的管理
  - 最近的维护性重构已把 Song Select 的 record/keymap/render/state 边界拆到专用 `.cpp` 文件中
  - 现在即使没有本地 `10k-calc` Python 参考实现，开源源码包也能通过 skip optional reference test 来运行核心测试集
- `SongIndexerThread`
  - 专用于曲目索引的后台线程
  - 向 Song Select 发送进度
- `AudioThread`
  - 负责音频主时钟与混音
- `InputThread`
  - 负责收集 RawInput/polling 输入并投递到队列
- `RenderThread` + `MenuWindow`
  - 基于 D3D11 + Direct2D/DirectWrite 的菜单/游戏中 HUD 渲染
  - 最近的维护型重构正在朝着把大型实现文件拆成碎片文件的方向整理
- `GameSession`
  - 负责谱面加载、gameplay audio prep、HUD snapshot、Gameplay 执行边界

## 当前可用功能
- BMS parser/normalizer/timeline 已增强到适合真实谱面兼容的程度
- BMS explicit key headers：
  - `#4K`
  - `#6K`
  - `#8K`
  - `5+1 SP`
  - `7+1 SP`
  - 当存在 header 或检测到 SP pattern 时，会按照该键数应用 compact lane mapping
- BMS keysound：
  - `follow`
  - `autoplay`
  - `ignore`
- BMS long note：
  - LN channel（`51`-`55`, `61`-`65`）
  - `#LNOBJ`
  - `#LNMODE 2` charge note 会使用 tail release timing 判定
  - 普通 BMS LN 保持到最后时会自动处理 tail，不使用 tail release timing 判定
- BMS audio decode：
  - WAV 原生优先
  - Windows Media Foundation OGG/MP3 fallback
  - MF 失败时使用 `ffmpeg.exe` fallback
- Song Select：
  - 缓存优先加载
  - `F5` 强制重索引
  - 鼠标滚轮移动
  - 左侧 `KEY` 快速过滤切换
  - 外部文件夹/BMS drag-and-drop
  - recent source 保存/重新打开
  - BMS / OSU / All 过滤
  - difficulty/title 排序
- osu!mania：
  - 4K~10K 读取/运行
  - 按 key mode 分开的 keymap
  - 4K~10K chart difficulty 计算
  - `mode.key_mode` 通过 N2NC 风格的 lane remap 进行键数变换
  - `mode.key_mode=none` 表示保持谱面的原始键数与基础 pattern 布局不变
- Skins / Gameplay feel：
  - `rect` / `circle` note shape
  - note border 开关
  - combo Y 调整
  - judge line / lane width / lane spacing / note width / divider width / 16K center gap / note height 调整
  - 会按 key mode 保存单独的 lane 宽度数组和 lane 间距数组，并在 preview、实际 gameplay、ghost field 中共用同一套布局计算
  - 读取 osu!mania `ColumnLineWidth` 并反映到 lane divider 宽度
  - `skin.lr2_resolution_mode` 以 `auto / sd / hd / fhd` 保存 LR2 playskin 的分辨率 override token
  - LR2 auto-detect 以 playskin `#DST_NOTE` 的坐标范围而不是 asset 名称来判断 SD/HD/FHD family
  - future note 的上方进入 easing
  - 最后一个判定 note 处理完后立即结束 gameplay
- Judge：
  - 默认 `GOOD` 范围为 `75ms`
  - 默认 `BAD` 范围为 `340ms`
  - 会真正消耗 note 的失败（auto-miss、过早吃掉 note、hold break / tail miss）仍然记为 `BAD`
  - 非消耗型的超早输入会按 LR2 风格记为 `POOR`，并重新出现在结果 / replay / UI 中
  - `POOR` 不会断 combo，不计入 score / accuracy 总数，并使用独立的 `PR` gauge 损失值
  - live gameplay 输入会直接使用 `ClockSync` 的估计结果，stale backlog 压缩也重新按 `BAD` 窗口执行，以匹配 `0.999` 的输入边界
  - tail release timing 仅适用于 osu hold 与 BMS `#LNMODE 2` charge note
  - 当两把键盘同时按住同一个键时，逻辑 `Pressed` 状态会一直保持到最后一个输入源释放为止
- Graphics：
  - 分辨率预设（`720p`、`1080p`、`qhd`、`native`）
  - `refresh_hz`（`60..1050`，默认 `300`）
  - VSync off：menu 有效上限 `300`，gameplay 最多使用配置值到 `1050`
  - VSync on：present refresh 跟随活动显示器 Hz，render pacing 目标为 `monitor_hz * 2`（上限 `1050`）
  - `visual_offset_ms`
  - `performance_overlay`
- Gameplay performance：
  - static playfield command-list cache
  - note head/tail bitmap cache
  - fixed-size HUD note transport
- Loading UX：
  - Song Select indexing progress
  - gameplay chart loading progress
  - gameplay loading 时 `Esc` 取消

## 曲目索引模型
- 在切换 song source 时，会优先读取 profile-local 的 `profiles/<name>/.tenriff/song-index/<source-hash>.json` 缓存
- 如果缓存不存在或无效，就会启动后台索引
- indexing profile：
  - `safe` 是默认值
  - `fast` 是可选值
  - 由 Mode Settings 的 `Indexing` row 和 `config.mode.song_index_profile` 控制
- 索引阶段：
  - `SCANNING FILES`
  - `BUILDING METADATA`
  - `WRITING CACHE`
- 面向大型曲库的内存加固：
  - 2-pass enumerate + small batch metadata build
  - `safe` profile 在大规模扫描时优先使用以 1-worker 为中心的 budget 和更频繁的 heap trim 来控制 RAM high-water
  - 用于索引的 BMS parse 采用低内存路径，跳过 asset map/不必要的 header/非必需 command
  - cache save 使用 streaming write，而不是生成巨大的 JSON tree
- 实测：
  - `D:\Stellaverse (2025-12-14)` 的 safe full-index 基准为 `46,636` 个 candidate / `46,602` 个 indexed entries
  - 峰值内存大约为 `working set 453MB`、`private 524MB`
  - 同一库 1024-chart sample 下，fast profile 吞吐量约为 safe 的 `2.05x`
- cache schema：
  - `version = 8`
  - 包含 `include_osu`
  - 可选包含 `layout_label`

## 运行时 / 打包规则
- 新用户 profile 会自动创建
- 最近一次 staged 的发布包是 `Baepoks/TenRiff-1.1.2`
- 发布包不包含 `Songs`
- 发布包会同时包含用于菜单 BGM 的 `Mainmusic/` 运行时资源
- 发布更新时，只把已构建的产物放进 `Baepoks/`
- 如果请求 source-only/public handoff，用户偏好是先写 include/exclude 列表
- 最近一次 staged 的公开源代码包会像 `opensource-Tenriff-source/TenRiff-1.1.2-source` 这样按版本单独 staging
- 刷新公开源代码包时，不能只同步文档和文件本身，还要确认 staged 出来的源码包目录可以独立完成 configure/build，并能直接运行核心测试二进制

## 配置 / Profile 现实情况
- 实际默认值来自 `config/config.json`
- runtime profile 位于 `profiles/<name>/config.json`
- keymap 位于 `profiles/<name>/keymap.json`
- `keymap.json` 采用 `modes.{4k..10k}` 的 per-mode binding 结构
- stale profile 会通过 runtime migration 自动修正部分值
  - BMS-first default
  - keysound policy
  - osu key-mode mismatch 等

## 高价值文件
- `src/app/MenuApp.cpp`
- `src/app/GameSession.cpp`
- `src/app/SongIndex.cpp`
- `src/app/SongIndexerThread.cpp`
- `src/render/MenuWindow.cpp`
- `src/render/RenderThread.cpp`
- `src/app/ChartLoader.cpp`
- `src/chart/BmsParser.cpp`
- `src/config/Config.*`
- `src/config/Keymap.*`

## 已验证命令
- `cmake --build build --config Release --target tenriff`
- `cmake --build build --config Release --target bms_parser_tests`
- `cmake --build build --config Release --target bms_realworld_smoke`
- `ctest --test-dir build -C Release --output-on-failure -R bms_parser_tests`
- `cmake -S opensource-Tenriff-source/TenRiff-1.1.2-source -B opensource-Tenriff-source/TenRiff-1.1.2-source/build-check -G "Visual Studio 17 2022" -A x64`
- `cmake --build opensource-Tenriff-source/TenRiff-1.1.2-source/build-check --config Release --target bms_parser_tests`
- `opensource-Tenriff-source/TenRiff-1.1.2-source/build-check/Release/bms_parser_tests.exe`

## 仍然需要手动验证的地方
- 在真实的 CJK-heavy 曲库上重现 Song Select 快速滚动崩溃
- 对 fast profile 进行长时间 full-index RAM/commit 重新验证
- gameplay 的 low-FPS / 0.1% / 0.01% low 确认
- OBS/Discord/Game Bar 与 graphics live-apply 的共存确认
- drag-and-drop / 外部 Korean-path sources 的 GUI 确认
- 4K~10K `.osu` 在实机上的 keymap 分离确认
- Linux 目前仍不是真实可运行版本

## 最适合接着看的文档
- runtime/config 相关：[`docs/config.zh-CN.md`](config.zh-CN.md)
- 菜单/索引/状态机：[`docs/menu.zh-CN.md`](menu.zh-CN.md)
- 播放循环/音频/判定：[`docs/core-loop.zh-CN.md`](core-loop.zh-CN.md)
