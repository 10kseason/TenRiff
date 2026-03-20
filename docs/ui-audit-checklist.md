# UI Audit Checklist

렌더러 레이아웃을 건드렸다면 이 체크리스트를 최소 수동 검증 기준으로 사용합니다. 특히 서브 에이전트가 UI 관련 리팩터를 한 뒤에는 이 문서를 기준으로 깨짐 여부를 반드시 확인합니다.

## Verification Matrix
- 해상도:
  - `1080p`
  - `720p windowed`
- Performance HUD:
  - `Off`
  - `On`
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
- `Skin Settings`
  - `Judge Line`, `Note Height`, `Combo Y`를 바꿨을 때 우측 preview가 계속 패널 내부에 남는지 확인
  - combo 숫자가 preview 중앙에 정렬되고 좌측으로 쏠리지 않는지 확인
- `Generic/settings lists`
  - row label/value, notes/help text, scrollbar, `+ / -` 버튼이 서로 겹치지 않는지 확인
  - Performance HUD가 켜져 있을 때 우상단 영역이 option row를 가리지 않는지 확인
- `Gameplay`
  - `Loading`, `Countdown`, `Live HUD` 모두 Performance HUD와 분리되어 보이는지 확인
  - score/combo/judge header text가 우상단 overlay 뒤로 숨지 않는지 확인
  - combo 숫자와 feedback text가 서로 겹치지 않는지 확인
- `Result`
  - gauge panel이 Performance HUD와 겹치지 않는지 확인
  - replay/result file text, notes, button stack이 서로 겹치지 않는지 확인

## Minimum Commands
- Build:
  - `cmake --build build-dist --config Release --target tenriff`

## When To Re-Run
- `src/render/MenuWindow_draw*.inl` 변경 후
- `src/render/MenuWindow.cpp`의 layout helper 변경 후
- skin preview / gameplay HUD / result panel 배치 변경 후
- 서브 에이전트가 renderer refactor를 수행한 뒤
