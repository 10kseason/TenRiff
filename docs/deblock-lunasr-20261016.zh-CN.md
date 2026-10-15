# FFmpeg去块与外部ONNX x2

2026-10-16 · TenRiff 1.9.2。[发行范围](release-1.9.2.zh-CN.md)

**原图 → FFmpeg去块 → 外部AI x2修复与残差合成 → 适配显示区域 → 必要时裁剪**

对完整原始RGB执行一次FFmpeg内置 `deblock=filter=weak:block=8:planes=7`。使用planar RGB避免YUV色度抽样，并保留原始透明度。AI输入与bilinear x2底图使用相同的去块结果。

动态空间轴模型一次处理完整原始H/W。原有固定960×540输入/1920×1080残差输出继续通过边缘填充及32像素上下文分块支持。两条路径均不在推理前缩放或裁成16:9，并检查实际输出必须恰为原图x2。输入输出名为 `rgb_lr` 与 `rgb_residual_x2`，边界类型为float32/float16 NCHW RGB；动态模型的batch/channel也必须为1/3。

**公开运行包和源码包均不包含BGA模型。** 默认模型路径为空，旧的空路径也保持为空。在Graphics选择或拖入外部ONNX只会保存路径。功能默认OFF，另行启用时保留确认。已有路径和原有NK3键位转换模型继续保留。

仅处理静态BGA与选曲图片，视频AI仍禁用。原图每轴上限4096像素，每实例缓存最多3张/96 MiB。动态模型一次需要完整原图的内存，尺寸上限并不保证GPU显存足够。FFmpeg在worker中隐藏运行，取消或超过10秒期限后终止。缺失、滤镜或模型失败时保持原生显示。已经开始的WinML推理本身无法中断，但旧结果不会被显示。

FFmpeg先从游戏旁的 `tools/ffmpeg-deblock/ffmpeg.exe` 查找，再查PATH。依赖声明见[THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md)。原文件、旧候选、安装和配置均保持不变。模型画质、性能和长时间运行需要单独验证。

[处理契约与验证工具](../tools/onnx_upscaler/README.md) · [1.9.2验证流程](release-1.9.2-gate.md)
