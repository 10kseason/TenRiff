# Agent Prism / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# Agent Prism Universal

AI 에이전트가 TenRiff 스킨 포맷만으로 어디까지 바꿀 수 있는지 보여 주는 1K~16K 범용 예제다.
로비·화면별 배경, 전체 테마, 일부 레이아웃, 인게임 배경, Native 노트 스타일, 키 모드별
레인 팔레트를 함께 사용한다.

## 사용

1. 게임에서 `Options > Skins > Import Skin`을 선택한다.
2. 이 폴더를 고르거나 스킨 설정 화면에 드래그한다.
3. `Skin Source`를 `TenRiff`로 두고 이 스킨을 선택한다.
4. 파일을 수정한 뒤 `F5`를 누르면 다시 읽는다.

## 구성

- `lobby/background.png`: 메뉴 중앙을 비운 프리즘 로비 배경
- `gameplay/background.png`: 플레이필드 가독성을 우선한 어두운 인게임 배경
- `theme`: 시안·바이올렛·코랄 팔레트로 메뉴와 장면 색상 변경
- `layout`: 타이틀과 스킨 미리보기의 위치 예시
- `gameplay.modes`: 4K, 5K, 7K, 8K, 10K, 12K, 14K, 16K 전용 대칭 팔레트
- 그 밖의 키 모드: 기본 시안 팔레트와 동일한 Native 스타일 사용

노트·LN·리셉터 이미지는 일부러 넣지 않았다. 누락 슬롯이 Native 렌더링으로 안전하게
대체되는 구조와, 이미지 없이 `note_shape`, `lane_colors`, 효과 설정만으로 완성형 스킨을
만드는 방식을 보여 주기 위해서다.

## 자산 출처

두 배경은 2026-08-25에 OpenAI 내장 이미지 생성 도구로 이 예제를 위해 새로 생성했다.
외부 게임이나 기존 스킨의 자산은 사용하지 않았다. 저장소의 MIT 라이선스를 따른다.

제작 절차는 [`docs/skin-agent-guide.md`](../../../docs/skin-agent-guide.md), 전체 필드는
[`docs/skin-format.md`](../../../docs/skin-format.md)를 참고한다.
