> **역사 문서 — 1.9.9.5 이전 로컬 후보:** 아래 동작·미공개 상태·검사 수치는 해당 후보 당시 기록입니다. 후보 검증은 새 릴리스 검증이 아니며, 현재 범위와 색 정책은 [1.9.9.5](release-1.9.9.5.ko.md), 새 검증 상태는 [공개 릴리스 검증표](release-1.9.9.5-gate.md)를 따릅니다.

# 화면공유 버퍼 점검 / BGA 정지 복구 (로컬 후보)

1.9.9.3과 앞선 심플 롱노트·EX 6색·올콤보 변경을 이어받는 로컬 후보입니다. 공개 버전과 일반 기준선1.9.6은 바꾸지 않습니다.

## 확인한 동작과 수정

- HUD는 최신 스냅샷 하나만 보관하고, 노트 시각은 최신 오디오 기준점으로 계산합니다. Present 동안 HUD/판정 잠금을 잡지 않습니다. 렌더 대기는 밀린 프레임을 누적하지 않습니다.
- DXGI flip-discard/버퍼2개/최대 대기1프레임 설정은 이미 이전 후보에 있었습니다. 이번 수정으로 주장하지 않습니다. FPS/VSync/Present 플래그는 그대로입니다.
- WASAPI는 고정 장치 버퍼의 빈 공간만 채웁니다. 별도의 무한 PCM 큐는 없습니다. 일반 루프백은 별도 캡처 버퍼로 복사하지만, 실제 Discord 훅/드라이버/CPU·GPU 경합까지 배제하지는 못합니다.
- MF BGA는 첫 요청 이후 큰 시간 점프에도 과거 프레임을 요청당240개씩 읽었습니다. 현재 위치와3초 넘게 벌어지면 기존1초 preroll seek로 복구합니다. 정상 재생·알려진 sparse gap·EOF는 기존 처리를 우선합니다.
- 부정확하거나 실패한 seek, 읽기 한도를 넘는 긴 GOP는 순차 디코딩으로 돌아가 진행 상황을 보존합니다. 같은 keyframe으로 무한히 되돌아가지 않습니다. seek 직후 EOF에서는 마지막 유효 이미지를 유지합니다.

## 검증과 한계

65초 합성 MP4/AVI에서20/40/60초 요청의 첫 완료 프레임은 수정 전8.23/16.23/24.23초, 수정 후20/40/60초입니다. 긴 GOP는 여러 요청으로 복구하며 첫 요청의 즉시 복구를 보장하지 않습니다. MF가 지원하지 않는 영상은 기존 FFmpeg 대체 경로로 별도 검사해야 합니다.

10분·120회 반복 정지를 FPS60/144/1500, rate0.75/1/1.5, offset−200/0/+200ms 조합으로 합성 검사합니다. 이는 실제10분 실행이나 Discord 종단 간 검사와 다릅니다. 실시간 공유 시청자 싱크 해결은 아직 확인하지 않았습니다.

판정/입력/오디오 시계·버퍼 크기·FPS 기본값·스코어·리플레이 코드를 바꾸지 않았습니다. 반복적인 실제 오디오 공급 부족은 장치 시간과 제출 샘플 시간을 벌릴 가능성이 있으나 미측정입니다. 이 가설 때문에 판정 시계를 바꾸지 않습니다.

## 재현

`bga_video_smoke <65초 이상 MF 영상> --stall-recovery`, 정확히65초 영상에 `--stall-eof`, 65초/30fps/30초 keyframe 간격 영상에 `--long-gop`를 사용합니다. MF 검사에서는 FFmpeg 대체가 성공을 가리지 못하게 실패로 처리합니다. 기존 `--timeline`과 `--ffmpeg`는 유지합니다.

공유 전후 비교는 동일 곡·동일 그래픽 설정으로 진행하고, Discord Debug의 Screen Share 캡처 방식과 지연 변화를 함께 기록해야 합니다. 현재 성능 오버레이만으로 Discord 내부 대기열이나 실제 오디오 underrun을 확정할 수 없습니다. 실제 게임의 장치/QPC 대비 샘플 차이와 GetBuffer/ReleaseBuffer 지연 계측은 아직 추가하지 않았습니다.


## References

- [Discord capture methods](https://support.discord.com/hc/en-us/articles/9410427556375--Windows-Capturing-Application-Window-for-Screen-Share-and-Go-Live)
- [Discord streaming pipeline](https://discord.com/blog/how-it-all-goes-live-an-overview-of-discords-streaming-technology)
- [Windows loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
- [MF approximate seeking](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-setcurrentposition)
