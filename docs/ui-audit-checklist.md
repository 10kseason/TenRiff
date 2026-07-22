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
  - `.osz`를 `Shift+F2`로 선택하거나 창에 drop했을 때 활성 song source에 설치되고, osu chart가 자동 활성화된 뒤 재인덱싱 결과에서 곧바로 선택되는지 확인
- `Skin Settings`
  - `Judge Line`, `Note Height`, `Combo Y`를 바꿨을 때 우측 preview가 계속 패널 내부에 남는지 확인
  - combo 숫자가 preview 중앙에 정렬되고 좌측으로 쏠리지 않는지 확인
  - `.osk` 파일 선택과 drag-and-drop 모두 설치 후 새 스킨을 즉시 활성화하는지 확인
  - 같은 이름의 스킨이 이미 있으면 기존 폴더를 덮어쓰지 않고 충돌 없는 새 폴더에 설치하는지 확인
  - 실제 PNG 노트, LN head/body/tail, key 이미지와 lane fallback이 gameplay에서 의도대로 보이는지 확인
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
