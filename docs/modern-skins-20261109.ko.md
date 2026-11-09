> **1.9.9.9-final3 반영:** 아래 로컬 후보 기록은 제작 이력입니다. [최종 사용 안내](release-1.9.9.9-final3.ko.md).

# Modern Common 내장 스킨 재디자인

사용자 선택: 현대적인 공통 스타일로 전체 재디자인. 내장12개 스킨을 같은 네이티브 메뉴·기어·노트·HUD 규격에 맞췄습니다. 색상 테마로 구분하며 폴더 이름과 v1 스킨 형식은 보존합니다.

- 공통 메뉴: 낮은 대비의 기하학 배경, 명확한 밝은 글자, 일정한 카드·목록·여백. 이전 절대좌표 메뉴 배치 대신 현재 네이티브 레이아웃을 사용합니다.
- 공통 플레이: 검정 읽기 영역, 확실한 색 구분의 불투명 노트 본체와 얇은 하이라이트, 별도 LN 꼬리, 낮춘 기어 높이, 작은 키 이동, 약한 글로우, 공통 글꼴 계층.
- 지원: 모든4~16키, 기존1~3키, 고스트, 스킨 미리보기, 사용자 판정선 위치·명도·노트 크기. 점수/판정/입력/오디오/리플레이와 랭킹 정책은 그대로입니다. 판정·게이지·등급의 의미 색상은 공통 엔진 값을 유지합니다.

## 테마

| 폴더 | 표시 이름 |
|---|---|
| LUNAR-FRACTURE-Void-Garden-7K | Void Garden / Modern |
| SunShine-Light-er-Arrow-7K | SunShine / Modern |
| Tencircle | Tencircle / Modern |
| Tengear | Tengear / Modern |
| Tenmania | Tenmania / Modern |
| TenRiff_AgentPrism_Universal_1K-16K | Agent Prism / Modern |
| TenRiff_NativeEditable | Native Editable / Modern |
| TenRiff_NeonStep_Arcade_Universal_4K-10K | Neon Step / Modern |
| TenRiff_Pulse_Mania_Universal_4K-10K | Pulse Mania / Modern |
| TenRiff_SimpleSquare | Simple Square / Modern |
| TenRiff_Studio | Studio / Modern |
| TenRiff_VelocityCircuit_DanceGrid_4K-16K | Velocity Circuit / Modern |

## 원본 보존과 재생성

각 폴더의 skin.legacy.json, README.legacy.md와 기존 이미지가 이전 디자인을 보존합니다. Modern 파일을 보관하고 legacy manifest를 skin.json으로 복사하면 복원할 수 있습니다. 프로필 폴더에 같은 이름의 사용자 스킨이 있으면 기존처럼 그 스킨이 우선합니다. 새 로컬 후보는 별도 modern-skins-local 프로필을 사용합니다.

공통 컴포넌트와 팔레트의 단일 원본은 tools/generate_modern_skins.py입니다. `python -B tools/generate_modern_skins.py --check`는 파일을 쓰지 않고 재생성 결과를 검사합니다. `--write`는 명시한12개 내장 폴더의 현재 Modern manifest/배경만 다시 만들며 legacy manifest는 재작성하지 않습니다. 사용자 편집은 먼저 따로 보관하세요. Pillow는 소스 재생성에만 필요합니다.

과거 이름에7K/4K-10K가 들어간 폴더도 호환성을 위해 이름을 유지합니다. 현재 Modern manifest는1~16키 전체에 색상 매핑을 제공합니다. 기존 설정/프리셋은 초기화하지 않습니다. 별도 후보이며 공개 릴리스·웹 배포는 하지 않았습니다. 실제 물리 입력·청취·장시간·다른 PC와 임의 사용자 스킨은 별도 검증입니다.
