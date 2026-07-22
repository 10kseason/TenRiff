# TenRiff 当前状态

这份文档是下一位 agent 或新任务接手时应该最先阅读的当前状态文档。目标是快速说明“这个项目现在是什么、应该先看哪里、还有哪些内容尚未验证”。

## 基线
- 当前项目版本为 `1.1.6 stable`
- direct-IP multiplayer 与 preview r5 的输入 backend 生命周期修复已整合进 `1.1.6 stable`
- `1.1.6` 包含 Ghost Battle 默认 `OFF`、安全 OSK/OSZ 安装、更完整的 osu!mania skin 应用、0～100% 判定线、LN 显示与难度评估改进，以及 Mirror 模式
- 后续工作的基准文档是 [`docs/baseline-1.1.2.zh-CN.md`](baseline-1.1.2.zh-CN.md)
- Windows GUI 构建是主目标
- Linux 仅存在 [`Baepoks-Linuxs/TenRiff-0.5.0-linux-preview`](../Baepoks-Linuxs/TenRiff-0.5.0-linux-preview) 级别的 preview
- 默认表面是 BMS-first
- `.osu` 可以通过选项重新启用，并支持 4K~10K
- `1.1.6 stable` 的 gameplay 输入优先使用 RawInput，同时在同一 `InputThread` 中持续运行 bound-key polling shadow；启动失败或 message pump 意外退出时，会在不重置 queue/pressed state 的情况下把该 producer 切换到 Polling
- menu 输入保持 foreground process/root-window 边界。检测到 RawInput 启动失败、process-global 注册目标丢失或 hidden message window 退出时，无需等待用户按键即可切换到 Polling。
- 已确认的 fallback 不会改写 profile，并在本次应用运行期间持续用于 menu 与后续 gameplay；重启应用或明确更改 `Options -> Input Settings -> Backend` 后才会重试。

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
  - gameplay 在单一 `InputThread` state tracker 中对 RawInput 与 bound-key polling shadow 去重，`GameSession` 不再按 source 二次过滤 logical edge
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
  - 可通过 `Shift+F2` 文件选择或 drag-and-drop 将 `.osz` 安装到当前 songs source；安装后会启用 osu chart 并重新索引该 source
  - OSZ 安装会先预检整个 archive，再通过 staging 解压并原子提交，不会覆盖已有文件夹，并将 `.osu` 的 background/audio/hitsound 引用限制在 chart 目录内
  - recent source 保存/重新打开
  - BMS / OSU / All 过滤
  - difficulty/title 排序
- osu!mania：
  - 4K~10K 读取/运行
  - 按 key mode 分开的 keymap
  - 4K~10K chart difficulty 计算
  - `mode.key_mode` 通过 N2NC 风格的 lane remap 进行键数变换
  - `mode.key_mode=none` 表示保持谱面的原始键数与基础 pattern 布局不变
- Native difficulty：
  - BMS/osu!mania 的 LV/CR 计算仅将 long-note Head/Tail 的 miss-ms 按 0.5倍评估，使 `300ms`按`150ms`处理；实际 gameplay 判定范围保持不变
- Lane transform：
  - Random 支持 `Off / Mirror / FR / SR`；Mirror 在 key-mode 变换后反转最终 lane，10K/16K 则在每个 player half 内独立反转
- Skins / Gameplay feel：
  - `rect` / `circle` note shape
  - note border 开关
  - combo Y 调整
  - judge line / lane width / lane spacing / note width / divider width / 16K center gap / note height 调整
  - 会按 key mode 保存单独的 lane 宽度数组和 lane 间距数组，并在 preview、实际 gameplay、ghost field 中共用同一套布局计算
  - 可在 Skins 页面通过文件选择或 drag-and-drop 将 `.osk` 安装到当前 profile 的 `skins`，并使用与 OSZ 相同的 transactional/no-overwrite 策略
  - 将受支持的 osu!mania note/LN 图片以及 `ColumnWidth`、`ColumnSpacing`、`ColumnLineWidth`、`HitPosition` 应用到 gameplay 布局
  - archive 中所有有效文件都会保留，但 TenRiff 不会对尚未支持的 osu! mode 或 UI asset 做 pixel-perfect 还原
  - `skin.lr2_resolution_mode` 以 `auto / sd / hd / fhd` 保存 LR2 playskin 的分辨率 override token
  - LR2 auto-detect 以 playskin `#DST_NOTE` 的坐标范围而不是 asset 名称来判断 SD/HD/FHD family
  - future note 的上方进入 easing
  - 最后一个判定 note 处理完后立即结束 gameplay
- Judge：
  - 默认 `GOOD` 范围为 `75ms`
  - 默认 `BAD` 范围为 `340ms`
  - 当同一 lane 的 pending note 已经是 `BAD`，而紧接的下一 note 明确可判为 `GOOD` 或更高时，前一 note 会记为 miss，当前按键则分配给下一 note，避免一次漏键锁成连续 `BAD`
  - 会真正消耗 note 的失败（auto-miss、过早吃掉 note、hold break / tail miss）仍然记为 `BAD`
  - 非消耗型的超早输入会按 LR2 风格记为 `POOR`，并重新出现在结果 / replay / UI 中
  - `POOR` 不会断 combo，不计入 score / accuracy 总数，并使用独立的 `PR` gauge 损失值
  - gauge 模式支持 `EX-Hard / Hard / Normal / Easy`，全部从 `100%` 开始，并在 `0%` 时立即失败
  - live gameplay 的 `ClockSync` 使用 centered anchor regression，避免大型 Windows QPC 绝对值造成精度损失，并在持续 clock discontinuity 后自动 rebase
  - stale backlog 按 QPC event age 与 `BAD` window 判定；若 fresh input 的 sample mapping 与当前 playback anchor 偏差过大，则 fallback 到 anchor
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
- Profile UX：
  - 可从 `Options -> Profile Setup` 重新打开当前 profile 的首次设置页面，并立即保存 language/audio/input/graphics/keymap
- Direct-IP multiplayer：
  - joiner 只在 active source 和 `recent_song_sources` 的现有 profile-local cache 中按 host chart 的 hash + size 查找
  - 不进行全盘扫描或自动重扫，并拒绝 cache 中指向 source root 外部的路径

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
  - 46k-chart Windows benchmark library 的 safe full-index 基准为 `46,636` 个 candidate / `46,602` 个 indexed entries
  - 峰值内存大约为 `working set 453MB`、`private 524MB`
  - 同一库 1024-chart sample 下，fast profile 吞吐量约为 safe 的 `2.05x`
- cache schema：
  - `version = 10`
  - 包含 `include_osu`
  - 可选包含 `layout_label`

## 运行时 / 打包规则
- 新用户 profile 会自动创建
- 当前正式 P2P 发布线为 `TenRiff 1.1.6 stable`
- 发布包不包含 `Songs`
- 发布包会同时包含用于菜单 BGM 的 `Mainmusic/` 运行时资源
- 发布更新只包含已构建产物和必要的运行时资源
- source-only/public handoff 前先确认 include/exclude 列表
- preview source branch 和 tag 与 stable release 分开管理
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
- `cmake -S . -B build-check -G "Visual Studio 17 2022" -A x64`
- `cmake --build build-check --config Release --target bms_parser_tests`
- `.\build-check\Release\bms_parser_tests.exe`

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
