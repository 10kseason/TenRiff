# 星座选项与全体聊天输入

> **2.0.1** 将默认银河背景的亮度降至2.0.0的40%（原图RGB倍率为0.392）。 将8张卡片的宽高及内部文字和图标缩小至85%，并同步调整点击区域。 在卡片下方以单行显示所选选项的说明，支持韩语、英语、日语和简体中文。 [2.0.1](release-2.0.1.zh-CN.md)


[한국어](options-chat.ko.md) | [English](options-chat.en.md) | [日本語](options-chat.ja.md) | [简体中文](options-chat.zh-CN.md)

内置选项页参考提供的`TENRIFF_constellation_options_mockup.png`，将8个分类排列为4×2卡片。每张卡片显示不同的星座图案、彩色图标、当前值和选中高亮。保留分类顺序、设置值、键盘导航和点击目标的原有含义。星座为静态装饰，并非真实天体地图。保留F1帮助。

将提供的`TENRIFF_galaxy_background.png`原样复制为`assets/menu/options-galaxy-background.png`。仅在显示时通过颜色矩阵应用0.92饱和度和0.392亮度，保持原有透明度。图片保持比例铺满屏幕并裁切多余边缘。应用于内置选项/设置页，不覆盖自定义皮肤背景；图片加载失败时恢复原有背景路径。柔和边框光晕和星座面板只改变呈现方式。

2.0.2通过已在线认证的全体聊天服务输入和发送。按F11打开、Enter发送，不依赖房间。切换访客、断开或更换账号时清除本地历史和待发队列。

[Global chat](global-chat.zh-CN.md) · [2.0.2 verification](release-2.0.2-gate.md)
