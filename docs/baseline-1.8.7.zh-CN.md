# TenRiff 1.8.7 Fixed Stable Baseline

Language: [Korean](baseline-1.8.7.md) | [English](baseline-1.8.7.en.md) | 简体中文 | [日本語](baseline-1.8.7.ja.md)

2026-10-10固定的后续工作基准为 **1.8.7**。此前的 `baseline-*` 文档是对应版本的历史记录，当前依据是本文、[当前状态](current-state.zh-CN.md)，以及固定提交中的代码和回归测试。固定基准不代表停止开发，而是比较与兼容性判断的起点不会自动改变。

## Release Identity

- 固定Git tag：`1.8.7`
- 固定源码提交：`6b060ad47b950ad9e0dbc516fbb40189e8e21347`（PR #81合并）
- 发布日期：2026-10-09。基准文档固定日期：2026-10-10。
- [指定版本的发布页](https://github.com/10kseason/TenRiff/releases/tag/1.8.7)。`latest`、不断变化的分支HEAD和Web部署版本都不是本基准的标识。
- 基准平台：Windows GUI。产品谱面范围：BMS系列（`.bms/.bme/.bml/.pms`）。
- 以下是现有公开资产的SHA-256。不得认为后续文档修改已包含在这些ZIP中，也不得替换tag、ZIP或校验和文件。

| 资产 | SHA-256 |
|---|---|
| `TenRiff-1.8.7.zip` | `270ace26dd912228083314ccc99bb27f4e6fd1e3d617f45cffc3f82bb46d3476` |
| `TenRiff-1.8.7-source.zip` | `4c22c89827e6c460ed951c178c0371355636ee6dd2836dece52b028aeeca5ad1` |
| `TenRiff-1.8.7-SHA256SUMS.txt` | `23ff5a092690fb40b5c486eda931db47dc034f5f2328de14ca9987e26a11be99` |

## Product And Packaging Contract

- 默认流程为 `Title -> Song Select -> Gameplay -> Result`。无论在线服务是否失败，都保留本地记录。
- 保留BMS解析、采样时间轴、变速滚动、长音符、地雷、键音和确定性回放复核。外部 `.osu` 谱面不属于当前产品范围。
- NK3以P64和host beam安全求解器为权威路径。通用模式MLP仅在非10K原谱转换为10K时使用。
- 支持native、TenRiff `skin.json` 和LR2皮肤。完整皮肤放在 `skins/`，最小模板放在 `examples/skins/TenRiff-Example`；同名的配置档皮肤优先于随包版本。
- 可运行ZIP包含 `Mainmusic/`、内置皮肤、NK3运行时和模型。不包含Songs、BGA超分辨率模型或standalone BMS key converter可执行文件。外部超分辨率功能默认OFF，单独选择模型不会启用它。
- 公开源码涵盖客户端、verifier和构建资料。服务器实现、部署秘密、账号、用户配置档/日志/回放/结果和上传密钥均不在分发范围内。遵循[源码包边界](../README_SOURCE_PACKAGE.md)。

## Input, Audio And Rules Contract

- 输入时间以audio playback head为基准。正向输入校准不会随回调进度缩小；WASAPI位置采样与观测该位置时的QPC配对使用。保留ASIO fallback。
- 区分已保存的RawInput/Polling设置与运行时fallback，不用fallback结果覆盖已保存的backend。支持左右Shift及主/辅助按键；在相同键数模式内重新分配按键时，将其从原槽位移出。明确清空的主键在保存后仍保持未分配。
- 默认输出为WASAPI，ASIO为可选功能。在谱面解析和采样率选择后打开游戏音频，仅将加载画面限制为60FPS。游玩开始后恢复配置的帧行为。
- 实时键音立即响应实际输入。回放只保存校准后的采样位置，因此不保证实时键音与回放键音的时刻始终相同。
- 歌曲结束方式为 `audio_ui.play_to_end=true`（默认，听到结尾）或 `false`（跳过尾奏）。正常完成的结果无需额外按键即可保存并提交；跳过剩余尾奏的输入不改变分数或回放。
- 新的标准游玩采用canonical R4。R1/R2/R3回放、Ghost和verifier保留各自原有规则。BMS RANK与Judge Mod对应的判定、LN松开以及血条/分数计算遵循[判定规则](judgement-windows.md)。
- 保留replay evidence v3的谱面SHA-256、ruleset与结果绑定、确定性输入重放以及已验证本地best的边界。记录普通Random实际使用的seed，并在重放时复用。

## Presentation And Controls Contract

- 为Native/LR2单人游玩提供Studio Deck，同时保留Classic选项，以及外部图像皮肤、Ghost和多人模式的Classic fallback。保留riff map显示、深色HUD底板和完整曲名自动适配。
- FAST/SLOW保持750ms，不被随后出现的PG清除。文字和条形指示的位置独立于判定位置，条形指示可选条件显示或始终显示。
- Options包含8张卡片。Mods从键数模式设置打开，NKRO Test从按键设置打开。游玩中 `F5/F6` 将Hi-Speed减半/加倍，`F7/F8` 调整画面时序，`F9` 截图，`F10` 打开账号，`F11` 打开聊天。区分菜单中 `F5` 的重新索引功能与游玩中的操作。
- VSync OFF时，受支持的音符图像按原始顺序进行GPU批量绘制。VSync ON、小批次、不受支持的变换/裁剪和procedural LN/地雷路径保留原有处理。不为性能而减少音符显示量、降低画质或时序精度。
- HUD属于显示层。画面修改不得改变判定、输入、音频、回放、Ghost、血条、分数或音符批量绘制的约定。

## Online And Trust Boundaries

- 最多8人的直接IP/LAN多人游玩及共享Rate要求使用兼容的protocol v6构建。peer分数为 `UNVERIFIED CLAIM`，不得视作服务器权威验证。遵循[多人游玩](multiplayer.md)和[Rate指南](multiplayer-rate-build.ko.md)。
- GPT Sites排行榜属于 `client_submitted` 记录。不得标示为服务器独立重放回放后得到的验证结果。连接和上传维持用户现有设置及同意边界。
- 区分自托管账号/聊天/排行榜路径与Sites。该排行榜的challenge、精确谱面字节和外部verifier复核策略，不得扩大解释为同样适用于Sites的保证。过去的兼容标注 `TenRiff Server v1.1.0` 不是当前服务器运营或全链路兼容性验证的证据。
- 保留自托管路径的DPAPI会话保护、密码掩码输入、批准后打开聊天URL及远程HTTPS要求。服务器运营、证书、秘密存储和保留策略属于单独的运营责任。
- 自托管端口和服务器认证策略遵循[服务器计划](ranked-integrity-plan.md)中的运营边界。不能仅凭公开客户端源码就认定已重新验证服务器实现及实际运营，也不能把相应端口策略解释为Sites托管设置。

## Verification And Limits

以下数字来自 **2026-10-09发布验证记录**，不代表本次文档固定工作重新运行了游戏检查。[1.8.7发布验证](release-1.8.7-gate.md)汇总了检查范围和证据位置。

- Release与独立解压的等价源码CTest各3/3，1,045 cases，集成测试跳过0；编辑器各33/33。
- 实际app/session 84/84，无声WASAPI共享输出23/23，两种HUD的sprite gate各7/7，17个原生菜单/939次hit检查，4种presentation mode各180/180 Presents。
- 28个曲名案例/2,520次测量Presents；与1.8.5比较的固定合成画面6轮A/B通过p50 +0.03ms上限及p95/p99门槛；PR/main ASan和OpenVINO CI通过。
- ZIP CRC、客户端573个/源码1,057个文件哈希、全部1,057个源码文件与合并Git blob一致、隐私信息检出0、3个公开资产重新下载比较均通过。最终ZIP的源码树与独立构建的候选相同，但没有再次从最终ZIP本身执行独立构建。
- 物理输入、可听输出、报告者设备、Discord、长时间游玩、其他PC、在线对战和API内部抢占仍未验证。可选的外部Python参照比较也未执行。
- 单独动态测试中的Classic p50 +0.1939ms、极长曲名的小字号、极端HUD布局、尚未检查的LR2变体，以及Web Canvas与实际游戏呈现之间的差异仍是限制。固定基准不代表这些限制已解决。

## Compatibility And Update Rule

- 后续工作从上述提交建立新分支/工作副本，并与1.8.7比较。针对其他版本测量性能时，注明比较版本。保留现有发布、安装、配置档及实验工作副本。
- 不因变化中的 `main`/`latest` 或新版本发布就自动更改固定基准。用户明确要求更改基准时，同步更新新版本文档及四种语言的README、文档索引、当前状态和路线图，并将旧文档标为历史记录。
- replay/score/chart identity、账号存储、API/端口/协议的变更必须伴随明确的兼容性决定、必要的migration和交叉兼容性验证。
- 区分后续文档提交与发布源码提交。不移动 `1.8.7` tag，也不重新生成或覆盖同名公开资产。
- 设置参阅[config](config.zh-CN.md)，游玩参阅[gameplay guide](gameplay-guide.zh-CN.md)，记录信任边界参阅[score integrity](score-integrity.md)和[ranked integrity](ranked-integrity-plan.md)。
