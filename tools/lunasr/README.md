# TenRiff LunaSR Quality RGB x2 INT8

이 폴더에는 TenRiff의 선택형 실시간 BGA/BGI 업스케일 경로에 사용하는 LunaSR RGB residual INT8 모델과 런타임 계약이 들어 있습니다.

## 동작 범위

- `graphics.background_upscale_mode=lunasr`이고 입력이 `1920x1080`보다 작을 때만 FHD x2 후보로 처리합니다.
- BMS gameplay base/overlay BGA, osu!mania 배경, Song Select의 선택 BGI를 렌더·오디오 스레드와 분리된 worker에서 처리합니다.
- 정적 PNG/JPG 계열은 WIC로 읽습니다.
- MPG/MPEG/MP4/M4V/WMV/AVI BGA는 Media Foundation을 먼저 사용하고, 시스템 코덱이 열지 못하면 `ffmpeg.exe` image-pipe로 폴백합니다.
- 모델 준비·벤치마크·디코드·추론이 실패하거나 느리면 native bitmap/video frame을 계속 표시합니다.

## 35 FPS 성능 게이트

LunaSR x2는 처음 필요해질 때 고정 `960x540 RGB -> 1920x1080 RGB residual` 추론을 3회 워밍업한 뒤 12회 측정합니다. 측정값이 `35 FPS` 미만이면 해당 프로세스에서 LunaSR를 비활성화하고 이후 BGA 프레임 복사와 추론도 중단합니다. BGA 자체는 native scaling으로 계속 재생됩니다.

2026-07-28 INT8 QDQ 모델은 Windows ML에서 모델 로드, DirectX session compile, 실제 evaluate까지 통과했습니다. 변환 당시는 GPU 학습이 동시에 실행 중이어서 FPS 수치는 대표값으로 기록하지 않았습니다. 실제 활성화 여부는 학습·게임·백그라운드 부하가 반영된 각 프로세스의 최초 35 FPS 벤치마크로 결정됩니다.

권장 GPU는 `RTX 3070급 이상`으로 추정합니다. 이는 보장 사양이 아니며, 드라이버·전원 상태·동시 GPU 부하에 따라 같은 하드웨어도 결과가 달라질 수 있습니다.

## 모델 계약

- 로컬 변환 입력 checkpoint SHA-256: `4027c43462e5422be23d2eed72056cf294e41390103fa79c62ddd36a5b5181ba` (미배포)
- 변환 기준 FP32 ONNX SHA-256: `12b93fd0f7d60cb5bbda80215f9b4eac78e241c83a3307821d3474830bac52f0` (미배포)
- WinML 런타임본: `lunasr_quality_rgb_staged32_intel_npu_x2_v1_e48_540p_rgb_residual_int8_qdq_winml_public.onnx`
- 입력: `rgb_lr`, float32 `[1, 3, 540, 960]`
- 출력: `rgb_residual_x2`, float32 `[1, 3, 1080, 1920]`
- 내부 양자화: static symmetric S8S8 QDQ, 35/35 Conv activation·weight INT8, per-channel weight
- 합성: `clamp(bilinear_x2(rgb_lr) + rgb_residual_x2, 0, 1)`
- ONNX opset 18, IR 9, 290,222 parameters, 503,736 bytes

입출력 경계가 float32인 것은 Windows ML 바인딩 계약이며 FP32 Conv 실행을 뜻하지 않습니다. 그래프 내부 35개 Conv 모두 앞뒤에 Q/DQ가 배치되고 INT8 weight를 사용합니다. 두 종류의 실게임 화면 16장으로 보정했고, 별도 4장 비교에서 FP32 기준 합성 RGB PSNR은 최소 `47.37 dB`, 평균 `49.89 dB`였습니다.

## 파일

- `*_int8_qdq_winml_public.onnx`: TenRiff가 로드하는 정적 S8S8 QDQ INT8 모델
- `*_int8_qdq_winml_public.json`: 공개 가능한 양자화·품질·게이트 검증 요약
- `quantize_int8.py`: FP32 ONNX와 사용자가 지정한 이미지 폴더로 동일한 INT8 형식을 생성하는 도구
- `winml_smoke.cpp`: 모델 로드, 출력, 35 FPS 정책 스모크
- `bga_video_smoke.cpp`: 실제 비디오 프레임 진행 스모크
- `SHA256SUMS.txt`: 공개 런타임 파일 무결성
- `LICENSE.LunaSR`: LunaSR 모델/코드 라이선스

`tools/lunasr/model/`은 로컬 변환 입력 전용이며 `.gitignore`로 차단됩니다. 그 안의 checkpoint, 로그, 설정은 Git 스테이징·소스 번들·GitHub 업로드 대상이 아닙니다. 로컬 절대 경로가 들어 있는 `*_verification.json`과 private Hugging Face 메모도 공개 배포 계약에 포함하지 않습니다.

## 제한

- 업스케일 결과는 고정 FHD이며 QHD/4K 직접 출력 모델이 아닙니다.
- FFmpeg 폴백이 필요한 MPG는 `ffmpeg.exe`가 실행 파일 옆 또는 PATH에 있어야 합니다.
- INT8 성능은 실행 공급자와 하드웨어에 따라 달라지므로, FP16 또는 FP32 측정치로 추정하지 않고 35 FPS 런타임 게이트 결과를 사용합니다.
- BMS poor-BGA 채널 06의 miss-trigger 전환 및 색상 키 합성은 별도 기능 범위입니다.