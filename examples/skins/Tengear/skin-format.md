# TenRiff 스킨 포맷 v1

TenRiff 스킨은 한 폴더의 `skin.json`과 PNG/JPG/BMP 이미지로 로비와 인게임 외형을 함께 바꾸는 선택형 포맷이다. 모든 항목은 선택 사항이며, 비어 있거나 누락된 슬롯은 기존 Native 렌더링으로 자동 대체된다.

## 빠른 시작

1. 게임에서 `Options > Skins > Create New Skin`을 누른다.
2. 자동으로 열린 폴더의 표준 파일명 자리에 이미지를 넣는다.
3. 게임으로 돌아와 `F5` 또는 `Reload Skin`을 누른다.
4. 더 세밀하게 바꾸고 싶으면 `skin.json`을 편집한다.

기존 스킨을 시작점으로 삼으려면 [`examples/skins/TenRiff-Example`](../examples/skins/TenRiff-Example)을
복사한 뒤 `Import Skin`으로 가져와도 된다. `Open Skin Folder`는 현재 선택한 TenRiff 스킨을
바로 열어 준다.

Skins 화면에 폴더나 `skin.json`을 드래그해도 가져올 수 있다. 가져온 사본은 `profiles/<profile>/skins/tenriff/<skin-name>/`에 저장된다. 같은 이름을 다시 가져오면 기존 폴더를 덮지 않고 `-2`, `-3` 접미사를 붙인다.

## 폴더 예시

```text
MySkin/
  skin.json
  lobby/
    background.png
    logo.png
    screens/
      settings_skins.png
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
  "$schema": "https://raw.githubusercontent.com/10kseason/TenRiff/main/docs/tenriff-skin.schema.json",
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

`lobby/background`, `lobby/logo`, `gameplay/background`, `gameplay/gear`, `gameplay/note`,
`gameplay/hold-head`, `gameplay/hold-body`, `gameplay/hold-tail`, `gameplay/key-idle`,
`gameplay/key-pressed`는 확장자만 `.png`, `.jpg`, `.jpeg`, `.bmp` 중 하나로 맞추면
`skin.json`에 경로를 쓰지 않아도 자동 감지한다.

## 화면별 배경과 색상 테마

`lobby/screens/<화면 ID>.png`를 넣으면 그 화면만 다른 배경을 쓴다. 같은 값은
`lobby.screen_backgrounds`와 `lobby.screen_opacities`에 직접 적을 수도 있다.

지원 화면 ID는 `quick_setup`, `title`, `options`, `multiplayer`, `song_select`,
`session_mix`, `song_browser`, `settings`, `settings_audio`, `settings_graphics`,
`settings_skins`, `settings_input`, `settings_calibration`, `mode_select`, `mode_mods`,
`keymap`, `keymap_confirm`, `onnx_upscaler_confirm`, `keymap_test`, `result`이다.
세부 설정 화면에 전용 배경이 없으면 `settings`, 곡 브라우저와 멀티플레이에는
`song_select`, 마지막으로 공용 `lobby.background` 순서로 대체된다.

```json
"lobby": {
  "screen_backgrounds": {
    "title": "lobby/screens/title.jpg",
    "settings_skins": "lobby/screens/settings_skins.png"
  },
  "screen_opacities": { "title": 0.9, "settings_skins": 0.75 }
},
"theme": {
  "accent": "#6EE7F2",
  "text": "#F4F7FF",
  "muted": "#9AA3AD",
  "card": "#1F2130E8",
  "panel": "#14141CB8",
  "button": "#242638",
  "button_selected": "#6EE7F238",
  "border": "#31344A",
  "scene_primary": "#61D6FA",
  "scene_secondary": "#8F9EFA",
  "scene_background": "#04081AFF",
  "judgement_line": "#FF4D6D",
  "lane_divider": "#F6F8FF"
}
```

색은 `#RRGGBB` 또는 `#RRGGBBAA`다. 빠진 항목은 기본 팔레트를 유지한다.

## 키 모드 공통화와 패턴 경로

레인별 파일 이름을 전부 배열로 쓰는 대신 문자열 패턴을 쓸 수 있다.

