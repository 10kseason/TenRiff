# TenRiff Localization Guide (current)

Language: Korean | [English](localization.en.md) | [简体中文](localization.zh-CN.md)

이 문서는 TenRiff의 현재 UI 현지화 구조를 정리하고, 나중에 다른 언어를 추가할 때 어디를 어떻게 건드려야 하는지 빠르게 찾을 수 있게 만드는 참고 문서입니다.

## Current Model
- 현재 공식 UI 언어는 `en`, `ko` 두 개입니다.
- 설정 키는 `ui.language`이며 기본값은 `en`입니다.
- 잘못된 토큰은 로드 시 `en`으로 정규화됩니다.
- 언어 변경은 Graphics Settings에서 즉시 메뉴 UI에 반영되고, 저장 후 다음 실행에도 유지됩니다.
- 현재 현지화 범위는 메뉴/설정/도움말/곡 선택/결과/게임플레이 HUD 중심입니다.
- 곡 제목, 아티스트, 저장된 결과 파일 안의 일부 상태 텍스트처럼 데이터 자체인 값은 번역 대상이 아니라 표시 시 sanitize 대상입니다.

## Main Boundaries
1. 설정 저장/로드와 언어 토큰 정규화
   - `src/config/Config.h`
   - `src/config/Config.cpp`
2. 앱 레벨 문자열 선택 헬퍼
   - `src/app/MenuApp.h`
   - `src/app/MenuApp.cpp`
3. 렌더 스냅샷으로 언어 상태 전달
   - `src/app/MenuAppTail.inl`
   - `src/render/MenuWindow.h`
4. 앱 쪽 메뉴 문자열 생성
   - `src/app/MenuAppDeviceSettings.cpp`
   - `src/app/MenuAppSettings.cpp`
   - `src/app/MenuAppKeymap.cpp`
   - `src/app/MenuAppSkin.cpp`
   - `src/app/MenuAppSongSelectRender.cpp`
   - `src/app/MenuAppTail.inl`
5. 렌더러 쪽 하드코딩 UI 문자열
   - `src/render/MenuWindow_draw.inl`
   - `src/render/MenuWindow_draw_title_body.inl`
   - `src/render/MenuWindow_draw_generic_body.inl`
   - `src/render/MenuWindow_draw_songselect_body.inl`
   - `src/render/MenuWindow_draw_result_body.inl`
   - `src/render/MenuWindow_draw_gameplay_body.inl`

## Current Conventions
- 앱 로직에서 바로 문자열을 만들 때는 `ui_text("English", "한국어")`를 사용합니다.
- 반복되는 토큰형 값은 전용 라벨 헬퍼를 사용합니다.
  - 예: `ui_on_off`, `ui_language_label`, `ui_gauge_label`, `ui_random_label`
- 렌더러에서만 쓰는 고정 문자열은 `MenuWindow::draw(...)` 안의 `loc(...)`, `wloc(...)`를 따라갑니다.
- 유저 데이터나 차트 메타데이터는 번역하지 말고 `sanitize_ui_text(...)`로 안전하게만 표시합니다.
- 설정에 저장되는 값은 표시용 라벨과 분리합니다.
  - 예: 저장 값은 `hard`, 표시 값은 `Hard` 또는 `하드`

## When Adding Or Editing A UI String
1. 앱 상태/설정/도움말에서만 쓰는 문자열이면 `MenuApp*` 계열 파일에서 `ui_text(...)`로 추가합니다.
2. 렌더 함수 내부에서만 쓰는 고정 문자열이면 `MenuWindow_draw*.inl` 쪽 `loc(...)` 또는 `wloc(...)`에 맞춰 추가합니다.
3. `on/off`, gauge, random, display mode처럼 반복되는 값이면 새 헬퍼를 만들거나 기존 헬퍼를 확장합니다.
4. 설정 저장값 자체를 현지화하지 말고, 저장값을 표시 라벨로 바꾸는 함수만 추가합니다.
5. 곡 메타데이터, 파일명, replay/result 경로 같은 데이터 값은 번역하지 않습니다.

## Recommended Workflow For A New Language
- 현재 구조는 `영어/한국어` 2개를 전제로 한 pair 기반입니다. 세 번째 언어부터는 임시 덧붙이기보다 구조 변경을 같이 하는 편이 안전합니다.

1. `Config`에 새 언어 토큰 정규화 규칙을 추가합니다.
2. `ui.language` 기본값과 migration 동작을 유지한 채 새 토큰을 저장/로드할 수 있게 합니다.
3. `render.ui_korean` 같은 bool 전달을 언어 enum 또는 언어 토큰 전달로 바꾸는 것을 우선 검토합니다.
4. `ui_text("en", "ko")` pair helper를 다국어 lookup 형태로 올리는 전용 모듈을 추가합니다.
   - 권장 위치: `src/ui/Localization.h`, `src/ui/Localization.cpp`
5. 반복 라벨을 그 모듈의 토큰 기반 API로 옮깁니다.
6. `MenuApp*`와 `MenuWindow_draw*`의 하드코딩 문자열을 전수 치환합니다.
7. 문서와 테스트를 같이 갱신합니다.

## Recommended Future Token Layout
- 저장값은 계속 짧은 안정 토큰을 유지합니다.
  - 예: `en`, `ko`, `hard`, `normal`, `easy`
- 표시 문자열은 토큰 테이블에서 꺼냅니다.
- 한 토큰에 대해 언어별 문자열을 한곳에서 관리합니다.
- 화면 파일은 가능하면 직접 문구를 들고 있지 않고 토큰만 요청하게 정리합니다.

예시:

```cpp
enum class UiTextId {
    Back,
    Save,
    Language,
    GaugeHard,
};

std::string localized_text(Language lang, UiTextId id);
```

## Minimum Regression Checklist
- `config save and load preserve ui language setting`
- `config load normalizes invalid ui language to english`
- Graphics Settings의 Language row가 즉시 화면에 반영되는지 확인
- Help overlay, Song Select, Result, Gameplay loading/countdown에서 언어가 섞이지 않는지 확인
- Keymap 저장 성공/실패 메시지, Result 재시작/리플레이 힌트, Song Select 브라우저 힌트가 같이 바뀌는지 확인
- `docs/ui-audit-checklist.md` 기준으로 `720p`, `1080p`, `Performance HUD on/off` 수동 확인

## Known Limitations
- 현재 구조는 엄밀한 다국어 시스템이 아니라 `en/ko` 분기형입니다.
- 저장된 result/replay 메타데이터의 일부 상태 문자열은 영문으로 남을 수 있습니다.
- 일부 fallback placeholder 문자열은 아직 영문일 수 있습니다.
- 새 언어를 제대로 추가하려면 bool 기반 전달과 pair helper부터 일반화하는 작업이 먼저 필요합니다.
