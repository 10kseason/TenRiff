# UI Audit Checklist

렌더러 레이아웃을 건드렸다면 이 체크리스트를 최소 수동 검증 기준으로 사용합니다. 특히 서브 에이전트가 UI 관련 리팩터를 한 뒤에는 이 문서를 기준으로 깨짐 여부를 반드시 확인합니다.

## Verification Matrix
- 해상도:
  - `1080p`
  - `720p windowed`
  - `960x540` 작은 창
- Performance HUD:
  - `Off`
  - `On`
- External overlay (manual Windows check):
  - Discord Voice widget pinned at bottom-left in `borderless`
  - verify both Performance HUD `Off` and `On`; avoid placing both overlays in the same corner
- 확인 화면:
  - `Title`
  - `Song Select / Songs`
  - `Song Select / Sources`
  - `Song Select / Records`
  - `Song Select / Browse` 관련 generic screens
  - `Settings / Skin Settings`
  - `Settings / generic lists` (`Audio`, `Graphics`, `Input`, `Mode`, `Keymap` 관련 row UI)
  - `Help overlay`
  - `Gameplay / Loading`
  - `Gameplay / Countdown`
  - `Gameplay / Live HUD`
    - P-GREAT만 노란색·무지개 반짝임·팝을 사용하고 GREAT/GOOD은 정지된 청록색/회색인지 확인
  - FAST/SLOW는 해당 방향만 표시하고, 키빔 감쇠는 렌더 주기에 이어지는지 확인. 지속 60/144 FPS는 별도 성능 측정으로 검증
- `Multiplayer`
  - 3인·8인 상태에서 인게임 상대 카드와 결과 참가자 전원이 보이는지 확인
  - 점수 비교 막대가 판정 글자와 겹치지 않고 최고 점수 상대를 기준으로 움직이는지 확인
  - 동점·미도착·중단 상태를 구분하고 모든 상대 종료 전 관전을 끝내지 않는지 확인
- `Result`

## Pass / Fail Rules
- 텍스트가 패널 밖으로 새어나가지 않아야 한다.
- 패널이 다른 패널, `Performance HUD`, footer, hint bar와 겹치지 않아야 한다.
- 버튼, `+ / -` 조절 UI, 재생/뒤로가기 같은 actionable control이 가려지지 않아야 한다.
- 긴 제목, 파일명, 경로, 힌트 문구는 clip 되더라도 다른 영역을 침범하면 안 된다.
- centered / trailing 텍스트는 다른 화면에서 남은 alignment 상태에 영향받지 않아야 한다.
- 판정 X/Y와 콤보 X/Y는 preview와 gameplay에서 독립적으로 적용되어야 한다. X=0에서는 필드 중심 정렬을 유지한다.

## Screen-Specific Checks
- `Title`
  - `GUIDE` 패널이 버튼, footer, Performance HUD와 겹치지 않는지 확인
  - guide line 수가 많아져도 panel 바깥으로 새지 않는지 확인
- `Song Select`
  - header, 상단 nav, list card, 우측 detail panel, footer hint가 한 safe area 안에 있는지 확인
  - `Songs / Sources / Records` 전환 시 우측 정보 패널 row가 넘치지 않는지 확인
  - BMS 폴더 또는 BMS 파일을 drop했을 때 해당 폴더가 활성 song source가 되고 BMS 계열 차트만 인덱싱되는지 확인
  - 인덱싱 stage/퍼센트/ETA와 progress bar가 화면 중앙에 유지되는지 확인
  - `- / +`로 다음 플레이 Rate를 조절할 수 있고 검색 입력 중에는 Rate hotkey가 개입하지 않는지 확인
  - 로컬 난이도표 JSON을 선택/해제했을 때 배지·정렬·그룹·필터가 표 레벨과 native LV 사이에서 일관되게 전환되는지 확인
- `Options`
  - 5열×2행 카드 10개가 모두 보이고 Mods/Key Test를 포함해 클릭·방향키 이동이 같은 대상에 연결되는지 확인
  - 고유 파스텔 색과 선택 표시가 구분되고 도움말이 카드 아래에 배치되는지 확인
