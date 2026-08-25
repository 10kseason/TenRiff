# AI용 TenRiff 스킨 제작 가이드

이 문서는 AI 코딩 에이전트에게 TenRiff 스킨 제작을 맡길 때 사용하는 작업 계약이다.
포맷의 전체 필드 설명은 [`skin-format.md`](skin-format.md), 자동 완성·검증 규칙은
[`tenriff-skin.schema.json`](tenriff-skin.schema.json)이 기준이다.

바로 실행되는 완성 예제는
[`TenRiff_AgentPrism_Universal_1K-16K`](../examples/skins/TenRiff_AgentPrism_Universal_1K-16K)을 참고한다.

## AI에게 줄 최소 요청

아래 문장을 복사한 뒤 대괄호 부분만 바꿔도 된다.

```text
TenRiff용 [스킨 이름] 스킨을 만들어 줘.
분위기: [예: 어두운 프리즘 / 따뜻한 아케이드]
주요 색: [예: 시안, 보라, 흰색]
지원 키 모드: [예: 4K, 7K, 10K, 16K 또는 1K~16K]
노트 형태: [rect / circle / diamond / hex]
배경은 중앙 플레이필드 가독성을 해치지 않게 어둡게 유지해 줘.
examples/skins/AGENTS.md와 docs/skin-format.md를 따르고,
skin.json 스키마 검증과 F5 리로드 확인까지 해 줘.
```

## 제작 순서

1. **브리프 작성** — 이름, 분위기, 팔레트, 지원 키 모드, Native 대체 사용 여부를 정한다.
2. **폴더 생성** — `examples/skins/<name>/` 아래에 `skin.json`, `README.md`, `lobby/`, `gameplay/`를 둔다.
3. **매니페스트 우선** — 이미지보다 먼저 `skin.json`을 작성해 실제 필요한 슬롯을 확정한다.
4. **자산 제작** — 로비·게임플레이 배경과 필요한 스프라이트를 만든다. 생성형 이미지는 결과를 직접 확인한 뒤 스킨 폴더로 복사한다.
5. **모드 분리** — 레인별 배열이 달라지는 부분은 `gameplay.modes.4k` 같은 얕은 오버라이드로 작성한다.
6. **스키마 검증** — 오타, 잘못된 타입, 범위 초과, `1K` 같은 잘못된 모드 키를 잡는다.
7. **실시간 확인** — `Options > Skins`에서 가져오거나 선택하고 `F5`를 눌러 4K·7K·10K·16K를 확인한다.
8. **출처 기록** — 직접 제작, AI 생성, 사용자 제공, 외부 라이선스 자산을 README에 구분해 적는다.

## 무엇을 어디서 바꾸나

| 목표 | `skin.json` 위치 | 권장 방식 |
|---|---|---|
| 전체 메뉴 분위기 | `theme` | accent, panel, button, text 색을 한 팔레트로 구성 |
| 로비 기본 배경 | `lobby.background` | 16:9 이미지와 `background_opacity` 사용 |
| 특정 화면 배경 | `lobby.screen_backgrounds` | 화면 ID별 이미지 지정 |
| 메뉴 위치 | `layout` | 1920×1080 기준 사각형만 필요한 슬롯에 지정 |
| 인게임 배경 | `gameplay.background` | 중앙은 어둡고 가장자리에만 장식 배치 |
| Native 노트 스타일 | `gameplay.note_shape`, `lane_colors` | 이미지 없이도 완성 가능한 가장 안전한 시작점 |
| 이미지 노트·LN | `note`, `hold_head/body/tail` | 투명 PNG, 종횡비와 경계 확인 |
| 키 수별 차이 | `gameplay.modes` | `1k`~`16k` 키로 필요한 필드만 덮어쓰기 |

## AI가 지켜야 할 안전선

- 경로는 `skin.json` 기준 상대 경로만 사용한다. 절대 경로와 `..`는 금지한다.
- 다른 게임이나 배포 스킨의 이미지를 허가 없이 복사하지 않는다.
- 글자가 포함된 생성 이미지는 철자를 확대 확인한다. 정확하지 않으면 글자 없는 자산으로 다시 만든다.
- 모든 슬롯을 채울 필요는 없다. 누락된 슬롯은 Native 렌더링으로 안전하게 대체된다.
- 범용 스킨은 한 모드의 레인 배열을 다른 키 수에 억지로 재사용하지 않는다.
- 스킨만 고칠 때는 엔진 코드를 건드리지 않는다. 스키마에 없는 기능이 정말 필요할 때만 엔진 변경을 별도 작업으로 제안한다.

## 검증 명령

저장소 루트의 PowerShell에서 실행한다.

```powershell
$skin = "examples/skins/<skin-name>/skin.json"
Get-Content -Raw $skin | Test-Json -SchemaFile docs/tenriff-skin.schema.json
```

`True`가 나온 뒤 게임에서 선택하고 `F5`를 누른다. 경고가 없고 로비·미리보기·실제
게임플레이가 모두 읽히면 완료다. 스킨 파일만 바꾼 경우 재빌드는 필요 없다.

## 완료 조건

- `skin.json` 스키마 검증 성공
- README에 지원 모드와 자산 출처 기록
- 참조된 이미지가 모두 스킨 폴더 안에 존재
- 4K·7K·10K·16K 시각 확인(범용 스킨일 때)
- 메뉴 텍스트, 노트, LN, 판정선의 대비 확보
- `F5` 리로드 뒤 경고 없음