```json
"gameplay": {
  "note": "gameplay/note-{lane}.png",
  "lane_map": ["left", "down", "up", "right"],
  "modes": {
    "4k": {
      "note": "gameplay/4k/note-{index:02}.png",
      "note_rotations": [270, 180, 0, 90]
    },
    "7k": { "note": "gameplay/7k/note-{index}.png" }
  }
}
```

- `{lane}`: `lane_map`의 현재 값
- `{index}`: 1부터 시작하는 레인 번호
- `{index:02}`: 두 자리 레인 번호 (`01`, `02`, ...)
- `modes`: `1k`부터 `16k`까지의 얕은 덮어쓰기. 공통 항목을 먼저 쓰고 모드마다 다른 항목만 적는다.

가져오기는 현재 선택한 모드뿐 아니라 `1k..16k`의 모든 참조 자산을 함께 복사한다.

## 게임플레이 스타일

스킨은 이미지뿐 아니라 아래 시각 옵션도 기본값으로 지정할 수 있다.

`show_lane_dividers`, `show_judgement_line`, `show_timing_feedback`,
`show_gear_boundary_line`, `show_hold_tail`, `hold_tail_taper`, `judgement_line_glow`,
`key_pulse`, `key_pulse_brightness`, `hit_burst_style`, `key_label_position`,
`note_border`, `note_shape`, `lane_colors`, `lane_background_opacity`, `black_playfield`,
`visual_opacity`, `note_outline_opacity`, `hold_body_opacity`.

매니페스트에 명시한 값은 스킨 제작자의 의도대로 플레이어의 같은 시각 옵션보다 우선하며,
누락한 값만 플레이어 설정을 사용한다. 허용 값과 범위는
[`tenriff-skin.schema.json`](tenriff-skin.schema.json)에 정의돼 있다.

## 이미지 슬롯

| 섹션 | 키 | 동작 |
|---|---|---|
| `lobby` | `background` | 게임플레이를 제외한 로비/메뉴 전체 배경 |
| `lobby` | `logo` | Title 중앙 및 Song Select 좌측 상단 워드마크 슬롯 |
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

## 화살표 스킨

DDR/StepMania식 화살표 노트는 정사각형에 가까운 이미지를 쓴다. 기본값에서 노트 이미지는
노트 사각형(레인 폭 × 노트 높이)에 그대로 늘려 그리므로, 화살표를 넣으면 세로로 눌려 나온다.
리셉터는 `full_lane_receptors`로 높은 사각형을 받아 멀쩡한데 떨어지는 노트만 납작해 보인다면
이게 원인이다.

`note_aspect`로 이미지를 어떻게 채울지 고른다.

| 값 | 동작 |
|---|---|
| `stretch` | 노트 사각형을 그대로 채운다. 기본값이고 막대형 노트에 맞다 |
| `contain` | 비율을 지키며 사각형 **안에** 넣는다. 노트가 작아진다 |
| `width` | 폭은 레인에 맞추고 **높이를 이미지 비율에서 뽑는다**. 화살표는 이걸 쓴다 |

```json
"gameplay": {
  "note": "gameplay/arrow.png",
  "note_aspect": "width",
  "note_rotations": [270, 180, 0, 90],
  "full_lane_receptors": true
}
```

`note_rotations`는 레인별로 이미지를 시계 방향으로 몇 도 돌릴지 적는다(도 단위, 이미지 중심 기준).
위를 향한 화살표 하나만 넣고 4키에 `[270, 180, 0, 90]`을 주면 좌·하·상·우가 된다.
레인마다 미리 돌려 놓은 PNG를 따로 만들 필요가 없다.

- 적용 대상: `note`, `hold_head`, `hold_tail`, 그리고 리셉터(`key_idle`, `key_pressed`).
- `hold_body`는 레인 방향으로 늘어나는 이미지라 회전하지 않는다.
- `key_rotations`를 따로 적으면 리셉터만 다른 각도를 쓴다. 비우면 `note_rotations`를 따라간다.
- 배열이 레인 수보다 짧으면 모자란 레인은 회전 `0`으로 둔다.
- 스킨이 `note_aspect`를 적으면 게임의 `Options > Skins > Image Aspect` 토글보다 우선한다.
  적지 않으면 그 토글이 `contain`/`stretch`를 고른다.