- `Skin Settings`
  - `Judge Line`, `Note Height`, 판정 X/Y, 콤보 X/Y를 바꿨을 때 우측 preview가 패널 내부에 남는지 확인
  - combo X=0에서 중앙 정렬되고 X 변경 시 지정한 방향으로 이동하는지 확인
  - LR2 playskin 폴더 선택과 drag-and-drop 모두 가져오기 후 새 스킨을 즉시 활성화하는지 확인
  - 같은 이름의 LR2 스킨이 이미 있으면 기존 폴더를 덮어쓰지 않고 충돌 없는 새 폴더에 설치하는지 확인
  - `LR2files`/`Theme` 루트 선택 시 정확한 `IIDX` 폴더와 IIDX 자산 의존 테마는 제외하고 바로 아래 스킨을 각각 설치하는지 확인
  - 서로 다른 Theme 하위 폴더를 가리키는 `LR2files\\Theme\\...` include/image 경로가 이식 뒤에도 해석되는지 확인
  - 실제 PNG 노트, LN head/body/tail, key 이미지와 lane fallback이 gameplay에서 의도대로 보이는지 확인
- `Generic/settings lists`
  - row label/value, notes/help text, scrollbar, `+ / -` 버튼이 서로 겹치지 않는지 확인
  - 긴 목록의 scrollbar track을 클릭하면 해당 위치의 row가 선택만 되고 값 변경이나 화면 진입은 일어나지 않는지 확인
  - 긴 도움말은 페이지 수와 이전/다음 버튼으로 모두 읽을 수 있고, 도움말 페이지 변경이 설정값·선택 행을 바꾸지 않는지 확인
  - Performance HUD가 켜져 있을 때 우상단 영역이 option row를 가리지 않는지 확인
- `Difficulty Table`
  - 선곡 카드의 URL 열기·취소, 파일 클릭, 기본 LV 복귀와 긴 표 이름 표시를 확인
  - URL 편집 중 뒤쪽 곡 선택·키 반복 입력이 작동하지 않고, 잘못된 주소가 현재 표를 지우지 않는지 확인
- `Gameplay`
  - `Loading`, `Countdown`, `Live HUD` 모두 Performance HUD와 분리되어 보이는지 확인
  - score/combo/judge header text가 우상단 overlay 뒤로 숨지 않는지 확인
  - combo 숫자와 feedback text가 서로 겹치지 않는지 확인
  - P-GREAT만 노란색·무지개 반짝임·팝을 사용하고 GREAT/GOOD은 정지된 청록색/회색인지 확인
  - FAST/SLOW는 해당 방향만 표시하고, 키빔 감쇠는 렌더 주기에 이어지는지 확인. 지속 60/144 FPS는 별도 성능 측정으로 검증
- `Multiplayer`
  - 3인·8인 상태에서 인게임 상대 카드와 결과 참가자 전원이 보이는지 확인
  - 점수 비교 막대가 판정 글자와 겹치지 않고 최고 점수 상대를 기준으로 움직이는지 확인
  - 동점·미도착·중단 상태를 구분하고 모든 상대 종료 전 관전을 끝내지 않는지 확인
- `Result`
  - gauge panel이 Performance HUD와 겹치지 않는지 확인
  - replay/result file text, notes, button stack이 서로 겹치지 않는지 확인
- `Discord Game Overlay`
  - `Borderless`에서 Voice 참가자/발화자 표시가 gameplay 진입과 그래픽 live-apply 뒤에도 유지되는지 확인
  - `Exclusive Fullscreen`은 현재 Discord overlay 비호환 모드로 UI/문서에 명확히 표시되는지 확인

## Minimum Commands
- Build:
  - `cmake --build build --config Release --target tenriff`

## When To Re-Run
- `src/render/MenuWindow_draw*.inl` 변경 후
- `src/render/MenuWindow.cpp`의 layout helper 변경 후
- skin preview / gameplay HUD / result panel 배치 변경 후
- 서브 에이전트가 renderer refactor를 수행한 뒤
