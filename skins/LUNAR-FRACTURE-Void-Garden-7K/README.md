# Void Garden / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# LUNAR FRACTURE // Void Garden 7K

TenRiff Skin Format v1용 7키 스킨입니다.

## 콘셉트
- 검정·보라·라벤더 기반의 달빛 정원
- 디지털 균열과 시안 신호선
- 7K 대칭 레인 컬러: White / Violet / Cyan / Center / Cyan / Violet / White
- 중앙 레인은 살짝 넓고, 다이아몬드 표식으로 빠르게 식별
- 게임플레이 중앙은 어둡게 유지해 BGA와 노트 가독성을 우선
- 우상단 Performance HUD와 좌하단 Discord Voice overlay를 고려해 해당 배경 영역의 명암을 낮춤

## 설치
1. 이 폴더를 그대로 유지합니다.
2. TenRiff에서 `Options > Skins > Import Skin`을 선택합니다.
3. 이 폴더 또는 `skin.json`을 지정합니다.
4. `Skin Source`를 `TenRiff`로 바꾸고 이 스킨을 활성화합니다.

## 권장값
- 1920×1080 기준
- Judge Line: 0.84
- Note Width: 0.94
- Note Height: 0.86
- Full Lane Receptors: On

## 확인 권장
실제 렌더러에서 다음 조합을 수동 확인하세요.
- 1080p / 720p windowed
- Performance HUD Off / On
- Borderless + Discord Voice widget bottom-left
- Skin Settings preview / Gameplay Loading / Countdown / Live HUD / Result

이 팩은 렌더러 코드를 변경하지 않으며, 이미지와 `skin.json`만 포함합니다.
