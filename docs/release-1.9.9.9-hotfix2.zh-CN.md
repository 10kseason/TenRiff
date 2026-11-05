# TenRiff 1.9.9.9-hotfix2 — 8K 与 10K 的 NK2 E+ 预设

在选项 > 模式中选择 **8K 或 10K**、转换算法 **NK2**，再选择位于 E 后的 **E+ - Full-field + taps**。切换到其他键数时预设恢复为 E。同键数谱面也会重新排列。

将 KeyWeaver 1.4.0 E+ 作为独立引擎导入。使用整个轨道范围，只在保留的原始音符起始时刻增加单键。保留原始采样时刻、LN 语义与键音，新增音符不复制键音。被移除原始音符的声音保留在现有自动播放队列中。安全检查失败时保留原谱。已有 E、Native/Transform/Remaster 及默认设置保持不变。

标识 `fullfield_taps_eplus_v1` 区分回放、BEST、幽灵与在线提交的转换模式，不改变 R13/R14 规则或文件格式。请勿用不支持此预设的旧客户端播放 E+ 回放。不保证任意谱面的转换品质或实际演奏手感。

包含 hotfix1 的 OGG 安全修复和通用 BGA，固定基准仍为 **1.9.6**。将 Windows ZIP 解压到新目录，运行 `launch_win.bat`，保留现有安装、配置与记录。本次不重新部署网站或数据库。

[Engine provenance](../nk2eplus/README.md) · [Validation](release-1.9.9.9-hotfix2-gate.md)
