# Velocity Circuit / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

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
