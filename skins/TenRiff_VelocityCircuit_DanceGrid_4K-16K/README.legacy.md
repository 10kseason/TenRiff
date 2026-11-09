# Velocity Circuit: Dance Grid

TenRiff의 기본 화면과 다른 구성을 목표로 만든 오리지널 아케이드 댄스 스킨입니다.
상용 리듬게임의 로고, UI, 노트 이미지는 복제하지 않았습니다.

## Visual brief

- Mood: 야간 아케이드 무대, 속도감 있는 회로와 댄스 플로어 조명
- Palette: acid lime `#C8FF3D`, electric cyan `#33E6FF`, hot pink `#FF2E93`, orange `#FF9A3D`
- Menu structure: 좌측 대형 로고와 가이드, 우측 대형 타이틀 메뉴, 3열 선곡 화면
- Result structure: 좌측 프로필, 중앙 곡/분석, 우측 통계와 액션을 분리한 비대칭 보드
- Notes: 투명 네온 화살표, 모드별 회전 배열, 검은 플레이필드
- Readability: 배경의 중앙 정보 영역을 어둡게 유지하고 장식은 가장자리와 원근선에 집중

## Supported modes

4K, 7K, 10K, 16K를 전용 색상 및 회전 배열로 튜닝했습니다. 다른 1K~16K 모드도
공통 정의로 안전하게 표시되지만 방향 배열은 별도로 최적화하지 않았습니다.

## Install

1. 이 폴더 또는 `skin.json`을 TenRiff의 `Options > Skins` 화면에 드래그합니다.
2. `Velocity Circuit: Dance Grid`를 선택합니다.
3. `F5` 또는 `Reload Skin`을 누릅니다.

## Assets and provenance

- `lobby/screens/*.png`, `gameplay/background.png`: OpenAI 내장 이미지 생성 도구로 이 스킨을 위해 새로 생성
- `lobby/logo.png`, `gameplay/note-*.png`, `gameplay/key-*.png`, `gameplay/hold-body-*.png`: `generate_assets.py`가 Pillow로 생성한 오리지널 도형
- Font: Windows 기본 Bahnschrift 또는 DejaVu Sans fallback
- External/commercial game assets: none

배경은 텍스트와 로고가 없는 원본 추상 이미지입니다. 정확한 워드마크와 화살표는 재현 가능한
로컬 스크립트로 생성해 글자 오류와 저작권 혼동을 피했습니다.

## Intentional Native fallbacks

판정 텍스트, 점수 글꼴, 패널 내부의 실제 컨트롤과 BGA는 TenRiff Native 렌더링을 사용합니다.
스킨은 배경, 팔레트, 레이아웃, 화살표 노트·리셉터·LN 바디를 담당합니다.

## Regenerate deterministic assets

```powershell
python skins/TenRiff_VelocityCircuit_DanceGrid_4K-16K/generate_assets.py
```
