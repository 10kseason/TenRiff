# TenRiff

Language: [한국어](README.md) | [English](README.en.md) | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

TenRiff 是一个以 BMS-first 为核心的 Windows GUI 节奏游戏运行时/启动器项目。它的目标是构建一个可独立运行的节奏游戏客户端，并直接控制判定、音频、输入和渲染管线，以提供可实际游玩的 BMS 环境。当前项目版本为 `1.1.3`；multiplayer preview r5 会作为独立 prerelease 发布，不覆盖现有的 `1.1.3 stable` 版本。项目整体使用 MIT 许可证，随项目分发的第三方组件说明见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

这份 README 是面向首次阅读者的入门文档。关于当前实际行为、当前 `1.1.3` 项目状态、`1.1.2 final stable` 基准、配置结构以及设计文档，请继续阅读 [`docs/README.zh-CN.md`](docs/README.zh-CN.md)。

TenRiff 也明确属于一种 `vibe coding` 作品：它更多是在快速迭代和实验中成形，而不是只按照传统的长篇设计先行流程推进。

## 项目一览

- 主要目标平台：Windows
- 主要谱面表面：BMS-first
- 可选支持谱面：`.osu` osu!mania 4K-10K
- 图形路径：D3D11 + Direct2D/DirectWrite
- 音频路径：WASAPI
- 输入路径：RawInput 或高轮询率键盘 polling
- 许可证：[MIT](LICENSE)
- 第三方说明：[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- 发布历史：[`CHANGELOG.md`](CHANGELOG.md)

## Credits / Attribution

TenRiff 当前的键位模式转换器包含基于 `krrcream-Toolkit` 中 N2NC 思路与代码的适配/移植实现。

- 原始项目：<https://github.com/krrcream/krrcream-Toolkit>
- 引入范围：基于 `Tools/N2NC/N2NC.cs` 的键位转换逻辑，并适配到 TenRiff 的 C++ `GameplayChart` 运行时模型
- 当前 TenRiff 实现位置：`src/gameplay/KeyModeConverter.*`
- 许可证/来源说明：[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)

对于这部分适配逻辑，TenRiff 会明确保留对 `krrcream` 及原始工具项目的致谢，同时单独说明 TenRiff 侧的整合与修改内容。

### 致谢

感谢 OpenAI Codex、ChatGPT、Claude Code、Gemini，以及帮助一起验证项目的各位 guest testers。

## 当前已实现内容

当前代码库已经达到“菜单可打开、可选歌、可加载并游玩谱面、可保存结果与本地记录”的阶段。

- BMS 解析/归一化/时间线处理
  - Header、字典和小节命令
  - 分数型 `#MEASURE`
  - 对 `#4K / #6K / #8K` header 进行 compact lane mapping
  - `#LNOBJ` 与 LN 通道（`51`-`55`, `61`-`65`）
  - 通过 CP932（Shift-JIS）回退兼容旧式 BMS 文本
- BMS 音频
  - 原生 WAV 解码
  - 优先使用原生 OGG Vorbis 解码（`stb_vorbis`），必要时回退到 Windows Media Foundation
  - MP3 通过 Windows Media Foundation 回退
  - 必要时可使用 `ffmpeg.exe` 回退
  - keysound 模式：`follow / autoplay / ignore`
- Song Select
  - 缓存优先加载
  - 通过 `F5` 强制重新索引
  - 搜索、键数过滤、难度过滤
  - `LV ASC/DESC`、`TITLE A-Z/Z-A` 排序
  - 外部文件夹/BMS 拖放
  - recent source 保存与重新打开
  - `BMS / OSU / All` 过滤
- Gameplay / HUD
  - 实时 HUD
  - 分阶段的谱面加载进度
  - 在 gameplay 加载过程中使用 `Esc` 取消
  - display offset
  - performance overlay
  - note head/tail 位图缓存与静态 playfield command-list 缓存
- 选项/皮肤
  - Hi-Speed、Rate、gauge、audio、input、graphics 设置
  - `Skins` 画面中可调判定线位置、note 宽度/高度
  - `5K~10K` lane color 编辑与实时预览
- 结果/本地记录
  - 专用结果画面
  - replay/result JSON 导出
  - 按谱面累计本地历史记录
  - best record 选择时优先 clear 状态

## 当前限制

项目已经可以使用，但还不是完全完工的产品。

- Windows GUI 是当前主要支持路径。
- Linux GUI/audio/input 后端尚未完成。
- 一部分 GUI 流程目前主要通过构建/测试验证，仍然需要更多实际运行下的手动验证。
- 较旧的设计文档可能与当前实现不完全一致，因此要确认当前行为时，应优先查看 [`docs/current-state.zh-CN.md`](docs/current-state.zh-CN.md)。

## 快速开始

### 1. 仓库结构

通常主要关注以下目录：

- `src/`：运行时与游戏代码
- `tests/`：单元测试与 smoke 测试
- `docs/`：当前状态与设计文档
- `config/`：默认全局配置
- `profiles/`：运行时 profile 配置、键位映射和本地结果
- `songs/`：谱面根目录

### 2. Release 构建

Windows 下的典型构建命令：

```powershell
cmake -S . -B build-dist -G "Visual Studio 17 2022" -A x64
cmake --build build-dist --config Release --target tenriff
cmake --build build-dist --config Release --target bms_parser_tests
```

如果 Windows Defender 或其他杀毒软件会暂时锁住 `TenRiff.exe`，可以改用下面的重试包装脚本。

```powershell
.\tools\build_with_retry.ps1 -BuildDir build-dist -Config Release -Targets tenriff,bms_parser_tests
```

### 3. 公开源代码包也可以直接构建

按版本发布的公开源代码包（例如 `TenRiff-1.1.3-source.zip`）已经包含 `external/`（但不含 `external/llama.cpp/`）、`src/`、`tests/`、`config/`、`docs/` 和 `Mainmusic/`，因此解压后就可以直接进行 configure/build。

- 源代码包中不包含 `tools/build_with_retry.ps1`，所以这里应使用原生 `cmake --build`。
- `10k-calc/` 会从公开源代码包中排除，因此依赖 Python reference 的 optional 检查即使输出 `[skip]` 也属于正常情况。
- `external/llama.cpp/` 也会从公开源代码包中排除，因此本地 LLM/tooling checkout 需要自行另外准备。
- `profiles/`、`songs/`、`logs/` 也不会放进源代码包，但 `launch_win.bat` 会在首次启动时自动创建所需目录。

在解压后的源代码包目录中，典型流程如下：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target tenriff
cmake --build build --config Release --target bms_parser_tests
.\build\Release\bms_parser_tests.exe
```

### 4. 运行测试

```powershell
.\build-dist\Release\bms_parser_tests.exe
```

### 5. 启动

直接启动：

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

使用启动脚本：

```powershell
.\launch_win.bat
```

## 配置与运行时数据

TenRiff 将全局配置与 profile 配置分开管理。

- 全局配置：`config/config.json`
- Profile 配置：`profiles/<name>/config.json`
- 键位映射：`profiles/<name>/keymap.json`
- 曲库索引缓存：`profiles/<name>/.tenriff/song-index/<source-hash>.json`
- Replay 导出：`profiles/<name>/replays/*.json`
- Result 导出：`profiles/<name>/results/*.json`
- 运行日志：`logs/run.log`
- 崩溃日志：`logs/crash-*.log`

如果想快速了解实际配置结构，最适合先看 [`docs/config.zh-CN.md`](docs/config.zh-CN.md)。

## 文档阅读顺序

README 只负责入门说明。更详细的内容建议按以下顺序阅读：

1. [`docs/README.zh-CN.md`](docs/README.zh-CN.md)
   - 文档总索引
2. [`docs/current-state.zh-CN.md`](docs/current-state.zh-CN.md)
   - 当前实际可用功能
3. [`docs/baseline-1.1.2.zh-CN.md`](docs/baseline-1.1.2.zh-CN.md)
   - 后续工作应保持的 `1.1.2 final stable` 基准文档
4. [`docs/gameplay-guide.zh-CN.md`](docs/gameplay-guide.zh-CN.md)
   - 实际开始游玩、基本操作、HUD、判定、结果说明
5. [`docs/config.zh-CN.md`](docs/config.zh-CN.md)
   - 配置、profile 与 keymap 结构
6. [`docs/menu.zh-CN.md`](docs/menu.zh-CN.md)
   - 菜单、状态机与选歌流程
7. [`docs/core-loop.zh-CN.md`](docs/core-loop.zh-CN.md)
   - Play loop 与数据流
8. [`docs/roadmap.zh-CN.md`](docs/roadmap.zh-CN.md)
   - 中长期方向

## 文档解释规则

设计文档与当前代码可能不一致。出现冲突时，优先级如下：

1. 当前代码
2. [`docs/current-state.zh-CN.md`](docs/current-state.zh-CN.md)
3. [`docs/config.zh-CN.md`](docs/config.zh-CN.md)
4. 较旧的设计文档

也就是说，判断“当前行为”时，应优先参考当前状态文档，而不是较旧的设计草案。

## 下一步建议阅读

- 想看实际游玩说明：[`docs/gameplay-guide.zh-CN.md`](docs/gameplay-guide.zh-CN.md)
- 想看配置细节：[`docs/config.zh-CN.md`](docs/config.zh-CN.md)
- 想看菜单流程：[`docs/menu.zh-CN.md`](docs/menu.zh-CN.md)
- 想看 play loop 细节：[`docs/core-loop.zh-CN.md`](docs/core-loop.zh-CN.md)
- 想快速了解整体状态：[`docs/current-state.zh-CN.md`](docs/current-state.zh-CN.md)
