# TenRiff 1.6.0 릴리스 게이트

`1.6.0`은 `1.5.1 fixed stable baseline` 위에서 메뉴 navigation과 settings ownership을 정리하고, 사용자 설정 동작을 일관되게 만든 Windows 클라이언트 릴리스다.

## 포함 범위

- `MenuNavigator` 기반 중첩 Back history
- Audio, Input, Calibration, Graphics/ONNX, Keymap/NKRO, Skin, Mode/Mod Manager의 typed controller 경계
- 화면 제목, 스킨 배경/fallback, snapshot/generic-view routing을 소유하는 exhaustive `MenuScreenDescriptor`
- Master/BGM/Keysound 가로 슬라이더와 keyboard/pointer 범위·snap 규칙 통합
- VSync off의 Unlimited gameplay pacing을 실제 최대 1500 FPS로 제한
- 멀티플레이 Space 옵션 shortcut의 active-match/Ready 안전 규칙 통합

## 자동 검증

- Release 단위 테스트: 706/706
- Release CTest: 3/3 (`nk3_onnx_smoke`, `gameplay_judgement_benchmark`, `bms_parser_tests`)
- MSVC AddressSanitizer CTest: 1/1 (`bms_parser_tests_asan`, `RelWithDebInfo`)
- Release `TenRiff.exe`와 `tenriff-replay-verifier.exe` 빌드
- 실행/source ZIP 전체 엔트리 읽기, 필수 파일 존재, standalone `bms_key_converter*.exe` 부재, SHA-256 manifest 일치 확인

## 수동 경계

자동 테스트는 실제 D3D11 창에서의 keyboard/click/drag 감각, 스킨별 화면 배경, 메뉴 음악, Windows file picker를 증명하지 않는다. 회귀 제보 시 `docs/menu-refactor-plan.md`의 GUI smoke 순서로 재현한다.

## 정식 자산

- `TenRiff-1.6.0.zip`
- `TenRiff-1.6.0-source.zip`
- `TenRiff-1.6.0-SHA256SUMS.txt`
