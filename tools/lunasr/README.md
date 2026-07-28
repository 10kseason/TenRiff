# TenRiff LunaSR Quality RGB x2

이 폴더에는 TenRiff의 선택형 실시간 BGA/BGI 업스케일 경로에 사용하는 LunaSR RGB residual 모델과 런타임 계약이 들어 있습니다.

## 동작 범위

- `graphics.background_upscale_mode=lunasr`이고 입력이 `1920x1080`보다 작을 때만 FHD x2 후보로 처리합니다.
- BMS gameplay base/overlay BGA, osu!mania 배경, Song Select의 선택 BGI를 렌더·오디오 스레드와 분리된 worker에서 처리합니다.
- 정적 PNG/JPG 계열은 WIC로 읽습니다.
- MPG/MPEG/MP4/M4V/WMV/AVI BGA는 Media Foundation을 먼저 사용하고, 시스템 코덱이 열지 못하면 `ffmpeg.exe` image-pipe로 폴백합니다.
- 모델 준비·벤치마크·디코드·추론이 실패하거나 느리면 native bitmap/video frame을 계속 표시합니다.

## 35 FPS 성능 게이트

LunaSR x2는 처음 필요해질 때 고정 `960x540 RGB -> 1920x1080 RGB residual` 추론을 3회 워밍업한 뒤 12회 측정합니다. 측정값이 `35 FPS` 미만이면 해당 프로세스에서 LunaSR를 비활성화하고 이후 BGA 프레임 복사와 추론도 중단합니다. BGA 자체는 native scaling으로 계속 재생됩니다.

2026-07-28 현재 이 개발 PC의 반복 WinML 스모크는 `24.09~47.92 FPS`로 측정되었습니다. `35 FPS` 미만 실행에서는 LunaSR가 차단되고, `35 FPS` 이상이면 활성화됩니다. 이 수치는 기기·드라이버에 따라 달라지며, 실제 사용 가능 여부는 각 PC에서 최초 실행 시 결정됩니다.

## 모델 계약

- 원본: `lunasr_quality_rgb_staged32_intel_npu_x2_v1_e48_540p_rgb_residual_fp16.onnx`
- WinML 런타임본: `lunasr_quality_rgb_staged32_intel_npu_x2_v1_e48_540p_rgb_residual_fp16_winml_public.onnx`
- 입력: `rgb_lr`, float16 `[1, 3, 540, 960]`
- 출력: `rgb_residual_x2`, float16 `[1, 3, 1080, 1920]`
- 합성: `clamp(bilinear_x2(rgb_lr) + rgb_residual_x2, 0, 1)`
- ONNX opset 18, 81 nodes, 290,222 parameters

원본 ONNX는 IR 10이지만 이 프로젝트가 사용하는 Windows ML 런타임은 IR 9까지만 허용합니다. 따라서 런타임본은 원본을 보존한 채 IR version 필드만 9로 낮춘 별도 파일입니다. graph, opset, tensor, weight는 바꾸지 않았습니다.

## 파일

- `*_fp16.onnx`: 전달받은 원본 RGB FP16 모델
- `*_winml_public.onnx`: TenRiff가 로드하는 WinML IR 9 파생본
- `*_winml_public.json`: 공개 가능한 모델·게이트 계약과 검증 요약
- `winml_smoke.cpp`: 모델 로드, 출력, 35 FPS 정책 스모크
- `bga_video_smoke.cpp`: 실제 비디오 프레임 진행 스모크
- `SHA256SUMS.txt`: 공개 런타임 파일 무결성
- `LICENSE.LunaSR`: LunaSR 모델/코드 라이선스

로컬 checkpoint 경로가 들어 있는 `*_verification.json`, `basic_v2_final.pt`, private Hugging Face 메모는 공개 배포 계약에 포함하지 않습니다.

## 제한

- 업스케일 결과는 고정 FHD이며 QHD/4K 직접 출력 모델이 아닙니다.
- FFmpeg 폴백이 필요한 MPG는 `ffmpeg.exe`가 실행 파일 옆 또는 PATH에 있어야 합니다.
- BMS poor-BGA 채널 06의 miss-trigger 전환 및 색상 키 합성은 별도 기능 범위입니다.