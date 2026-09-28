# TenRiff UI 현지화

Language: 한국어 | [English](localization.en.md) | [日本語](localization.ja.md) | [简体中文](localization.zh-CN.md)

## 언어 선택

- 클라이언트는 영어(`en`), 한국어(`ko`), 일본어(`ja`)를 지원합니다. 기본값은 `en`입니다.
- Options → Profile Setup → Language 또는 첫 실행 Quick Setup에서 좌우로 선택합니다. 순서는 영어 → 한국어 → 일본어이며 반대 방향으로도 순환합니다.
- 언어와 메뉴 글자 크기는 선택 즉시 현재 프로필에 저장하고 적용합니다.
- `jp`, `japanese`, `ja-jp`, `ja_JP`도 대소문자 구분 없이 `ja`로 정규화합니다. 알 수 없는 값은 기존처럼 `en`으로 돌아갑니다.

## 구현 경계

| 경로 | 역할 |
| --- | --- |
| `src/ui/Localization.h/.cpp` | `ui::Language`, 언어 토큰 정규화·순환, 공용 문자열 선택 |
| `src/ui/JapaneseStrings.inc` | 영어 문구를 키로 사용하는 컴파일 내장 일본어 카탈로그 |
| `src/config/Config.cpp` | 언어 설정 읽기·저장 |
| `src/app/MenuApp.cpp` | `ui_language()`와 `ui_text(영어, 한국어)` |
| `src/app/menu/settings/*SettingsView` | 언어 enum을 전달받는 설정 화면 |
| `src/app/MenuAppTail.inl` | `MenuRenderData.ui_language` 및 로딩 상태 전달 |
| `src/render/MenuWindow_draw*.inl` | `loc`, `wloc`, `result_loc`에서 공용 번역 호출 |

세 번째 언어를 추가하면서 bool 언어 전달을 enum으로 바꿨습니다. 기존 영어·한국어 리터럴은 유지하고 일본어 문구를 한 카탈로그에서 관리합니다. 사전은 한 번 생성되며 문자열 조회는 별도 할당 없이 수행합니다. 문구가 없으면 영어로 표시합니다.

UI 텍스트와 게임 상태는 구분합니다. BMS 편집 상태와 게임 로딩 단계는 내부 영어값을 유지하고 표시 스냅샷에서 알려진 문구만 번역합니다. 판정·오디오·노트 배치·스킨 형식은 번역 때문에 바뀌지 않습니다.

## 문구 추가

1. 앱에서는 `ui_text("English", "한국어")`, 렌더러에서는 `loc`/`wloc`/`result_loc`, 설정 view에서는 `localized(language, ...)`를 사용합니다.
2. 같은 영어 문구를 `JapaneseStrings.inc`에 한 번 추가합니다. 공백·줄바꿈·UTF-8 기호까지 원문과 맞춰야 합니다.
3. 저장되는 설정 토큰, 사용자 이름·채팅·곡명·아티스트·파일 경로는 번역하지 않습니다.
4. 새 동적 문구는 감사 스크립트에 명시적 소스 목록이나 검사 경로를 추가합니다. 부분 문자열 치환으로 사용자 데이터를 번역하지 않습니다.

## 검증

```powershell
python tools/audit_japanese_localization.py
python tools/audit_japanese_localization.py --json
```

검사는 앱·렌더러의 명시적 번역 호출, 화면 제목·내장 난이도표, 알려진 BMS 편집 상태·로딩 단계에서 일본어 누락, 중복 키, 빈 번역, 해석되지 않은 동적 키를 찾습니다. 하나라도 있으면 종료 코드가 1입니다. 검사 수는 출력의 실제 값을 사용합니다.

`test_localization.cpp`는 언어 정규화·양방향 순환·메뉴/설정/로딩/결과/편집 문구·영어/한국어 보존·미등록 값 보존을 확인합니다. 프로필 설정 테스트는 `en`/`ko`/`ja`와 메뉴 글자 크기의 저장 후 읽기, 첫 실행과 프로필 재설정에서의 양방향 전환을 확인합니다.

## 검증 범위와 제한

명시적 UI 문구 검사 통과가 화면 전체 번역이나 시각 QA 완료를 뜻하지는 않습니다. 서버·오디오·파서가 반환하는 진단, 과거 저장 결과의 자유형 설명, 외부 스킨 이미지 속 글자와 사용자 데이터는 영어 또는 원문으로 남을 수 있습니다. FAST/SLOW, BGA, ASIO, Rate, 키 이름 등 게임·기술 용어도 일부 유지합니다. OS 파일 선택창의 파일 형식 설명은 일부 영어입니다.

새 문구를 넣은 뒤에는 일본어로 타이틀·선곡·설정·도움말·결과·로딩을 확인하고, 720p/1080p에서 잘림·글꼴 대체·줄바꿈을 점검합니다. 자동 소스 검사와 C++ 단위 테스트는 실제 플레이나 원격 대전 검증을 대신하지 않습니다.
