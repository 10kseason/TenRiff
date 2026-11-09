> **Integrated in 1.9.9.9-final3.** The original candidate report below is historical. See [release guide](release-1.9.9.9-final3.zh-CN.md) for final behavior and validation scope.

# 动态皮肤背景视频与 TenRiff Code / CLI 皮肤（2026-11-09 本地候选）

[한국어](skin-video-background-20261109.ko.md) · [English](skin-video-background-20261109.en.md) · [日本語](skin-video-background-20261109.ja.md)

用户需求：做一个像在玩 Claude Code CLI 的皮肤，并让背景动起来。本候选是在 Modern Common 候选基础上单独延续的本地候选。

## 皮肤背景视频

- `lobby.background_video`：所有菜单画面共用的循环视频，替代内置的选曲／结算循环视频和皮肤静态背景。
- `lobby.screen_videos.<画面 ID>`：按画面指定的循环，按画面 → 备用画面 → 共用视频的顺序查找。空字符串表示该画面保持静态背景。
- `gameplay.background_video`：游戏中循环视频，仅在谱面没有基础 BGA 图层时解码和显示，并跟随与 BGA 相同的谱面时钟（暂停、倍速、重开）。支持按键数模式覆盖。
- 格式：MP4/M4V/MPG/MPEG/WMV/AVI/MOV/WEBM/MKV，路径规则与图片相同。不会自动检测，也不会播放音轨。
- 视频以同一画面的背景不透明度绘制在静态背景的位置。首帧之前、系统／菜单减少动态效果时以及解码失败时使用静态背景。重新加载皮肤会清除失败记录并重新打开文件。
- 文件夹导入和 `.trskin` 导出会一并复制被引用的视频。Schema 和随附离线编辑器（校验、打开文件夹、导出 ZIP）也支持新字段；编辑器预览画布显示静态背景。

## 视频解码器修复

H.264 1080p 在内部解码为 1088 行。Media Foundation 路径没有按 1080 行的显示区域裁剪，导致屏幕底部出现 4 行绿色；内置选曲／结算循环视频也能复现。解码器现在读取 `MF_MT_MINIMUM_DISPLAY_APERTURE`（缺失时使用 geometric aperture）并据此裁剪。信息缺失或无效时仍像以前一样使用整个缓冲区。

## TenRiff Code / CLI 皮肤

内置 `TenRiff_Code_CLI`：Cascadia Mono 字体、珊瑚色强调色、暖黑色面板、块状光标音符、选区风格长条、光标下划线按键以及 `pixels` 击打效果。菜单使用旋转符号漂浮的 16 秒循环，标题画面是输入框键入命令的循环，游戏中是会话日志逐行打出的 24 秒循环。支持 1K～16K 与 7+1。这是参考 Claude Code CLI 氛围制作的非官方同人皮肤，没有使用 Anthropic 的标志、吉祥物或商标图像，所有图片和视频均由随附的 Pillow 生成脚本绘制。

## 未改变的内容

判定、分数、计量槽、输入、音频、回放与排行规则、`version: 1`、现有皮肤和个人资料设置均不变，没有视频字段的皮肤行为也相同。但 1080p H.264 菜单视频底部 4 行的裁剪同样适用于内置视频。实机输入、长时间游玩以及其他电脑上的解码环境仍需另行验证。
