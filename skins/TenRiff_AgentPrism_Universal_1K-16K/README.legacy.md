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
