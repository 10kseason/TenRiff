# AI용 TenRiff 스킨 제작 가이드

이 문서는 AI 코딩 에이전트에게 TenRiff 스킨 제작을 맡길 때 사용하는 작업 계약이다.
포맷의 전체 필드 설명은 [`skin-format.md`](skin-format.md), 자동 완성·검증 규칙은
[`tenriff-skin.schema.json`](tenriff-skin.schema.json)이 기준이다.

바로 실행되는 완성 예제는
[`TenRiff_AgentPrism_Universal_1K-16K`](../skins/TenRiff_AgentPrism_Universal_1K-16K)을 참고한다.
최소 제작 템플릿은 [`TenRiff-Example`](../examples/skins/TenRiff-Example) 하나만 유지한다.

## 적용 기준과 지원 포맷

2026-11-09 **1.9.9.9-final3** 클라이언트와 웹/동봉 에디터 기준이다.
`version: 1`을 유지하며 기존 스킨을 계속 읽는다. 새 판정선 기본 명도와 선택형 배경
영상 필드는 final3에서 지원한다. 공개 final2 이하를 대상으로 제작할 때는 이 필드들을
생략한다. GitHub 공개 스키마 또는 함께 받은 동일 버전의 로컬 스키마로 검증한다.

| 입력 | 실제 지원과 제작 방식 |
|---|---|
| TenRiff `skin.json` v1 폴더 | 메뉴·Native 벡터·이미지 플레이 스타일, `1k`~`16k`와 별도 `7+1` |
| Native | 게임 내 기본 렌더러. 제작 파일에서는 `lobby.renderer`와 `gameplay.renderer`를 각각 `native`로 지정 |
| LR2 플레이 스킨 | 기존 LR2 소스로 가져오기. 지원되는 노트·LN·키·기어 표시이며 LR2 전체 메뉴/모든 명령 호환을 뜻하지 않음 |
| `.trskin` / `.trprofile` | 게임의 기존 휴대용 스킨 / 전체 프로필 컨테이너. 웹·오프라인 스킨 에디터는 이 바이너리 형식을 생성하지 않음 |
| 에디터 ZIP | `skin.json`과 이미지 폴더를 묶은 전달물. 압축을 풀고 폴더를 가져옴 |
| osu! `skin.ini` 등 다른 게임 형식 | 직접 로더 없음. 허가받은 이미지를 TenRiff 슬롯에 매핑해 v1 매니페스트를 새로 작성 |

내장 12개 테마는 공통 메뉴·낮은 건반·불투명 노트와 테마별 강조색을 사용한다.
[`modern-skins-20261109.ko.md`](modern-skins-20261109.ko.md)에 공통 규칙과 원본 보존 방식을 정리했다.

## AI에게 줄 최소 요청

아래 문장을 복사한 뒤 대괄호 부분만 바꿔도 된다.

```text
TenRiff용 [스킨 이름] 스킨을 만들어 줘.
분위기: [예: 어두운 프리즘 / 따뜻한 아케이드]
주요 색: [예: 시안, 보라, 흰색]
지원 키 모드: [예: 4K, 7K, 10K, 16K 또는 1K~16K]
노트 형태: [rect / circle / diamond / hex]
대상: 함께 받은 Modern Common 로컬 클라이언트와 로컬 JSON 스키마
기존 examples/skins/TenRiff-Example을 새 폴더로 복사해 시작해 줘.
배경은 중앙 플레이필드 가독성을 해치지 않게 어둡게 유지해 줘.
skins/AGENTS.md와 docs/skin-format.md를 따르고,
skin.json 스키마 검증과 F5 리로드 확인까지 해 줘.
```

## 제작 순서

