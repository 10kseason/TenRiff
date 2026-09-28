# TenRiff UI 本地化

Language: [한국어](localization.md) | [English](localization.en.md) | [日本語](localization.ja.md) | 简体中文

## 支持的语言

客户端支持英语（`en`）、韩语（`ko`）和日语（`ja`），默认语言为`en`。中文文档不表示客户端已支持中文界面。

在 Options → Profile Setup → Language 或首次运行的 Quick Setup 中用左右键切换。语言和菜单字号立即更新并保存到当前配置档。`jp`、`japanese`、`ja-jp`、`ja_JP`不区分大小写，统一保存为`ja`；未知值回退到英语。

## 代码边界

- `src/ui/Localization.h/.cpp`：语言枚举、标识规范化、循环切换和统一翻译。
- `src/ui/JapaneseStrings.inc`：以完整英语 UI 文本为键的内置日语词典。
- `src/config/Config.cpp`：保存和读取`ui.language`。
- `src/app/MenuApp.cpp`：`ui_language()`及现有`ui_text(english, korean)`。
- `src/app/menu/settings/*SettingsView`：接收语言枚举的设置视图。
- `src/app/MenuAppTail.inl`及`src/render/MenuWindow_draw*.inl`：通过`ui_language`传递语言，使用`loc`、`wloc`、`result_loc`显示文本。

原来的韩语布尔值已改为语言枚举。保留现有英韩文本，在统一词典中维护日语。词典只构建一次，查找时不分配字符串；缺失文本回退到英语。

BMS 编辑器状态和游戏加载阶段保留内部原值，只在生成 UI 数据时翻译已知文本。判定、音频和音符布局保持不变。

## 添加文本和验证

使用现有 UI 翻译函数，并在`JapaneseStrings.inc`添加完全一致的英语键和日语译文。保留空格、换行和 UTF-8 符号。不要翻译配置值、曲名、作者、文件路径、玩家名或聊天内容。新动态键必须加入明确的检查范围。

```powershell
python tools/audit_japanese_localization.py
python tools/audit_japanese_localization.py --json
```

脚本检查显式翻译调用、画面标题、内置难度表及已知编辑器/加载状态。缺失、重复、空译文或未解析的动态键都会返回退出码1。使用运行时输出的覆盖数量。

C++ 测试覆盖语言标识、双向循环、代表性文本及未知值保留。配置测试覆盖三种语言的保存和加载；配置档设置测试覆盖语言行、首次运行选择和菜单字号。

## 限制

源码覆盖不等于所有界面已完成视觉验证。服务器、音频和解析器诊断、历史结果中的自由文本、外部皮肤图片内的文字以及用户内容可能保留原文。FAST/SLOW、BGA、ASIO、Rate 和按键名称等术语也可能保持英文。部分系统文件选择器的文件类型说明仍为英语。

请在720p和1080p下检查日语标题、选曲、设置、帮助、结果和加载画面的截断、字体回退和换行。源码和单元测试不能替代真实游戏或远程联机验证。
