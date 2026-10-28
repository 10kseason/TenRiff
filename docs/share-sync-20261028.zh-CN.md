> **历史文档 — 1.9.9.5之前的本地候选：** 下方行为、未发布状态和检查数量仅记录当时的候选。候选验证不等于新的发布验证。当前范围及配色以[1.9.9.5](release-1.9.9.5.zh-CN.md)为准，新验证状态见[公开发布验证表](release-1.9.9.5-gate.md)。

# 屏幕共享缓冲检查与BGA停顿恢复（本地候选）

继承1.9.9.3及前一个候选的简约长音符、EX六色和ALL COMBO更改。已发布版本和一般基线1.9.6保持不变。

HUD只保存最新快照，音符显示使用最新音频基准点。Present期间不持有HUD/判定锁，错过的渲染期限不会形成积压。flip-discard、两个缓冲和最多一帧排队早已存在；FPS、VSync及Present标志不变。

WASAPI只写入固定设备缓冲的空闲空间，没有无限PCM发送队列。标准loopback复制到独立采集缓冲，但不能据此排除Discord钩子、CPU/GPU或驱动竞争。反复的音频供应不足可能使设备时间与提交样本时间分离；尚未测量。判定、输入、音频时钟、缓冲大小、分数及回放代码均不更改。

MF BGA原先在后续大幅时间跳跃后仍每次读取最多240个旧帧。相差超过3秒时，现在使用已有的1秒preroll seek恢复。正常播放、已知稀疏帧间隔及EOF优先保留原行为。seek不精确/失败或长GOP超过读取预算时继续顺序解码，避免反复回到同一关键帧。seek后立即EOF时保留最后有效图像。

65秒合成MP4/AVI中，20/40/60秒请求的首个完成帧由修改前8.23/16.23/24.23秒恢复为20/40/60秒。长GOP可能需要多次请求；MF不支持的视频需通过已有FFmpeg后备路径单独检查。十分钟、120次停顿的确定性模拟覆盖FPS60/144/1500、rate0.75/1/1.5和offset−200/0/+200ms。这不是实时十分钟或Discord端到端验证，观众同步仍待确认。

使用`bga_video_smoke <至少65秒的MF视频> --stall-recovery`；精确65秒视频用`--stall-eof`；65秒/30fps/30秒关键帧间隔用`--long-gop`。MF专用检查拒绝把FFmpeg后备当作通过。保留`--timeline`和`--ffmpeg`。应使用相同歌曲及设置比较共享前后，并记录Discord Debug的Screen Share采集方式。已有性能叠层不能确定Discord队列深度或实际游戏音频underrun；未添加实际游戏device/QPC与GetBuffer/ReleaseBuffer跟踪。


## References

- [Discord capture methods](https://support.discord.com/hc/en-us/articles/9410427556375--Windows-Capturing-Application-Window-for-Screen-Share-and-Go-Live)
- [Discord streaming pipeline](https://discord.com/blog/how-it-all-goes-live-an-overview-of-discords-streaming-technology)
- [Windows loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
- [MF approximate seeking](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-setcurrentposition)
