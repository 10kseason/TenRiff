# 스튜디오 덱 HUD 구현 사양

> 이 문서는 1.8.6 최초 구현 사양이다. 이후 로컬 보완에서는 사용자 요청으로 LR2 가져오기 스킨에도 단독 플레이 스튜디오 덱을 적용하고, 리프 맵 표시 옵션과 BGA 대비용 정적 받침을 추가했다. 해당 변경은 [스튜디오 덱 보완](studio-deck-followup.ko.md)을 기준으로 하며, 아래 1단계의 LR2 제외 규칙을 대체한다.

TenRiff 인게임 HUD를 스튜디오 덱 형태로 바꾸는 작업의 구현 사양이다. 디자인 시안은 "B · 스튜디오 덱"이며 원본 마크업은 `../handoff/studio-deck-mockup.dc.html`(이 저장소 바깥, `1.8.6-ingame-aesthetics/handoff/`)에 있다. 좌표와 색은 모두 이 문서에 다시 적었으므로 시안 파일은 참고용이다.

## 0. 시작점과 절대 바꾸면 안 되는 것

- 작업 위치: `1.8.6-ingame-aesthetics/source` (Git worktree). 현재 브랜치 `feature/ingame-aesthetics`(커밋 `14c0126`, 1.8.5 `9bc399f` 기반 인게임 다듬기)에서 `git switch -c feature/studio-deck-hud`로 새 브랜치를 만든다. 1.8.5 릴리스 폴더와 설치본, 다른 worktree는 건드리지 않는다.
- 빌드 폴더: `1.8.6-ingame-aesthetics/build` (이미 구성됨). 빌드 스크립트 `1.8.6-ingame-aesthetics/build.py`.
- **불변 조건** (위반 시 작업 실패로 본다):
  - 판정, 입력, 오디오, 리플레이(R1~R4 포맷과 검증기), 고스트 규칙, 게이지 계산, 점수 계산 코드를 바꾸지 않는다. HUD는 표시만 한다.
  - 노트 그리기 루프와 1.8.5 GPU 노트 배칭 경로(`can_batch_note_field`, `draw_note_sprite`, `flush_note_sprites`, 노트 배열 순서)를 바꾸지 않는다. 노트 루프 앞뒤에 새 그리기를 넣을 때도 배칭 중간에 끼우지 않는다.
  - 외부 이미지 스킨(LR2/TenRiff 이미지 스킨, `use_imported_metrics == true`)의 화면은 바꾸지 않는다.
  - 고스트 대전(`surface_layout.ghost_visible`)과 멀티플레이(`data.gameplay.peer_visible`) 화면은 1단계에서 기존 배치를 그대로 쓴다.
  - 프레임 경로에서 힙 할당, 파일 접근, 매 프레임 `CreateTextLayout`/`CreateGradientStopCollection`/레인 수만큼의 `FillRoundedRectangle`을 추가하지 않는다. (직전 작업에서 레인마다 둥근 사각형을 그려 16K p50이 0.07ms 늘었다가 일반 사각형으로 바꿔 해결했다.)

## 1. 화면 구성 (기준 좌표 1920×1080)

렌더러는 기준 좌표(`kBaseWidth`×`kBaseHeight`)로 그리고 해상도에 맞춰 축소·확대한다. 아래 숫자는 모두 기준 좌표이며 `field.left/right`는 `surface_layout.player_field`, `hit_y`는 판정선 y(`gameplay_field_y(... judgement_line_position)`)다. 8K 기본값에서 필드는 약 x 470~1450이다(시안은 472~1448로 근사했으므로 위치는 항상 필드 기준 상대값으로 계산한다).

