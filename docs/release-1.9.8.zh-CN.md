# TenRiff 1.9.8 — 皮肤X/Y位置与键位分隔线亮度

2026-10-19

[한국어](release-1.9.8.ko.md) | [English](release-1.9.8.en.md) | [日本語](release-1.9.8.ja.md) | [简体中文](release-1.9.8.zh-CN.md)

现在可以在皮肤设置中分别调整判定、连击、FAST/SLOW指示器和FAST/SLOW条的X与Y。重新显示原有的隐藏X控件，保留默认值0及已保存坐标；修改X不会改变Y。每项X范围为−600～600px、步长10px，显示在对应Y的前一行。Native/TenRiff/LR2均支持本地化搜索、键盘左右和鼠标−/+。

键位分隔线亮度范围为0～100%，步长1%。默认100%保持原色，0%为黑色。仅调整RGB，显示开关、透明度、宽度及小节线保持不变。可选字段 `skin.lane_divider_brightness` 以 `0..1` 保存，默认 `1`，支持配置和便携皮肤，并应用于预览、Classic/Studio及幽灵显示。

复用既有X坐标存储与绘制路径，不重置或迁移坐标。判定线仍附着于轨道，横向位置跟随已有的场地拖动。固定判定、分数、评级、输入、音频与录像规则，R9/R10及历史R1–R8、peer v7和Guest/client_submitted边界保持不变。

完整保留1.9.7的通用按键及冲突检查、加大的彩色标签、蓝色FAST/红色SLOW、1%调整、游戏模式设置与Normal显示、每局/Retry新种子和已记录的录像种子，以及12.5%的场景音乐基准增益。通用固定基线仍为1.9.6，保留以前的发行版、候选、安装和配置。本次不包含网站、数据库或公开皮肤编辑器变更。

验证流程描述待完成的要求，并不代表已通过。构建、测试、打包和下载证据另行记录；实际输入、IME、听音、长时间游玩、其他PC及任意外部皮肤需单独验证。

[X/Y](skin-xy-20261019.zh-CN.md) · [Baseline 1.9.6](baseline-1.9.6.zh-CN.md) · [Normal / Hard / EX](judgement-baseline-20261018.zh-CN.md) · [Verification](release-1.9.8-gate.md) · [1.9.8 release](https://github.com/10kseason/TenRiff/releases/tag/1.9.8)