## 레이아웃 슬롯

`layout`은 이미지가 아니라 **메뉴 요소의 위치**를 옮긴다. 좌표는 렌더러가 쓰는
`1920x1080` 기준 공간이고 순서는 `[left, top, right, bottom]`이다. 실제 창 크기로는
엔진이 알아서 스케일하므로 해상도별로 따로 적을 필요가 없다. 적지 않은 슬롯은
기본 위치를 그대로 쓴다.

```json
"layout": {
  "song_select": {
    "logo": [48, 18, 388, 108],
    "left_panel": [38, 152, 486, 922]
  },
  "title": {
    "buttons": [470, 360, 1450, 940]
  }
}
```

| 화면 | 슬롯 | 기본값 | 대상 |
|---|---|---|---|
| `title` | `spectrum` | `[696, 66, 1224, 150]` | 상단 스펙트럼 막대 18개 |
| `title` | `logo` | 워드마크 크기에 맞춰 가운데 정렬, 위 `184` | `TenRiff` 워드마크 |
| `title` | `buttons` | `[470, 360, 1450, 940]` | 메뉴 버튼 스택 전체 |
| `title` | `guide` | `[1492, 386, 1834, 924]` | 우측 가이드 패널 |
| `title` | `footer` | `[80, 972, 1840, 1056]` | 하단 상태 바 |
| `song_select` | `top_bar` | `[0, 0, 1920, 126]` | 상단 바 |
| `song_select` | `logo` | `[48, 18, 388, 108]` | `lobby.logo` 이미지 슬롯 |
| `song_select` | `nav` | `[492, 12, 1244, 126]` | 상단 탭 4개 |
| `song_select` | `profile` | `[1518, 16, 1880, 112]` | 프로필 패널 |
| `song_select` | `left_panel` | `[38, 152, 486, 922]` | 곡 라이브러리 |
| `song_select` | `center_panel` | `[510, 152, 1266, 922]` | 곡 목록 |
| `song_select` | `right_panel` | `[1290, 152, 1882, 922]` | 난이도 패널 |
| `song_select` | `bottom_bar` | `[38, 944, 1882, 1048]` | 하단 바 |

그 밖에도 `generic.content`, `generic.preview`, `result.profile`, `result.song_panel`,
`result.analysis_panel`, `result.stats_panel`, `result.continue`, `result.replay`,
`result.retry`를 옮길 수 있다. 목록형 화면 ID에는 `content`와 `preview`를 직접 지정할 수 있다.
예를 들어 `layout.settings_skins.content`가 있으면 스킨 설정 화면에만 적용하고, 없으면
`layout.settings.content`, 다시 없으면 `layout.generic.content`를 사용한다.

- 패널 안의 제목, 카드, 버튼은 패널 사각형에서 계산하므로 함께 따라 움직인다.
- 클릭 판정 영역도 같은 사각형을 쓰므로 마우스 입력이 어긋나지 않는다.
- `spectrum`과 `nav`는 막대/탭 개수를 유지한 채 사각형에 맞춰 비례 조정된다.
- `title.footer`를 옮기면 그 위 버튼 스택이 쓰는 세로 공간도 따라 바뀐다.
- `title.buttons`와 `title.guide`의 기본값은 성능 오버레이 표시 여부에 따라 조금 달라진다.
- `right > left`, `bottom > top`이 아니거나 좌표 크기가 `8192`를 넘으면 그 슬롯은
  무시하고 경고를 남긴다. 슬롯 이름을 잘못 적어도 조용히 넘어가지 않고 경고가 뜬다.

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
- 알 수 없는 키, 잘못된 타입, 범위를 벗어난 값은 스킨 설정 화면과 로그에 경고한다.
- 렌더 스레드는 매니페스트를 직접 읽지 않는다. `Reload Skin` 때 1K~16K 정의를 다시 만들고
  안전한 스냅샷으로 교체하므로 플레이 중 디스크/JSON 읽기가 끼어들지 않는다.
- JSON 자동 완성/검증은 [`tenriff-skin.schema.json`](tenriff-skin.schema.json)을 사용할 수 있다.
