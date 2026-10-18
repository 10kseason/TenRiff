# TenRiff 1.9.5.3 — 皮肤设置整理与搜索、输入改进

- 皮肤设置新增**轨道内判定计数**（默认关闭）。在指示器正下方显示PG / GR / G / BAD / PR累计数量，跟随指示器位置并限制在轨道内。击打间隔仍显示，玩家与幽灵分别统计。判定名称和计算不变。

- 点击预览中的数字按钮选择编辑轨道。UI字体、字体文件和游戏内光标移至个人设置。划盘位置仅在运行键模式为Original时显示，并可在7+1布局中调整。

- 新设置默认音符/区域60%、按键光束最大高度50%、5K白/蓝/白/蓝/白、简洁矩形。F1速度+0.25、F2−0.25、F3判定线上移1%、F4下移1%；保留F5/F6的×0.5/×2。

- 保持默认区域背后BGA遮挡ON、LN尾部显示OFF、LN主体宽度/不透明度100%、轨道间距0%、按键标签在下方，并隐藏这些皮肤设置和LN尾部缩小。保留已明确保存的值。

- 同时移除目标轨道、单色、音符边框、整体/边框不透明度、轨道背景浓度、16K中央间距和分隔线宽度行。16K中央间距新默认100%，实际分隔线宽度固定100%。判定线位置可调1–100%。

- 图片比例由配置后的实际音符区域决定：1:1时保持比例，其余情况拉伸。修复F6全屏皮肤预览和.trskin预设文件保存。

- 长按Backspace逐个删除完整UTF-8字符，不再返回上一页。普通搜索仅匹配曲名和艺术家。Lv:16和Lv:16+分别精确匹配各自等级。

- 按数字升序显示难度表定义的全部等级文件夹，保留空等级；进入后仅显示该等级曲目。最后音符/LN结束后，仅Enter可跳过尾奏。

- 复用现有设置、预设格式和输入动作以保留保存值。判定、分数、血条、音频及回放规则不变。保留固定基准1.9.0、Guest排除、固定评级、client_submitted及不附带BGA权重策略。

[Release and downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.3) · [Verification scope](release-1.9.5.3-gate.md).

Physical long play, other PCs and arbitrary imported skins remain unverified. Browser preview is illustrative; use in-game preview for actual rendering.
