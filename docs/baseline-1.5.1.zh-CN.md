# TenRiff 1.5.1 Fixed Stable Baseline

本文档把 TenRiff 的稳定契约固定在 `1.5.1`。后续工作不得在没有明确兼容性变更与 migration 的情况下破坏这些规则。旧基准文档继续作为历史记录保留，但当前判断以本文档、实际代码和 `docs/current-state.zh-CN.md` 为准。

## Release Identity

- 固定基准版本线：`1.5.1`
- 发布名称：`fixed stable baseline`
- 基准平台：Windows GUI
- 产品范围：仅 BMS family（`.bms/.bme/.bml/.pms`）
- 兼容自托管服务器：`TenRiff Server v1.1.0`
- 正式资产：运行 ZIP、公开源码 ZIP、SHA-256 manifest

## Stable Contract

- 基本流程保持 `Title -> Song Select -> Gameplay -> Result`；在线服务失败时仍保留本地记录。
- BMS parsing/timing、long note、landmine、keysound、replay v3 与 deterministic replay verification 都属于基准行为。
- gameplay 输入时间以 audio playback head 为准。优先 RawInput，并保留 bound-key polling shadow 与 Polling fallback，但不改写用户保存的 backend 设置。
- NK3 以 P64 和 host beam safety solver 为 authoritative path；generalized MLP 只用于非10K原谱转换到10K。
- Windows 包包含 `Mainmusic/`、内置 skin、NK3 runtime/model，不包含 Songs 与 standalone BMS key-converter executable。

## Account And Global Chat

- `F10` 打开账号界面，并要求在登录/注册前选择主服务器或用户输入的私有 API 服务器。
- 密码绝不明文保存。服务器使用独立 salt 与 PBKDF2-HMAC-SHA256 600,000 次迭代，bearer token 也只保存 hash。
- 客户端保存的 session 使用 Windows DPAPI 保护。找回账号/密码不是此基准的必需功能。
- 密码栏支持 `Ctrl+V`，保留首尾空格，并始终以掩码显示粘贴内容。
- `F8` 可在菜单、游戏、结果界面打开同一 server-wide global chat overlay，聊天输入不得触发 gameplay/menu command。
- `/np` 发送当前曲名和 artist。聊天 URL 必须经过带动画的警告与用户明确确认后才能打开。
- 登录后，多人房间搜索使用所选主服务器/私有服务器的认证房间列表；退出登录时继续使用 LAN 搜索。

## Ranked BMS

- 批量 song indexing 只用于本地搜索，不预先填充 ranked catalog。
- 新 BMS 只在实际 result submission 时成为注册候选；只有 challenge-bound replay 成功复现后，记录才能进入 leaderboard。
- osu 派生、autoplay/assist、不完整及不可复现提交一律 fail-closed 拒绝。
- 管理员可通过 BMS SHA-256 exclusion list 禁止注册与 leaderboard 显示。
- 不信任客户端 score claim；服务器使用精确 chart bytes 与 external verifier 的结果。

## Network And Security

- 游戏服务器：`27301/TCP`
- origin API：`127.0.0.1:27302/TCP`，不得直接暴露到 LAN/WAN
- 公共 HTTPS gateway：`27303/TCP`；启用 HTTP/3 时再开放 `27303/UDP`
- `27304~27305` 保留；默认 host mapping 不开放 `27300`、`80`、`443`。
- 远程账号、聊天、排名必须使用可信 HTTPS certificate；HTTP 仅限 loopback 开发。

## Compatibility Rule

- 后续工作默认以 additive change 叠加在 `1.5.1 fixed stable baseline` 上。
- 修改 replay、score、chart identity、account storage、API 或 port 时，必须同时提供 versioned migration 与 cross-compatibility test。
- Git tag `1.5.1` 是不可变基准标识；发布后不得替换 tag 与公开资产。

## Companion Docs

- `docs/current-state.zh-CN.md`
- `docs/config.zh-CN.md`
- `docs/gameplay-guide.zh-CN.md`
- `docs/multiplayer.md`
- `docs/release-1.5.1-gate.md`
