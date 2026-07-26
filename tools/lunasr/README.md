# TenRiff용 LunaSR General 65:35 모델

이 폴더는 TenRiff의 선택형 x2 배경 업스케일 경로에 사용하는 LunaSR General 모델과 계약 문서다. Windows gameplay 런타임은 저해상도 BMS BGA 및 osu!mania 배경 이미지를 비동기로 1920x1080까지 보간하며, 처리 중이거나 실패한 경우에는 기존 native bitmap을 계속 표시한다.

## 모델 성격

- 학습 비율: Minecraft 65%, Wuthering Waves 35%
- 구조: Dense8-B6, 총 3,876 파라미터
- 처리 방식: 프레임별 BT.709 luma residual x2
- 시간축 모델이 아니므로 프레임 간 일관성을 별도로 학습하지 않았다.

독립 홀드아웃에서 bilinear 대비 평균 luma PSNR 개선량은 Minecraft `+2.197 dB`, Wuthering Waves `+1.869 dB`, 65:35 가중치 `+2.082 dB`였다.

## 파일

- `lunasr_general_mc65_ww35_dense8_b6_v1_540p_residual_winml_public.onnx`: Windows ML 호환 IR v9와 공개 배포용 metadata 정리를 적용한 런타임 모델
- `lunasr_general_mc65_ww35_dense8_b6_v1_540p_residual.json`: 입출력 계약
- `lunasr_general_mc65_ww35_dense8_b6_v1_540p_residual.verification.json`: PyTorch/ONNX Runtime 수치 검증
- `SHA256SUMS.txt`: 파일 무결성
- `LICENSE.LunaSR`: LunaSR 코드·모델 패키지 라이선스

원본 ONNX, 재내보내기용 PyTorch 체크포인트, private Hugging Face 위치 handoff는 로컬 보관물이며 공개 Git 저장소와 배포 패키지에는 포함하지 않는다.

## ONNX 계약

- 입력 이름: `luma_lr`
- 입력: `[1, 1, 540, 960]`, float32, gamma-encoded BT.709 luma, 범위 `0..1`
- 출력 이름: `luma_residual`
- 출력: `[1, 1, 1080, 1920]`, float32 residual
- 최종 RGB: `clamp(bilinear_x2(rgb_lr) + luma_residual, 0, 1)`
- ONNX opset: 18
- 연산자: `Add`, `Conv`, `DepthToSpace`, `Relu`

모델 출력은 완성 RGB가 아니라 luma residual이다. 저해상도 RGB를 bilinear x2한 뒤 residual 한 채널을 R/G/B에 동일하게 더해야 원래 색차를 보존한다.

## TenRiff 런타임 연결

1. BMS 채널 `04`/ `07`의 sample-aligned 이미지 cue 또는 osu!mania Events 배경을 gameplay HUD에 전달한다.
2. 원본이 1920x1080보다 작은 경우 WIC로 중앙 16:9 crop 후 `960x540` BGRA/luma 입력을 만든다.
3. Windows ML의 DirectXHighPerformance 장치로 `luma_lr`를 비동기 추론한다.
4. CPU half-pixel bilinear x2 RGB에 residual을 합성하고, 알파를 premultiply한 FHD bitmap을 D2D 캐시에 넣는다.
5. worker가 준비될 때까지는 native bitmap을 표시하며, 모델/디코드/추론 실패도 native 경로로 유지한다.

`TENRIFF_ENABLE_LUNASR=ON`이 Windows MSVC 빌드의 기본값이다. Graphics Settings의 `BGA Upscale`에서 `LunaSR FHD` 또는 `Native`를 선택할 수 있고, 설정 키는 `graphics.background_upscale_mode`이다.

QHD 출력은 이 고정 모델의 shape와 맞지 않는다. `1280x720 -> 2560x1440` 고정 ONNX를 다시 내보내거나, 먼저 1080p 출력 후 별도 스케일을 적용해야 한다. 임의 shape 입력은 현재 ONNX에서 지원하지 않는다.

## 검증 상태

- ONNX checker: 통과
- ONNX Runtime CPU 실행: 통과
- PyTorch 대비 최대 절대오차: `3.8743019104e-7`
- RTX 4060 Ti FP32 실측: `256² -> 512²` GPU 상주 약 `1,148 fps`, `512² -> 1024²` 약 `534 fps`
- 실제 Intel NPU/OpenVINO 장치: 미검증

## 현재 제한

- 고정 모델이므로 출력은 FHD이며 QHD/4K에서는 이 결과를 기존 렌더러가 다시 화면 크기에 맞춘다.
- 현재 이미지 BGA와 이미지 배경의 첫 프레임을 처리한다. 동영상 BGA 및 애니메이션 GIF의 연속 프레임 추론은 지원하지 않는다.
- BMS poor-BGA 채널 `06`의 miss-trigger 의미와 검정색 chroma-key 합성은 아직 구현하지 않았다. PNG 알파는 유지한다.

공개 WinML 호환본은 원본의 IR version을 `10 -> 9`로 맞추고 계산과 무관한 exporter debug metadata를 제거했다. ONNX Runtime 비교에서 같은 난수 입력에 대한 최대 절대오차는 `0.0`이다.
