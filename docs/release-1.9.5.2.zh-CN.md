# TenRiff1.9.5.2 — 音符与区域大小最低10%

审查还修复了BMS编辑器长条主体可能比窄音符头更宽的问题。主体限制在显示头部宽度的80%以内，50–140%时保留原有坐标。编辑器仍保留8px最小头部以保证可读性，因此极窄轨道并非严格等比缩放；游戏区域缩放独立于此。

原生预览工具现已支持新的运行中尺寸范围，并传递编辑器宽度和键数。工具记录实际值和音符区域，用于10%和长条像素检查；编辑器输入分发与像素验证分别报告。

包含GOMazk的贡献(PR #102)。Note & Field Size的范围从50–140%扩展为10–140%。默认100%、已保存值和各键数模式设置保持不变；菜单、游戏、幽灵和预览使用相同下限。判定、分数、输入、音频计算以及1.9.5.1的350ms FAST/SLOW和文件选择行为保持不变。

Contribution: [GOMazk / PR #102](https://github.com/10kseason/TenRiff/pull/102), commit `29bc6341588390b2299d335a248617d2cde6d0ef`. The original contribution remains in Git history.

The fixed baseline remains1.9.0. No replay/schema migration, website/database deployment or bundled BGA restoration weights. Existing releases, installations and profiles are preserved.

Verification scope: [release gate](release-1.9.5.2-gate.md). Publication and downloads: [1.9.5.2](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.2). Physical long play, other PCs and arbitrary imported skins remain unverified.
