# 곡 소스와 기본 난이도표

## 곡 소스 관리

선곡 화면의 **소스 / SOURCES**를 선택하면 저장된 곡 폴더 목록을 볼 수 있습니다.

- **폴더 추가 [F2]**: 폴더 선택 창에서 곡 폴더를 추가합니다. 폴더 드래그 앤 드롭도 유지합니다.
- **목록에서 제거 [Del]**: 선택한 소스의 저장된 참조만 제거합니다. 실제 폴더, 차트, 키음과 기존 캐시 파일을 삭제하지 않습니다.
- 현재 소스를 제거하면 남은 소스를 열고, 마지막 소스를 제거하면 빈 목록으로 전환합니다. 빈 목록은 재실행 후에도 유지됩니다. 다시 폴더를 추가하면 원래 파일을 사용할 수 있습니다.
- `Del`은 소스 목록에 포커스가 있을 때만 소스를 제거합니다. 곡 목록에서의 기존 세션 믹스 초안 삭제 동작은 유지됩니다.

`ui.song_sources_initialized`는 한 번 소스 목록을 사용한 뒤 의도적으로 비워 둔 상태를 처음 실행한 상태와 구분합니다. 기존 프로필은 처음 로드할 때 종전 소스 선택을 유지합니다. 사용자가 명시한 다른 `--songs` 경로는 빈 목록에서도 열 수 있습니다.

## 기본 난이도표

선곡 중앙의 **난이도표 / 선택** 카드를 누르면 아래 아홉 항목을 3×3 버튼 또는 F1–F9로 선택할 수 있습니다. 선택 창이 열려 있는 동안 F8 채팅과 F9 스크린샷 대신 해당 표를 선택합니다.

| 단축키 | 선택 이름 | 공식 주소 |
|---|---|---|
| F1 | 5키 에리 | https://asumatoki.kr/table/aery/header.json |
| F2 | 7키 에리 | https://asumatoki.kr/table/aery7/header.json |
| F3 | 10키 리바이브 | https://calc.10k-revive.cloud/table.html |
| F4 | 기본 LV / Native LV | 로컬 곡 메타데이터 사용 |
| F5 | 스텔라 | https://stellabms.xyz/st/table.html |
| F6 | 새틀라이트 | https://stellabms.xyz/sl/table.html |
| F7 | 4키 U_E 팩 | https://classmaterma.github.io/4UE/table.html |
| F8 | 6키 U_E 팩 | https://classmaterma.github.io/UE/table.html |
| F9 | 8키 U_E 팩 | https://classmaterma.github.io/8UE/table.html |

선택 시 기존 BMSTable 가져오기 기능으로 표를 내려받아 프로필에 캐시하고 적용합니다. 각 HTML의 상대 `bmstable` 헤더 링크도 처리합니다. 새 표 가져오기에 실패하면 기존 표를 유지하며 오류를 표시합니다. 앱을 실행할 때마다 모든 표를 자동 다운로드하지 않습니다.

U_E 팩 4/6/8키 주소는 [U_E 팩 공식 안내 페이지](https://sites.google.com/view/6k4kbms/)에서 연결하는 표입니다. 표 헤더 이름은 각각 `4UE Difficulty Table`, `6UE Difficulty Table`, `8UE Difficulty Table`입니다. 스텔라·새틀라이트·U_E 팩는 페이지의 `header.json`을 통해 `score.json`을 가져옵니다.

같은 창에서 사용자 URL을 입력할 수 있고, 카드 옆 **파일**은 로컬 JSON을 고릅니다. 표 선택값은 프로필에 저장됩니다. 표에 포함된 차트의 실제 BMS/오디오는 별도로 보유해야 합니다.

**F4 기본 LV**를 누르면 외부 표의 URL·경로와 표 레벨을 해제하고 선택 창을 닫습니다. 카드 옆 **기본** 버튼도 같은 동작을 합니다. 표시·정렬·그룹·숫자 난이도 필터가 모두 기본 LV를 사용하며, 정상 곡 캐시가 있으면 전체 재스캔 없이 즉시 적용합니다. 기본값인 `mode.calculate_song_index_difficulty=false`에서는 BMS `#PLAYLEVEL`을 사용하고, 자체 난이도 계산을 켜서 저장한 캐시에는 계산된 LV를 사용합니다.
