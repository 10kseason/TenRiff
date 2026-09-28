# Luma Keys 기본 인게임 스킨

2026-09-28, TenRiff 1.7.8 릴리스. 이전 메뉴 스킨 작업에 이어 인게임의 시각 요소를 새로 제작했습니다.

## 디자인

- 실제 디지털 악기처럼 깊이감이 있는 건반, 빠른 눌림과 부드러운 복원.
- 건반 아래 LED와 누른 위치의 짧은 신호 펄스. 롱노트를 누르는 동안 안정적인 발광 유지.
- 발광 바 형태의 노트, 별도 롱노트 헤드·꼬리, 중앙이 어두운 빛의 레일.
- 얇은 민트색 판정선과 낮춘 레인 구분선 밝기로 밀집 패턴의 판독성 유지.
- 게임 안의 스킨 설정 미리보기도 같은 건반·노트·롱노트 레일과 레인 색상을 표시합니다.
- 4·5·6·7·8·9·10·11·12·13·14·15·16키 모두 지원하는 스킨 배치. 빠져 있던 11·13·15키 팔레트와 스킨 편집 모드도 보완했습니다.

기존 사용자의 레인 색상, 노트 크기/형태, 외곽선, 판정선 표시, 이펙트 밝기를 따릅니다.
외부 TenRiff/LR2/OSU 스킨의 기존 표시 경로는 유지합니다. 판정 시각·음원·입력·채보 변환 규칙은 변경하지 않습니다.
이 문서의 4~16키 지원은 스킨과 레인 표시 범위를 뜻하며 새로운 채보 변환 모드를 추가하는 작업은 아닙니다.

## 사용

새 배포본을 별도 폴더에 풀고 `launch_win.bat`으로 실행하세요. `Options > Skins`에서 기본 Native 스킨을 선택합니다.
기존 배포 폴더와 사용자 프로필은 덮어쓰지 마세요. 기존 노트 형태를 다른 모양으로 지정했다면 `Rect`에서 새 기본 노트 에셋을 볼 수 있습니다.

에셋 15개의 원본은 [assets/native-gameplay/luma-keys](../assets/native-gameplay/luma-keys)에 있으며 배포본에는 `tools/skin_editor/vector-gameplay/luma-keys`로 동봉합니다.
아트는 직접 만든 벡터 도형입니다. 게임에서는 시작/스킨 변경 때 GPU 비트맵으로 만들고 재사용합니다. 매 프레임 디스크 읽기나 새 이미지 생성이 없습니다.

## 웹 스킨 에디터에서 수정

[웹 스킨 에디터](https://tenriff-skin-editor.lastestarcorp.chatgpt.site/) 또는 동봉된
`tools/skin_editor/index.html`에서 Native 기본 스킨을 시작점으로 사용할 수 있습니다.
[`TenRiff_NativeEditable/skin.json`](../skins/TenRiff_NativeEditable/skin.json)에
Luma Keys의 기본 도형과 시각 설정을 담았습니다.

- `gameplay.renderer: "native"`로 기본 디지털 건반 렌더러를 선택합니다.
- `gameplay.native.sprites`에서 대기·눌린 건반, 일반 노트, 롱노트 머리·꼬리의 도형 레이어를 편집합니다.
- `gameplay.native.metrics`, `colors`, `motion`, `rects`, `fonts`에서 건반 크기와 눌림·복원, LED, 판정선, 레일, HUD 위치와 글꼴의 지원 항목을 조정합니다.
- `gameplay.modes`에 4~16키 중 특정 모드의 설정을 넣어 키 수마다 다른 모습을 만들 수 있습니다.
- 사본을 저장해 게임의 `Options > Skins > Import Skin`으로 가져옵니다. 이후 가져온 폴더를 수정했다면 `F5 / Reload Skin`으로 다시 읽습니다.

자세한 범위와 상속 규칙은 [스킨 포맷](skin-format.md#native-인게임-편집)에 설명되어 있습니다.
웹 미리보기는 도형과 움직임을 편집하기 위한 화면이며, 게임의 최종 출력은 가져오기와 리로드 후 확인합니다.

## 검증 방법

- `python tools/generate_luma_keys_assets.py --check`
- Windows Release 빌드와 전체 CTest.
- `menu_visual_preview --gameplay --keys 4 --1080p --fixture-time 12.06 --capture`
- `--keys`를 4~16으로 바꿔 각 화면 확인. `--idle-keys`는 누르지 않은 비교 상태입니다.
- `--small`은 960×540, `--capture-frames 12,24,36`은 이동 중인 여러 프레임을 기록합니다.

검증 도구는 합성 채보로 실제 D3D 렌더러만 실행합니다. 사용자 입력과 오디오를 연결한 실플레이, 장시간 FPS, 여러 PC에서의 체감은 별도 확인이 필요합니다.

## 확인 결과

- Windows Release 빌드 성공. CTest 3/3, 단위·통합 러너 831개 통과.
- RTX 4060 Ti에서 4~16키 전체의 1920×1080 화면, 16키 960×540 화면, 스킨 설정 화면과 여러 애니메이션 프레임을 캡처했습니다.
- 건반 복원 계산은 60/144/300/1000 FPS에서 같은 경과 시간에 같은 상태가 되는지 검증했습니다. 이는 실제 프레임 속도 벤치마크 결과가 아닙니다.
- 15개 SVG와 컴파일되는 도형 데이터의 일치 확인을 통과했습니다.

기존 렌더러의 `SetMaximumFrameLatency(1)` 경고(`0x887a0001`)가 캡처 실행에서도 관찰됐지만 초기화와 이미지 저장은 성공했습니다. 선택 기능인 ONNX 배경 업스케일러 모델이 없는 미리보기 작업 폴더에서는 기본 배경 크기 조절로 실행했습니다.
