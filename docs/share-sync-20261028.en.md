> **Historical document — local candidate before 1.9.9.5:** Behavior, unpublished status and check counts below describe that candidate only. Candidate evidence is not fresh release evidence. Current scope/colors follow [1.9.9.5](release-1.9.9.5.en.md); fresh verification follows the [public release gates](release-1.9.9.5-gate.md).

# Screen-share buffer audit / BGA stall recovery (local candidate)

Continues1.9.9.3 and the previous simple-LN, EX palette and ALL COMBO changes. The published version and general1.9.6 baseline remain unchanged.

HUD snapshots are latest-only; note rendering follows the latest audio anchor. Present holds neither the HUD nor judgement lock, and missed render deadlines do not form a backlog. Flip-discard, two buffers and the verified one-frame DXGI limit were already inherited. FPS, VSync and Present flags are unchanged.

WASAPI writes only into free space in a fixed endpoint buffer; there is no unbounded outgoing PCM queue. Standard loopback copies into a separate capture buffer. This does not exclude Discord hooks or CPU/GPU/driver contention. Repeated real audio starvation could separate submitted-sample time from device time; it remains unmeasured. No judgement, input, audio-clock, buffer-size, score or replay code is changed.

MF BGA previously drained at most240 old frames per request after a later time jump. A gap over3s now uses the existing1s preroll seek. Normal cadence, known sparse gaps and EOF retain priority. Failed/inaccurate seeks or a long GOP beyond the read budget revert to sequential progress, avoiding repeated rewinds to one keyframe. Immediate EOF after a catch-up seek retains the last valid image.

Synthetic65s MP4/AVI requests at20/40/60s returned8.23/16.23/24.23s before and20/40/60s after. Long GOPs may need several requests. Media unsupported by MF requires separate checks through the existing FFmpeg fallback. A deterministic ten-minute simulation covers120 stalls for FPS60/144/1500, rate0.75/1/1.5 and offset−200/0/+200ms. It is not a real-time ten-minute or end-to-end Discord test; viewer synchronization remains unverified.

Use `bga_video_smoke <native MF fixture at least65s> --stall-recovery`, `--stall-eof` for EXACT65s fixtures, and `--long-gop` for65s/30fps fixtures with30s keyframe spacing. MF-specific probes reject FFmpeg fallback. Existing `--timeline` and `--ffmpeg` remain. Compare identical song/settings before and during sharing and record Discord Debug Screen Share capture method. The existing overlay cannot establish Discord queue depth or actual-game audio underruns; no runtime device/QPC or GetBuffer/ReleaseBuffer trace was added.


## References

- [Discord capture methods](https://support.discord.com/hc/en-us/articles/9410427556375--Windows-Capturing-Application-Window-for-Screen-Share-and-Go-Live)
- [Discord streaming pipeline](https://discord.com/blog/how-it-all-goes-live-an-overview-of-discords-streaming-technology)
- [Windows loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
- [MF approximate seeking](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-setcurrentposition)
