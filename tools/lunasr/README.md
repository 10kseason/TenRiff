# TenRiff LunaSR 사용자 모델 연동

이 폴더에는 선택형 실시간 BGA/BGI 업스케일 코드와 모델 변환·검증 도구만 들어 있습니다. 공개 저장소와 릴리즈 패키지는 ONNX 모델, checkpoint, 학습 데이터, 모델 전용 검증 메타데이터를 포함하지 않습니다.

## 공개 배포 경계

- 제3자 게임 촬영 자료가 섞인 실험 모델은 권리 경계가 명확하지 않아 공개 배포 대상에서 제외합니다.
- 별도 라이선스 파일을 붙이거나 제거하는 것만으로 학습 자료의 재배포 권리가 생기지는 않습니다.
- 사용자는 직접 제작했거나 사용·재배포 권리가 확인된 모델만 연결해야 합니다.
- `graphics.background_upscale_mode` 기본값은 `off`이며, 모델이 없거나 로드·추론에 실패하면 native bitmap/video scaling을 계속 사용합니다.

## 사용자 모델 계약

사용자가 기능을 명시적으로 켜려면 아래 계약을 만족하는 모델을 `tools/lunasr/lunasr_user_rgb_x2_winml.onnx` 또는 실행 파일 옆 `lunasr/lunasr_user_rgb_x2_winml.onnx`에 둡니다.

- 입력: `rgb_lr`, float32 `[1, 3, 540, 960]`
- 출력: `rgb_residual_x2`, float32 `[1, 3, 1080, 1920]`
- 합성: `clamp(bilinear_x2(rgb_lr) + rgb_residual_x2, 0, 1)`
- 런타임: Windows ML에서 load, DirectX session compile, evaluate가 모두 성공해야 함
- 성능 게이트: 3회 warm-up 후 12회 측정해 `35 FPS` 이상일 때만 현재 프로세스에서 활성화

모델이 계약을 만족하더라도 결과 품질, 성능, 권리 상태는 사용자가 제공한 모델에 따라 달라집니다. TenRiff 공개 릴리즈는 특정 모델의 품질이나 권리 상태를 보증하지 않습니다.

## 도구

- `quantize_int8.py`: 사용자가 지정한 FP32 ONNX와 권리 정리된 이미지 폴더로 static S8S8 QDQ INT8 모델을 만드는 도구
- `winml_smoke.cpp`: 사용자 모델의 load, session compile, evaluate, 출력 크기를 확인하는 Windows ML 스모크
- `bga_video_smoke.cpp`: 비디오 BGA 프레임 진행 스모크

`tools/lunasr/model/`은 로컬 변환 입력 전용이며 `.gitignore`로 차단됩니다. checkpoint, 로그, 설정, 로컬 절대 경로가 들어 있는 `*_verification.json`은 Git 스테이징·소스 번들·GitHub 업로드 대상이 아닙니다.

## 제한

- 업스케일 결과는 고정 FHD이며 QHD/4K 직접 출력 모델이 아닙니다.
- FFmpeg 폴백이 필요한 MPG는 `ffmpeg.exe`가 실행 파일 옆 또는 PATH에 있어야 합니다.
- BMS poor-BGA 채널 06의 miss-trigger 전환 및 색상 키 합성은 별도 기능 범위입니다.