1. **브리프 작성** — 이름, 분위기, 팔레트, 지원 키 모드, Native 대체 사용 여부를 정한다.
2. **폴더 생성** — 완성 스킨은 `skins/<name>/`에 `skin.json`, `README.md`를 둔다. 이미지가 있을 때만 `lobby/`, `gameplay/` 등의 자산 폴더를 추가한다. 원본과 다른 폴더에서 작업한다.
3. **매니페스트 우선** — 이미지보다 먼저 `skin.json`을 작성해 실제 필요한 슬롯을 확정한다.
4. **자산 제작** — 로비·게임플레이 배경과 필요한 스프라이트를 만든다. 생성형 이미지는 결과를 직접 확인한 뒤 스킨 폴더로 복사한다.
5. **모드 분리** — 레인별 배열이 달라지는 부분은 `gameplay.modes.4k` 등에 작성한다. 일반 필드와 배열은 통째로 교체하고 `native`의 각 범주만 슬롯별로 병합한다. BMS 7키+스크래치는 일반 8K와 구분되는 `7+1`을 사용한다.
6. **스키마 검증** — 오타, 잘못된 타입, 범위 초과, `1K` 같은 잘못된 모드 키를 잡는다.
7. **실시간 확인** — `Options > Skins`에서 가져오거나 선택하고 `F5`를 눌러 4K·7K·10K·16K를 확인한다.
8. **출처 기록** — 직접 제작, AI 생성, 사용자 제공, 외부 라이선스 자산을 README에 구분해 적는다.

## 무엇을 어디서 바꾸나

| 목표 | `skin.json` 위치 | 권장 방식 |
|---|---|---|
| 전체 메뉴 분위기 | `theme` | accent, panel, button, text 색을 한 팔레트로 구성 |
| 현대 기본 메뉴 세부 편집 | `lobby.renderer: native`, `native` | 동봉 스킨 에디터와 native-catalog.json의 슬롯 사용 |
| 공통 Native 인게임 세부 편집 | `gameplay.renderer: native`, `gameplay.native` | 동봉 에디터에서 도형 레이어·건반 움직임·시각 수치·색을 편집 |
| 로비 기본 배경 | `lobby.background` | 16:9 이미지와 `background_opacity` 사용 |
| 특정 화면 배경 | `lobby.screen_backgrounds` | 화면 ID별 이미지 지정 |
| 움직이는 배경 | `lobby.background_video`, `lobby.screen_videos`, `gameplay.background_video` | 무음 반복 영상. 정지 배경을 대체 수단으로 함께 두고, 자동 탐색은 없음 |
| 메뉴 위치 | `layout` | 1920×1080 기준 사각형만 필요한 슬롯에 지정 |
| 프로필 이미지 크기 | `layout.song_select.avatar`, `layout.result.avatar` | 프로필 패널 안의 독립 사각형으로 지정 |
| 인게임 배경 | `gameplay.background` | 중앙은 어둡고 가장자리에만 장식 배치 |
| Native 노트 스타일 | `gameplay.note_shape`, `lane_colors` | 이미지 없이도 완성 가능한 가장 안전한 시작점 |
| 이미지 노트·LN | `note`, `hold_head/body/tail` | 투명 PNG, 종횡비와 경계 확인 |
| 키 수별 차이 | `gameplay.modes` | `1k`~`16k` 또는 `7+1` 키로 필요한 필드만 덮어쓰기 |
| 판정선 기본 색상 | `gameplay.native.colors.judgement_line`, `judgement_glow` | Native Type 1 색상 슬롯. Type 2 색상 선택은 사용자 프로필 설정 |
| 판정선 기본 명도 | `gameplay.judgement_line_brightness` | `0..1`, 기본 1. 사용자 명도와 곱하며 알파는 유지 |

## 공통 스타일에서 자주 틀리는 규칙

- `note`, `hold_head`, `hold_tail` 벡터의 **배열 인덱스 0·1은 외곽선 전용**이다.
  `note_border: false`이면 두 레이어가 사라진다. 불투명한 본체를 **인덱스 2 이후**에 둔다.
  두 레이어만으로 만든 노트는 테두리를 끄면 통째로 보이지 않을 수 있다.
- 벡터 도형은 `note_shape: rect`로 시작한다. 다른 도형의 기본 단색 LN 규칙과
  명시한 벡터/이미지의 우선순위를 확인한다. 게임 스킨 미리보기에서 노트·LN 모두 확인한다.
- 생략한 이미지 슬롯은 표준 파일명을 자동 탐색한다. 예전 그림이 있는 폴더를 재사용하면서
  Native 표시를 원하면 `note: ""`, `gear: ""` 등으로 자동 탐색을 명시적으로 끈다.
  `lobby.screen_backgrounds`의 빈 문자열은 화면별 표준 파일 탐색을 막지 않으므로,
  오래된 화면 배경이 남아 있으면 원하는 새 배경 경로를 화면별로 지정한다.
