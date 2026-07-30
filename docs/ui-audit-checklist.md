# UI Audit Checklist

렌더러 레이아웃을 건드렸다면 이 체크리스트를 최소 수동 검증 기준으로 사용합니다. 특히 서브 에이전트가 UI 관련 리팩터를 한 뒤에는 이 문서를 기준으로 깨짐 여부를 반드시 확인합니다.

## Verification Matrix
- 해상도:
  - `1080p`
  - `720p windowed`
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
  - `Result`

## Pass / Fail Rules
- 텍스트가 패널 밖으로 새어나가지 않아야 한다.
- 패널이 다른 패널, `Performance HUD`, footer, hint bar와 겹치지 않아야 한다.
- 버튼, `+ / -` 조절 UI, 재생/뒤로가기 같은 actionable control이 가려지지 않아야 한다.
- 긴 제목, 파일명, 경로, 힌트 문구는 clip 되더라도 다른 영역을 침범하면 안 된다.
- centered / trailing 텍스트는 다른 화면에서 남은 alignment 상태에 영향받지 않아야 한다.
- `Combo Y`는 preview와 gameplay 둘 다 중앙 정렬 상태를 유지한 채 세로 위치만 바꿔야 한다.

## Screen-Specific Checks
- `Title`
  - `GUIDE` 패널이 버튼, footer, Performance HUD와 겹치지 않는지 확인
  - guide line 수가 많아져도 panel 바깥으로 새지 않는지 확인
- `Song Select`
  - header, 좌측 nav, list card, 우측 detail panel, footer hint가 한 safe area 안에 있는지 확인
  - `Songs / Sources / Records` 전환 시 우측 정보 패널 row가 넘치지 않는지 확인
  - BMS 폴더 또는 BMS 파일을 drop했을 때 해당 폴더가 활성 song source가 되고 BMS 계열 차트만 인덱싱되는지 확인
  - 인덱싱 stage/퍼센트/ETA와 progress bar가 화면 중앙에 유지되는지 확인
  - `- / +`로 다음 플레이 Rate를 조절할 수 있고 검색 입력 중에는 Rate hotkey가 개입하지 않는지 확인
  - 로컬 난이도표 JSON을 선택/해제했을 때 배지·정렬·그룹·필터가 표 레벨과 native LV 사이에서 일관되게 전환되는지 확인
- `Skin Settings`
  - `Judge Line`, `Note Height`, `Combo Y`를 바꿨을 때 우측 preview가 계속 패널 내부에 남는지 확인
  - combo 숫자가 preview 중앙에 정렬되고 좌측으로 쏠리지 않는지 확인
  - LR2 playskin 폴더 선택과 drag-and-drop 모두 가져오기 후 새 스킨을 즉시 활성화하는지 확인
  - 같은 이름의 LR2 스킨이 이미 있으면 기존 폴더를 덮어쓰지 않고 충돌 없는 새 폴더에 설치하는지 확인
  - 실제 PNG 노트, LN head/body/tail, key 이미지와 lane fallback이 gameplay에서 의도대로 보이는지 확인
- `Generic/settings lists`
  - row label/value, notes/help text, scrollbar, `+ / -` 버튼이 서로 겹치지 않는지 확인
  - 긴 목록의 scrollbar track을 클릭하면 해당 위치의 row가 선택만 되고 값 변경이나 화면 진입은 일어나지 않는지 확인
  - 공간 때문에 help text가 생략되면 마지막 보이는 줄에 `F1`과 남은 줄 수가 표시되는지 확인
  - Performance HUD가 켜져 있을 때 우상단 영역이 option row를 가리지 않는지 확인
- `Gameplay`
  - `Loading`, `Countdown`, `Live HUD` 모두 Performance HUD와 분리되어 보이는지 확인
  - score/combo/judge header text가 우상단 overlay 뒤로 숨지 않는지 확인
  - combo 숫자와 feedback text가 서로 겹치지 않는지 확인
- `Result`
  - gauge panel이 Performance HUD와 겹치지 않는지 확인
  - replay/result file text, notes, button stack이 서로 겹치지 않는지 확인
- `Discord Game Overlay`
  - `Borderless`에서 Voice 참가자/발화자 표시가 gameplay 진입과 그래픽 live-apply 뒤에도 유지되는지 확인
  - `Exclusive Fullscreen`은 현재 Discord overlay 비호환 모드로 UI/문서에 명확히 표시되는지 확인

## Minimum Commands
- Build:
  - `cmake --build build-dist --config Release --target tenriff`

## When To Re-Run
- `src/render/MenuWindow_draw*.inl` 변경 후
- `src/render/MenuWindow.cpp`의 layout helper 변경 후
- skin preview / gameplay HUD / result panel 배치 변경 후
- 서브 에이전트가 renderer refactor를 수행한 뒤
