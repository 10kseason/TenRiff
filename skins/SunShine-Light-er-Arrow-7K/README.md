# SunShine / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# SunShine Light-er

밝고 화사한 태양광 계열의 TenRiff 7K 스킨입니다.
이번 버전은 **화살표 노트(Arrow Note)** 중심으로 설계했습니다.

## 콘셉트
- 푸른 하늘 + 햇빛 + 유리같은 밝은 플레이필드
- 7K 대칭 레인 컬러
  - White / Orange / Cyan / Sun Yellow / Cyan / Orange / White
- 일반 노트와 롱노트 헤드를 모두 화살표 형태로 제작
- 중앙 레인은 살짝 넓고 태양색으로 강조
- 밝은 스킨이지만 플레이필드 중앙은 약간 차분하게 정리해서 가독성을 유지

## 권장 설정
- Judge Line: 0.825
- Note Width: 0.96
- Note Height: 0.92
- Full Lane Receptors: On

## 설치
1. TenRiff에서 `Options > Skins > Import Skin`
2. 이 폴더 또는 `skin.json` 선택
3. `Skin Source = TenRiff`
4. `SunShine Light-er` 선택

## 포함 자산
- lobby/background.jpg
- lobby/logo.png
- gameplay/background.jpg
- gameplay/gear.png
- gameplay/note-*.png
- gameplay/hold-head-*.png
- gameplay/hold-body-*.png
- gameplay/hold-tail-*.png
- gameplay/key-idle-*.png
- gameplay/key-pressed-*.png

실제 렌더러에서 1080p / 720p, Performance HUD On/Off, Borderless + Discord Overlay 조합 검증을 권장합니다.