- `judgement_line_brightness: 0.8` × 프로필 50%는 최종 40%다. 모드 값은 공통 스킨
  명도를 교체한 뒤 프로필과 곱한다. 0은 투명화가 아니라 검정색이다.
- `hud_layout`, `gauge_width`, `hud_riff_map_visible`, 글자 크기 배율,
  `key_overlay_enabled/scale`은 **프로필 설정**이다. `skin.json`에 임의 필드를 만들지 않는다.
  통합 키 오버레이는 4~16키, 25~100% 크기를 지원하며 기본값은 꺼짐이다.

## AI가 지켜야 할 안전선

- 경로는 `skin.json` 기준 상대 경로만 사용한다. 절대 경로와 `..`는 금지한다.
- 다른 게임이나 배포 스킨의 이미지를 허가 없이 복사하지 않는다.
- 글자가 포함된 생성 이미지는 철자를 확대 확인한다. 정확하지 않으면 글자 없는 자산으로 다시 만든다.
- 좁은 슬롯에서는 UI 글자가 자동 축소되지만, 실제 화면에서 최소 한 번 읽기 쉬운 크기인지 확인한다.
- 모든 슬롯을 채울 필요는 없다. 누락된 슬롯은 Native 렌더링으로 안전하게 대체된다.
- `native.rects`는 `[dx, dy, dwidth, dheight]` 증감량이고, `layout`은 절대 사각형이다. 두 형식을 혼동하지 않는다.
- `gameplay.native`는 인게임 전용이며 최상위 `native` 메뉴 항목과 별개다. `gameplay.renderer: native`를 함께 지정한다.
- 이미지 없는 최소 예제는 `TenRiff-Example`, 완성 공통 스타일은 [`TenRiff_NativeEditable`](../skins/TenRiff_NativeEditable/skin.json)을 복사한다. 후자는 `modern/background.png`도 함께 복사한다. 도형 좌표는 건반 128×256, 노트 128×32 기준이다.
- 스프라이트 배열을 생략하면 기본 도형을 쓴다. 빈 배열 `[]`은 해당 스프라이트를 숨기는 명시적 설정이다.
- 인게임 스킨이 바꾸는 것은 표시와 애니메이션뿐이다. 판정 시간, 입력 처리, 음원, 채보 규칙은 스킨 파일에서 바꾸지 않는다.
- 범용 스킨은 한 모드의 레인 배열을 다른 키 수에 억지로 재사용하지 않는다.
- 스킨만 고칠 때는 엔진 코드를 건드리지 않는다. 스키마에 없는 기능이 정말 필요할 때만 엔진 변경을 별도 작업으로 제안한다.

## 검증 명령

저장소 루트의 **PowerShell 7**에서 실행한다. 운영체제 기본 Windows PowerShell 5에는
`Test-Json`이 없으므로 `pwsh`를 사용하거나 동봉 에디터의 검증 결과를 확인한다.

```powershell
$skin = "skins/<skin-name>/skin.json"
Get-Content -Raw $skin | Test-Json -SchemaFile docs/tenriff-skin.schema.json
```

`True`가 나온 뒤 게임에서 선택하고 `F5`를 누른다. 경고가 없고 로비·미리보기·실제
게임플레이가 모두 읽히면 완료다. 스킨 파일만 바꾼 경우 재빌드는 필요 없다.

저장소 개발 환경에서는 `node --test tools/skin_editor/test_core.cjs`로 오프라인
에디터의 검증·병합·저장 규칙도 확인할 수 있다. 캔버스 범위 합계나 참조 파일 존재까지
JSON Schema가 전부 검사하지는 않으므로 스키마 통과만으로 실행 검증을 대체하지 않는다.

## 완료 조건

- `skin.json` 스키마 검증 성공
- README에 지원 모드와 자산 출처 기록
- 참조된 이미지가 모두 스킨 폴더 안에 존재
- 4K·7K·10K·16K 시각 확인(범용 스킨일 때)
- 메뉴 텍스트, 노트, LN, 판정선의 대비 확보
- 테두리 ON/OFF에서도 노트 본체 유지, 판정선 명도 0/50/100% 확인
- 에디터 저장 후 다시 열었을 때 모드별 값·이미지 참조 보존
- `F5` 리로드 뒤 경고 없음
- 실제 플레이를 확인하지 못했다면 자동 검증·합성 미리보기와 구분해 보고
