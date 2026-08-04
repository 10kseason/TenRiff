# TenRiff 스킨 포맷 v1

TenRiff 스킨은 한 폴더의 `skin.json`과 PNG/JPG/BMP 이미지로 로비와 인게임 외형을 함께 바꾸는 선택형 포맷이다. 모든 항목은 선택 사항이며, 비어 있거나 누락된 슬롯은 기존 Native 렌더링으로 자동 대체된다.

## 빠른 시작

1. [`examples/skins/TenRiff-Example`](../examples/skins/TenRiff-Example) 폴더를 복사한다.
2. 원하는 이미지를 복사한 폴더 안에 넣는다.
3. `skin.json`의 빈 문자열을 이미지 상대 경로로 바꾼다.
4. 게임에서 `Options > Skins > Import Skin`을 누르고 그 폴더를 선택한다.
5. `Skin Source`를 `TenRiff`로 바꾸고 `TenRiff Skin` 행에서 설치한 스킨을 고른다.

Skins 화면에 폴더나 `skin.json`을 드래그해도 가져올 수 있다. 가져온 사본은 `profiles/<profile>/skins/tenriff/<skin-name>/`에 저장된다. 같은 이름을 다시 가져오면 기존 폴더를 덮지 않고 `-2`, `-3` 접미사를 붙인다.

## 폴더 예시

```text
MySkin/
  skin.json
  lobby/
    background.png
    logo.png
  gameplay/
    background.jpg
    gear.png
    note.png
    hold-head.png
    hold-body.png
    hold-tail.png
    key-idle.png
    key-pressed.png
```

## `skin.json`

```json
{
  "$schema": "../../../docs/tenriff-skin.schema.json",
  "format": "tenriff-skin",
  "version": 1,
  "name": "My Skin",
  "author": "Your Name",
  "lobby": {
    "background": "lobby/background.png",
    "logo": "lobby/logo.png",
    "background_opacity": 0.72
  },
  "gameplay": {
    "background": "gameplay/background.jpg",
    "background_opacity": 0.66,
    "gear": "gameplay/gear.png",
    "note": "gameplay/note.png",
    "hold_head": "gameplay/hold-head.png",
    "hold_body": "gameplay/hold-body.png",
    "hold_tail": "gameplay/hold-tail.png",
    "key_idle": "gameplay/key-idle.png",
    "key_pressed": "gameplay/key-pressed.png",
    "note_width_ratio": 1.0,
    "note_height_ratio": 1.0,
    "judgement_line_position": 0.82,
    "full_lane_receptors": false
  }
}
```

## 이미지 슬롯

| 섹션 | 키 | 동작 |
|---|---|---|
| `lobby` | `background` | 게임플레이를 제외한 로비/메뉴 전체 배경 |
| `lobby` | `logo` | Song Select 좌측 상단 TenRiff 워드마크 슬롯 |
| `gameplay` | `background` | 차트 BGA 아래에 그리는 인게임 기본 배경 |
| `gameplay` | `gear` | 레인과 판정선 위에 그리는 플레이필드 오버레이 |
| `gameplay` | `note` | 일반 노트 머리 |
| `gameplay` | `hold_head` | 롱노트 머리. 비우면 `note` 사용 |
| `gameplay` | `hold_body` | 롱노트 몸통. 비우면 기본 몸통 사용 |
| `gameplay` | `hold_tail` | 롱노트 꼬리. 비우면 `hold_head` 사용 |
| `gameplay` | `key_idle` | 판정선의 대기 키 이미지 |
| `gameplay` | `key_pressed` | 키 입력 순간 이미지. 비우면 `key_idle` 사용 |

`note`, `hold_head`, `hold_body`, `hold_tail`, `key_idle`, `key_pressed`는 문자열 하나 또는 문자열 배열을 받는다. 문자열 하나는 모든 레인에 공용으로 사용한다. 배열은 왼쪽 레인부터 순서대로 적용되므로 osu!mania처럼 레인별 이미지를 만들 수 있다.

```json
"note": [
  "gameplay/note-white.png",
  "gameplay/note-blue.png",
  "gameplay/note-white.png",
  "gameplay/note-blue.png"
]
```

## 크기와 좌표

- 로비/인게임 배경 권장 크기: `1920x1080` 이상, 16:9.
- 로고는 투명 PNG를 권장하며 원본 종횡비를 유지한다.
- 노트와 키 이미지는 투명 PNG를 권장한다.
- `background_opacity`: `0.0..1.0`.
- `note_width_ratio`, `note_height_ratio`: `0.1..4.0`.
- `judgement_line_position`: 화면 위 `0.0`, 아래 `1.0` 기준이며 허용 범위는 `0.05..0.98`이다.
- `column_widths`: 레인별 상대 너비 숫자 배열, 최대 16개.
- `column_spacings`: 레인 사이 상대 간격 숫자 배열, 최대 15개.

## 안전성과 호환성

- 지원 이미지: `.png`, `.jpg`, `.jpeg`, `.bmp`.
- 모든 이미지 경로는 `skin.json` 기준 상대 경로여야 한다.
- 절대 경로와 `..`로 스킨 폴더 밖을 가리키는 경로는 거부된다.
- 포맷 버전이 지원되지 않거나 필수 메타데이터가 틀리면 스킨을 활성화하지 않는다.
- 기존 `native` 및 `lr2` 소스와 설정은 그대로 유지된다.
- JSON 자동 완성/검증은 [`tenriff-skin.schema.json`](tenriff-skin.schema.json)을 사용할 수 있다.
