# TenRiff LunaSR Basic v2

이 폴더에는 TenRiff의 선택형 실시간 배경 업스케일링에 사용하는 LunaSR Basic v2 공개 런타임 모델과 계약 문서가 들어 있습니다.

## 동작 범위

- 입력 이미지가 `1920x1080`보다 작고 `graphics.background_upscale_mode`가 `lunasr`일 때만 처리합니다.
- 게임플레이 BMS BGA의 Base 채널과 Overlay 채널, osu!mania 배경을 각각 비동기로 FHD 보간합니다.
- Song Select의 선택 곡 BGI/재킷 미리보기도 같은 경로로 비동기 보간합니다.
- 추론 준비 중이거나 실패한 경우에는 원본 WIC 비트맵을 그대로 표시합니다.
- 고해상도 원본은 재처리하지 않습니다.

## 배포 파일

- `lunasr_basic_v2_dense8_b6_540p_residual_winml_public.onnx`: Windows ML용 공개 런타임 모델
- `lunasr_basic_v2_dense8_b6_540p_residual.json`: 입출력 계약
- `lunasr_basic_v2_dense8_b6_540p_residual.verification.json`: 체크포인트/ONNX 검증 결과
- `export_basic_v2_winml.py`: 동일한 공개 ONNX를 다시 만드는 변환 스크립트
- `SHA256SUMS.txt`: 공개 파일 무결성
- `LICENSE.LunaSR`: LunaSR 코드·모델 패키지 라이선스

`basic_v2_final.pt`는 변환 소스 체크포인트이며 공개 Git 또는 배포 패키지에 포함하지 않습니다.

## 모델 계약

- 구조: Dense8-B6, 8채널, 6블록, residual gate 없음
- 입력: `luma_lr`, float32 `[1, 1, 540, 960]`, gamma-encoded BT.709 luma `0..1`
- 출력: `luma_residual`, float32 `[1, 1, 1080, 1920]`
- 합성: `clamp(bilinear_x2(rgb_lr) + luma_residual, 0, 1)`
- ONNX opset 18 / IR version 9

모델 출력은 완성 RGB가 아니라 luma residual입니다. TenRiff는 원본 RGB를 half-pixel bilinear x2로 확대한 뒤 같은 residual을 R/G/B 채널에 합성합니다.

## 검증 결과

- source checkpoint: `basic_v2_final.pt`, epoch 84
- validation PSNR: `44.04436367648002 dB`
- bilinear baseline: `42.454356084956224 dB`
- improvement: `+1.5900075915237935 dB`
- ONNX checker: 통과
- ONNX Runtime CPU: 통과
- PyTorch 대비 최대 절대 오차: `3.2782554626464844e-07`
- PyTorch 대비 평균 절대 오차: `3.704051820818677e-08`

## 현재 제한

- 고정 입력/출력 모델이므로 QHD/4K 직접 출력은 지원하지 않습니다. FHD 결과를 렌더러가 화면 크기에 맞춥니다.
- 현재 이미지 BGA/BGI의 정적 프레임을 처리합니다. 동영상 BGA나 애니메이션 GIF의 연속 프레임 추론은 지원하지 않습니다.
- BMS poor-BGA 채널 06의 miss-trigger 전환과 색상 키 합성은 이 기능 범위 밖입니다.