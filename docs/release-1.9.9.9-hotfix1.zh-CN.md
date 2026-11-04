# TenRiff 1.9.9.9-hotfix1 — OGG 安全修复、通用 BGA 与皮肤编辑器

- 修复 stb_vorbis 的 comment 大小与数量、乘法和对齐、codebook 限制及部分分配失败时的清理。probe 与 decode 均使用修复后的公共解析器。保留许可证并记录 [upstream SHA 与依赖](../external/stb_vorbis.TENRIFF.md)。
- 在选项 > 图形中选择 **通用 BGA 视频**并启用 **通用 BGA**，用同一无声循环视频替换所有谱面的 BGA 和叠加层。关闭 BGA 显示时不会播放。默认关闭、路径为空。[用法](common-bga-20261104.zh.md)。
- 更新离线皮肤编辑器版本、图形预览的 14 个选项、选曲与结果示例及韩英日说明。通用 BGA 属于游戏配置文件，不写入 skin.json v1。浏览器预览只是示例，请在游戏中检查保存的皮肤。

保留 Media Foundation fallback。本机缺少 Opus-in-OGG 处理程序，实际 Opus 播放兼容性尚未验证。正常 mono/stereo Vorbis、多个 sample rate 和 65 秒音源的修复前后 PCM 一致。特别大的 codebook 按 upstream 限制拒绝。

将 Windows ZIP 解压到新目录并运行 `launch_win.bat`。`tools/skin_editor/Launch-Skin-Editor.cmd` 打开离线编辑器。保留现有安装、配置与记录。固定基准仍为 **1.9.6**。不改变判定、回放、在线记录或音频时序，也不部署网站或数据库。

[Verification / 검증](release-1.9.9.9-hotfix1-gate.md)
