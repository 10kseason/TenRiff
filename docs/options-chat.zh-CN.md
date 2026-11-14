# 星座选项界面与房间聊天输入

[한국어](options-chat.ko.md) | [English](options-chat.en.md) | [日本語](options-chat.ja.md) | [简体中文](options-chat.zh-CN.md)

内置选项页参考提供的`TENRIFF_constellation_options_mockup.png`，将8个分类排列为4×2卡片。每张卡片显示不同的星座图案、彩色图标、当前值和选中高亮。保留分类顺序、设置值、键盘导航和点击目标的原有含义。星座为静态装饰，并非真实天体地图。保留F1帮助。

将提供的`TENRIFF_galaxy_background.png`原样复制为`assets/menu/options-galaxy-background.png`。仅在显示时通过颜色矩阵应用0.92饱和度和0.98亮度，保持原有透明度。图片保持比例铺满屏幕并裁切多余边缘。应用于内置选项/设置页，不覆盖自定义皮肤背景；图片加载失败时恢复原有背景路径。柔和边框光晕和星座面板只改变呈现方式。

修复房间聊天输入错误依赖无关全局聊天服务配置的问题。打开的聊天层通过已连接的`PeerSession`接收已确认文本，托管房间也使用同一路径。F11开关、Enter发送和UTF-8的256字节上限保持不变。聊天隐藏、断线及账号/URL警告窗口会阻止输入落入错误字段。账号认证、protocol 2和服务器发送频率限制均不变。

回归覆盖已确认的韩文/ASCII输入队列、本地双向聊天、托管模拟响应和双客户端Worker聊天。本文件不宣称测试已通过或性能已提升；实际执行结果以[2.0.0验证关卡](release-2.0.0-gate.md)为准。真实Windows输入法候选/确认键时序、生产账号消息、网络延迟、长时间使用和其他电脑仍需单独验证。现有[ALL COMBO显示](menu-starlight.zh-CN.md)、计分和判定规则不属于此次修改范围。
