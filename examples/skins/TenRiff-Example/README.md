# TenRiff Example Skin

이 폴더를 복사한 뒤 `skin.json`의 빈 이미지 경로만 채우거나, 표준 파일명으로
이미지를 넣으면 된다. 게임 안에서는 `Create New Skin`이 같은 용도의 최소 템플릿을 만든다.

- 로비 배경/로고: `lobby/`
- 화면별 배경: `lobby/screens/<screen-id>.png`
- 노트/롱노트/키/기어/인게임 배경: `gameplay/`
- 색상, 화면별 레이아웃, 4K 모드 스타일 예시는 현재 `skin.json`에 포함돼 있다.
- 전체 필드와 배열 사용법: [`docs/skin-format.md`](../../../docs/skin-format.md)

빈 슬롯은 오류가 아니며 TenRiff 기본 렌더링으로 대체된다. 파일을 교체한 뒤
스킨 설정 화면에서 `F5`를 누르면 즉시 다시 읽는다.
