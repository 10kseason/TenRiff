# TenRiff 1.9.9.8 hot fix 1 — 2026-10-30

音频输出不可用导致单人游戏无法开始时，选曲界面底部会显示黄色提示。请在**选项 > 音频 > 声音输出设备**中选择已连接的设备，然后重试。ASIO请选择已安装的驱动程序。游戏会话初始化成功后提示会消失。

[Details](audio-device-guide-20261030.zh-CN.md) · [Validation](release-1.9.9.8-hotfix1-gate.md)

The old failure text was only drawn inside the difficulty-table URL editor. The fix uses the visible song-selection footer. The saved output choice is retained; there is no silent output switching. Audio processing, input, judgement, scoring, R13/R14 and replay/peer contracts remain unchanged. This does not change the multiplayer loading-error screen. No website or database deployment. Fixed baseline: **1.9.6**.

[Release and downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.8-hotfix1). Extract into a new folder and run launch_win.bat. Preserve existing installations and profiles.