| 요소 | 위치·크기 | 모양 |
|---|---|---|
| 배경 | 화면 전체 | BGA·스킨 배경이 없을 때 #050608 |
| 필드 | 기존 그대로 | 검정, 레인 구분선 흰색 5%, 필드 테두리선 없음(레일이 대신함) |
| 판정선 주변 빛 | `hit_y-112` ~ `hit_y` | 판정선 글로우 색 0 → 12% 세로 그라데이션 (현재 132px/17%보다 약하게) |
| 키 받침 | 기존 | #0A1119 |
| **레일 게이지** | 왼쪽 x `field.left-6`, 오른쪽 x `field.right+2`, 폭 4, y 0 ~ `hit_y` | 바탕 #12161E, 아래에서 위로 게이지 비율만큼 채움 |
| **게이지 값 표지** | x `field.right+14`, y = `hit_y - fill_h - 12`를 [8, `hit_y`-45]로 제한 | 둥근 표지(반경 6, 패딩 2/8) 바탕=게이지색, 글자 #0A0C10 16px "75%"; 아래 3px 띄워 게이지 이름(NORMAL 등) 11px 자간 2px, 게이지색 |
| **리프 맵** | x `field.left-80`(폭 56), y 60~1020 | 곡 전체 노트 밀도 막대 64개(피치 15, 막대 높이 10, 반경 1), 필드 쪽(오른쪽) 정렬, 길이 = 밀도×56 |
| 리프 맵 색 | | 앞으로 올 구간: 강조색 70%. 밀도 ≥0.8: #FF9F43. 지나간 구간: #262C36 |
| 재생 위치 | 리프 맵 y = 60 + 960×진행률 | 강조색 2px 선, x `map.x-8` ~ `map.x+64`; 선 위 20px, 띠 안쪽 오른쪽 정렬로 경과 시간 13px 강조색 |
| 시작/끝 시간 | x `map.x-58`, 폭 48, 오른쪽 정렬, y 56 / 1006 | "0:00" / 총 길이, 12px #4B5563 |
| 왼쪽 열 | x 84, y 52, 폭 = max(260, `map.x`-84-70) | 제목 24px 600 #F4F7FB, 아티스트 14px #7C889B, 18px 아래 설정 칸 |
| 설정 칸 | 3열 격자, 간격 6 | 칸: 패딩 7/9, 반경 8, 선 1px #1A202A, 바탕 #0A0D12. 이름 11px #6E7A8C, 값 16px #DDE3EB. RATE·HS·BPM / 스크롤·판정선·레이턴시 |
| 오른쪽 열 | x `field.right+100`, y 48, 폭 = max(240, 1856-x) | 아래 순서로 세로 배치, 간격 6 |
| SCORE | | 12px 자간 3px #6E7A8C |
| 점수 계수기 | | 40px, 앞자리 0은 #232A35, 유효 숫자 #F4F7FB. 예 `00,009,500` |
| PACE | 페이스메이커가 켜졌을 때만 | 14px, 양수 #66E6B0 / 음수 #FF6B78. 점수 모드 "PACE +240 pts", 정확도 모드 "PACE +0.12%p" (기존 표기 유지) |
| 정확도 | 14px 위 여백 | "99.98%" 26px 강조색 + "DETAIL 98.76" 14px #6E7A8C |
| 판정 비율 막대 | 18px 위 여백, 높이 8, 반경 4, 열 폭 전체 | 바탕 #12161E. PG/GR/G/BAD/PR 개수 비율만큼 판정색 구간 |
| 판정 개수 | | 한 줄(넘치면 줄바꿈): 판정색 이름 + 개수 14px #8A96A8 |
| 최대 콤보 | 10px 위 여백 | "MAX 125" 13px #6E7A8C |
| **판정 배지** | 필드 가운데, 기존 판정 위치(콤보 기준점) | 높이 46, 반경 23, 좌우 패딩 18, 테두리 1.5px 판정색, 바탕 판정색 12%. 판정 이름 30px 굵게 자간 3px; 구분선 1×22 흰색 22%; 오차 "−28ms"/"+52ms" 18px (빠름 #5DA9FF, 느림 #FF6B7A) |
| 타이밍 선 | 배지 10px 아래 | 200×2 #1E2530, 가운데 눈금 2×12 #7C889B, 점 8px 빠름/느림색, 위치 = clamp(오차/80ms)×100 |
| 콤보 | 기존 콤보 위치 | 숫자 70px 600 흰색 92%, 아래 "COMBO" 12px 자간 5px #6E7A8C |

P GREAT는 지금처럼 오차를 표시하지 않는다(배지에서 구분선·오차를 뺀다). `show_timing_feedback`이 꺼지면 배지의 오차 부분을, `show_timing_bar`가 꺼지면 타이밍 선을 숨긴다.

### 색은 기존 슬롯을 따른다

새 스킨 색 슬롯은 1단계에서 만들지 않는다. 이미 있는 Native 스킨 색을 그대로 쓴다.

- 게이지: `gauge_normal/hard/easy/ex_hard` (`gameplay_gauge_color` 기본값). EX-HARD 기본색 #292C31은 검은 배경의 레일에서 안 보이므로 레일·표지에서만 #C9CED6처럼 밝힌다.
- 판정: `judgement_pg/gr/gd/bd/pr`, 빠름/느림: `timing_fast/timing_slow`.
- 제목·본문·점수·콤보 글자: `title/body/score/combo`.
- 강조색: 기존 `accent_brush`.

### 글꼴은 시스템 글꼴로 옮긴다

시안의 IBM Plex는 Windows 기본 글꼴이 아니고 이 코드에는 글꼴 파일을 읽어 오는 경로가 없다. 1단계에서는 아래처럼 바꿔 쓴다.

- 숫자·영문 이름(점수, 콤보, 판정 이름, 설정 값, 시간): `Bahnschrift SemiBold`(콤보에 이미 사용 중). 숫자 폭이 일정하다.
- 한글(제목, 설정 이름): 기존 UI 글꼴(`ui_family`).
- 새 `IDWriteTextFormat`은 다른 글꼴 형식과 같은 곳(`MenuWindow.cpp`의 `preview_fonts` 생성부)에서 만들고, 게임 화면과 스킨 미리보기 양쪽(`d2d_->preview_fonts`)에 둔다.

## 2. 설정: HUD 스타일

`hit_burst_style`을 추가했던 경로를 그대로 따라 `skin.hud_layout`을 만든다. 같은 파일 목록:

- `src/config/Config.h`, `src/config/Config.cpp`: 필드, 토큰 정규화(`classic`/`studio`, 그 외 → 기본값), 읽기/쓰기.
- `config/config.json`, `docs/config.md`, `config.en.md`, `config.ja.md`, `config.zh-CN.md`: 키 설명.
- `src/app/menu/settings/SkinSettingsController.cpp`, `src/app/MenuAppSkin.cpp`: Options › Skins에 "HUD 스타일: 클래식 / 스튜디오 덱" 행.
- `src/app/MenuAppTail.inl`: `GameplayHudData`로 복사. `src/render/MenuWindow.h`의 `GameplayHudData`, `src/render/SkinGameplayPreview.h`(스킨 미리보기).
- `tools/menu_visual_preview.cpp`: `--hud-style classic|studio` 옵션.
- `tests/unit/test_config.cpp`: 기본값, 정규화, 저장·읽기 왕복.

기본값은 `studio`다. 스튜디오 덱은 다음 조건을 **모두** 만족할 때만 적용하고, 하나라도 아니면 기존 HUD를 그린다.

```text
hud_layout == "studio"
&& !use_imported_metrics          // Native 렌더러
&& !surface_layout.ghost_visible  // 고스트 대전 아님
&& !data.gameplay.peer_visible    // 멀티플레이 아님
```

Native 스킨이 `gameplay.native.rects`로 옮긴 기존 HUD 사각형(title, score, gauge 등)은 클래식에만 적용된다. 이 점을 `docs/skin-format.md`에 한 줄 적는다. 스킨 JSON 스키마와 웹/오프라인 스킨 에디터는 1단계에서 바꾸지 않는다.

## 3. 리프 맵 데이터

렌더러가 받는 `GameplayHudData::notes`는 화면에 보이는 최대 128개 노트뿐이라 곡 전체 밀도를 알 수 없다. 차트를 불러올 때 한 번 계산해 HUD 상태로 넘긴다.

1. 순수 함수 `build_riff_map(const gameplay::GameplayChart&, std::size_t bins)`를 `src/gameplay/`(예: `RiffMap.h/.cpp`)에 둔다.
   - 구간 = `duration_samples`를 `bins`(64)등분. 각 노트의 `start_sample`이 속한 구간에 1을 더한다(롱노트 끝, 지뢰는 세지 않는다).
   - 정규화: 가장 큰 구간의 값으로 나눠 0~255 정수로 저장한다. 노트가 없거나 길이가 0이면 빈 맵.
   - 배속(Rate)이 이미 반영된 차트 샘플을 쓰므로 Rate가 바뀌어도 그대로 맞는다.
2. `GameSession`이 차트를 준비한 뒤(엔진 생성 후) 한 번 계산해 멤버로 보관하고, 재시작·차트 변경 때 다시 계산한다. 리비전 번호를 1 올린다.
3. HUD 스냅샷(`GameSession.h`의 상태 구조체) → `MenuApp.h`의 `GameplayHudState` → `GameplayHudData`에 `std::array<uint8_t, 64> riff_map{}`, `std::size_t riff_map_count`, `uint64_t riff_map_revision`을 추가해 복사한다. 64바이트 고정 배열이라 8ms마다 복사해도 할당이 없다.
4. 진행률은 기존 진행 바와 같은 `current_sample / duration_samples`를 쓴다.

## 4. 렌더링 구조

### 정적 레이어 (`ensure_gameplay_static_cache`, `MenuWindow.cpp`)

한 번 기록해 매 프레임 `DrawImage`로 재생하는 커맨드 리스트다. 스튜디오 덱일 때 여기에 넣는다.

- 레일 바탕 2개, 필드 테두리선 생략.
- 리프 맵 막대 64개(모두 "앞으로 올 구간" 색과 고밀도 색으로).
- 시작/끝 시간 글자는 넣지 않는다(총 길이는 텍스트라 동적 레이어의 캐시된 레이아웃으로 그린다).

`GameplayStaticCache`(`MenuWindow.h`) 비교 키에 `hud_layout`, `riff_map_revision`, 강조색, 게이지 종류를 추가한다. 키가 같으면 다시 만들지 않는다.

### 동적 레이어 (`MenuWindow_draw_gameplay_body.inl`)

매 프레임 그리는 것은 다음으로 제한한다.

- 레일 채움 2개(`FillRectangle`), 게이지 표지 1개(둥근 사각형 1개 + 글자 2개).
- 리프 맵: 지나간 구간 위에 배경색 75% 사각형 1개, 재생 위치 선 1개, 경과 시간 글자 1개.
- 판정 배지, 타이밍 선, 콤보, 오른쪽 열 글자, 판정 비율 막대(구간 5개 + 바탕).
- 왼쪽 열 글자와 설정 칸 6개. 칸 테두리·바탕은 위치가 바뀌지 않으므로 정적 레이어로 옮겨도 된다(권장).

점수 계수기는 글자 두 번으로 그린다. 같은 오른쪽 정렬 사각형에 0으로 채운 전체 문자열(`00,009,500`)을 흐린 색으로, 그 위에 유효 부분(`9,500`)을 밝은 색으로 그리면 숫자 폭이 같아 정확히 겹친다. 0으로 채울 자릿수는 이 게임 점수의 이론상 최대 자릿수다. 점수 계산 코드에서 최대값을 확인해 정하고, 확인이 어려우면 7자리로 하되 점수가 넘치면 자릿수를 늘린다.

판정 배지는 기존 `draw_feedback_overlay`의 판정 애니메이션 변환(`judgement_text_animation`, 글자 크기 배율, `judgement_offset_x`)을 그대로 적용한다. 타이밍 글자·선 오프셋 설정(`timing_text_offset_*`, `timing_bar_offset_*`)도 배지의 오차 부분과 타이밍 선에 각각 적용한다. 콤보도 기존 `draw_combo_overlay`의 위치·배율·애니메이션 설정을 따른다.

문자열은 텍스트 리비전이 바뀔 때 `scene_hud_cache`에서 한 번 만들고(현재 코드 방식), 그릴 때는 `draw_readable_text_aligned`의 슬롯 캐시를 쓴다. UI 문자열 정리 함수가 연속 공백을 하나로 줄이므로 간격은 공백이 아니라 좌표로 만든다.

### 좁은 공간 처리

- 리프 맵: `map.x - 58 < 84 + 220`이면 시작/끝 시간을 숨긴다. `map.x < 84 + 220`이면 리프 맵을 숨긴다(필드를 왼쪽 끝으로 끈 경우).
- 오른쪽 열: 폭이 200 미만이면(필드를 오른쪽 끝으로 끈 경우) 기존 오른쪽 HUD로 돌아간다.
- 4K처럼 필드가 좁으면 열이 넓어질 뿐 배치 규칙은 같다. 16K·10K도 필드 폭은 기존 규칙(최대 980)이다.
- 성능 오버레이가 켜지면 오른쪽 열 오른쪽 끝을 `header_safe_right`로 줄인다.
- 7+1 숨김 스크래치(`hidden_scratch_mask`)는 기존처럼 레인만 숨기고 HUD 배치는 같다.

## 5. 손대지 않는 화면

로딩 카드, 시작 카운트다운 카드, 일시정지 메뉴, 재개 카운트다운, GAME OVER, 결과 화면은 1단계 범위가 아니다. 다만 시작 카운트다운 카드 뒤에 깔리는 정적 레이어와 헤더가 스튜디오 덱이면 그것도 스튜디오 덱 배치를 따른다.

## 6. 테스트

`tests/unit/`에 추가한다(`bms_parser_tests`에 포함됨, 단언은 `doctest::Approx(...).epsilon(1e-3)`처럼 허용 오차를 명시. 이 저장소의 `Approx` 기본 허용 오차는 절대값 1e-6이라 float 계산 결과와 자주 어긋난다).

- `build_riff_map`: 빈 차트, 길이 0, 한 구간에 몰린 노트, 균등 분포, 마지막 샘플이 마지막 구간에 들어가는지, 최대값 255 정규화.
- 배치 함수(순수 함수로 분리, 예: `GameplayMotion.h` 또는 새 `GameplayStudioDeck.h`): 리프 맵 x = `field.left-80`, 오른쪽 열 x = `field.right+100`, 왼쪽 열 폭 하한 260; 필드를 끌었을 때 숨김·복귀 조건; 4K/16K.
- 게이지 표지 y: 0%, 50%, 100%에서 [8, hit_y-45] 제한.
- 점수 계수기 문자열: 0, 9,500, 최대 자릿수 경계, 자릿수 초과.
- 판정 비율: 합계 0일 때 빈 막대, 비율 합이 100%.
- `hud_layout` 설정 기본값·정규화·왕복(`test_config.cpp`).

## 7. 검증 절차

모두 `1.8.6-ingame-aesthetics` 폴더에서 실행한다. 결과물은 `evidence-studio/`에 모은다.

1. 빌드: `python build.py tenriff menu_visual_preview bms_parser_tests tenriff_replay_verifier nk3_onnx_smoke gameplay_judgement_benchmark`
2. 테스트: `build`에서 `C:/msys64/mingw64/bin/ctest.exe -C Release --output-on-failure` 3/3, `build/Release/bms_parser_tests.exe`가 "All tests passed ... 0 integration skips".
3. GPU 배칭 게이트: `python -I sprite_regression.py build/Release/menu_visual_preview.exe evidence-studio/sprites`가 `all passed`. (스튜디오 덱이 기본값이면 이 게이트가 스튜디오 덱 화면으로 돈다. 클래식도 `--hud-style classic`을 넣은 사본으로 한 번 더 돌린다.)
4. 렌더 비용: `python -I perf_ab.py "../1.8.5-release/build/Release/menu_visual_preview.exe" build/Release/menu_visual_preview.exe evidence-studio/perf-ab 6`. 1.8.5 대비 p50 증가가 0.03ms 이하, p95/p99가 동등 이하여야 한다. 넘으면 원인을 찾아 줄인 뒤 다시 잰다. (인자 순서: 기준 exe, 후보 exe, 출력 폴더, 반복 횟수, 그 뒤는 공통 미리보기 옵션)
5. 화면: `capture.py`에 스튜디오 덱 케이스를 추가해 4K/7+1/8K/10K/16K, 페이스메이커 켬, 게이지 HARD·EASY·0%·100%, 필드를 왼쪽·오른쪽 끝으로 끈 상태, 960×540, 영어, 고스트(클래식으로 돌아가는지), 외부 스킨 LUNAR-FRACTURE(바뀌지 않는지)를 찍고 `evidence/baseline`과 나란히 비교한다. 미리보기 도구에는 리프 맵을 채울 고정 데이터(결정적 패턴)를 추가한다.
6. 기록: `evidence-studio/VERIFICATION.ko.md`에 결과와 확인하지 못한 것을 적고, 이 문서의 "남은 결정"을 갱신하고, 상위 폴더 `AGENTS.md` Completed 목록 맨 위에 한 항목을 추가한다.

커밋은 `feature/studio-deck-hud` 브랜치에만 한다. push, PR, 릴리스, 웹사이트 배포는 사용자가 요청할 때만 한다.

## 8. 2단계 후보 (이번 작업 아님)

- 고스트 대전·멀티플레이 화면의 스튜디오 덱 배치.
- 스킨 JSON의 `gameplay.native`에 스튜디오 덱 사각형·색 슬롯 추가, 스키마와 스킨 에디터 동기화.
- IBM Plex(OFL) 글꼴 동봉과 DirectWrite 사용자 글꼴 모음 로딩.
- 리프 맵에 BPM 변화·마디 표시.

## 확정한 결정 (2026-10-08)

- 기존 사용자 프로필에 `hud_layout`이 없으면 `studio`를 기본 적용한다. 명시적으로 저장한 `classic`은 유지하며 Options › Skins › HUD에서 선택할 수 있다.
- EX-HARD 기본색 `#292C31`은 레일·표지에서만 `#C9CED6`으로 밝힌다. Native 스킨이 지정한 다른 EX-HARD 색은 보존한다. 계산과 클래식 화면의 게이지 색은 바꾸지 않는다.

## 구현 보충

- `ResultStats.h`의 Native 표시 점수 최대값은 10,000이다. 따라서 점수는 5자리(`00,000`, `09,500`, `10,000`)로 채우고 범위를 넘는 입력도 자르지 않는다. 위 시안의 8자리 예시는 실제 점수 상한이 아니다.
- 오른쪽 열은 화면/성능 오버레이까지의 실제 가용 폭을 사용한다. `max(240, ...)`를 먼저 적용하면 200px 미만 복귀 조건에 도달할 수 없으므로, 200~239px 구간은 실제 폭으로 그리고 200px 미만은 기존 오른쪽 HUD로 돌아간다.
- 스튜디오 글자는 기존 슬롯별 텍스트 레이아웃 캐시를 사용한다. 작은 글자의 선명도와 렌더 비용을 위해 어두운 스튜디오 배경에서는 중복 윤곽선 패스 없이 그린다. 클래식의 글자 렌더링은 유지한다.
- 리프 맵 리비전은 새 `GameSession` 인스턴스 사이에도 증가한다. 서로 다른 곡에서 같은 리비전이 재사용되어 정적 맵이 남는 것을 막는다. 스킨 미리보기도 텍스트 관련 설정이 바뀔 때만 리비전을 갱신한다.
- 이번 빌드는 기존 `evidence/build.log`를 보존하기 위해 같은 명령의 `build_studio.py`를 사용하고 `evidence-studio` 아래에 시도별 로그를 남긴다.

### 캡처에서 확인한 배치 한계

- 필드를 왼쪽 끝으로 옮기면 리프 맵은 사양대로 숨지만, x=84로 고정한 제목·설정 칸은 필드와 겹친다. 1단계에서는 지정 좌표를 유지한다.
- 성능 오버레이가 켜지면 오른쪽 열은 기존 HUD로 복귀한다. 필드 오른쪽에 고정한 게이지 표지는 성능 오버레이에 가려질 수 있다.
- 눌린 키 이동은 실제 프레임 시간으로 보간되므로 고정 차트 시각만으로는 픽셀 동일 비교가 되지 않는다. 외부 스킨·고스트·멀티의 정확한 폴백 비교에는 idle 키를 사용했고, 원래 눌린 키 캡처도 보존했다.
