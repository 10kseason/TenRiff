# TenRiff Developer Extension Guide
Language: Korean | [English](developer-extension-guide.en.md) | [简体中文](developer-extension-guide.zh-CN.md) | [日本語](developer-extension-guide.ja.md)

이 문서는 새 `mode/mod`를 추가하거나 기존 모드 체인을 확장할 때 어디를 바꿔야 하는지 정리한 개발자용 안내서입니다. 사용자 문서가 아니라 유지보수 문서이므로, 코드 경계와 테스트 경계를 먼저 보고 작업하는 기준으로 읽으면 됩니다.

## Responsibility Map

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - `ChartFormatMode`, `KeyMode`, `GaugeMode`, `RandomMode` 같은 기본 열거형과 문자열 파서/출력 함수가 있습니다.
- `src/app/ModeResolver.h` / `src/app/ModeResolver.cpp`
  - `config::ModeConfig`를 런타임용 `gameplay::ModeSettings`로 바꾸고, 잘못된 토큰 경고를 모읍니다.
- `src/app/ModeManager.h` / `src/app/ModeManager.cpp`
  - 모드 모디파이어 레지스트리, 카테고리, 점수 배율, 판정창 스케일, 차트 변형을 담당합니다.
- `src/gameplay/ModeApplier.h` / `src/gameplay/ModeApplier.cpp`
  - 키 모드 변환과 랜덤 계열 변형을 실제 `GameplayChart`에 적용합니다.
- `src/app/MenuAppSettings.cpp`, `src/app/MenuAppTail.inl`, `src/app/MenuAppSettingsUtils.h`
  - `Mode Settings`, `Mod Manager`, `Key Mode` 같은 메뉴 UI와 입력 처리, 도움말 문구를 담당합니다.
- `src/config/Config.h` / `src/config/Config.cpp`
  - `config/config.json`과 프로필 설정의 로드/저장 스키마를 정의합니다.
- `src/app/RuntimeConfigMigration.cpp`
  - 오래된 기본값이나 레거시 토큰을 새 구조로 옮깁니다.
- `src/app/PersistedRuntimeConfig.cpp`
  - 세션 전용 모드를 저장용 설정에서 제거합니다.
- `src/gameplay/Replay.cpp`, `src/gameplay/Replay.h`, `src/app/MenuRecordUtils.cpp`, `src/app/GameSession.cpp`, `src/app/MenuAppTail.inl`
  - 리플레이/리절트 저장과 로드, 결과 화면 표시를 담당합니다.

## Add A New Mode

새 모드를 추가할 때는 보통 아래 순서로 작업합니다.

1. 먼저 `src/gameplay/ModeSettings.h`에 열거형이나 토큰 정의를 추가합니다.
2. `src/gameplay/ModeSettings.cpp`에서 `to_string(...)`과 `parse_...(...)`를 같이 갱신합니다.
3. 새 토큰이 설정 파일에 들어올 수 있으면 `src/app/ModeResolver.cpp`에서 해석과 경고를 추가합니다.
4. 모드가 차트 구조를 바꾸면 `src/app/ModeManager.cpp` 또는 `src/gameplay/ModeApplier.cpp`에서 실제 변형 로직을 넣습니다.
5. 메뉴에서 조작할 수 있어야 하면 `src/app/MenuAppSettings.cpp`의 행과 입력 처리를 추가합니다.
6. 저장/복구가 필요하면 `src/config/Config.cpp`, `src/app/RuntimeConfigMigration.cpp`, `src/app/PersistedRuntimeConfig.cpp`를 함께 확인합니다.
7. `tests/unit`과 `tests/smoke`에 회귀 테스트를 넣고, 필요하면 문서를 같이 맞춥니다.

## Add A New Key Mode

키 모드는 단순 토큰 추가가 아니라, 입력/표시/리플레이가 같이 바뀌는 항목입니다.

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - `KeyMode`에 새 값을 넣고 `parse_key_mode(...)`, `to_string(...)`를 맞춥니다.
- `src/gameplay/ModeApplier.cpp`
  - `target_lane_count(...)`와 차트 변형이 새 lane 수를 처리해야 합니다.
  - 키 모드 변환이 타이밍/홀드 메타데이터를 보존하는지 확인합니다.
- `src/app/ModeManager.cpp`
  - `target_lane_count(...)`, `key_mode_for_lane_count(...)` 같은 추론 규칙을 갱신합니다.
- `src/app/MenuAppSettingsUtils.h`
  - `normalize_runtime_key_mode(...)`, `cycle_runtime_key_mode(...)`, 라벨 헬퍼를 확인합니다.
- `src/app/MenuAppSettings.cpp`
  - `Key Mode` 행의 표시 문자열과 입력 순환을 맞춥니다.
- `src/app/MenuApp.cpp`
  - 키맵 편집/현재 차트 lane 수/런타임 lane binding 경로가 새 모드를 따라가는지 확인합니다.
- `src/app/GameSession.cpp`
  - 실제 플레이에서 선택된 lane count가 리플레이/결과 메타데이터와 일치해야 합니다.
- `src/app/PersistedRuntimeConfig.cpp`
  - 세션 전용이 아닌 한 저장에서 제거되지 않는지 확인합니다.

실수하기 쉬운 점은 `none`, `auto`, 대소문자, 레거시 alias를 한쪽만 바꾸는 것입니다. 설정 파일 토큰, UI 라벨, 런타임 enum 값이 모두 같은 의미를 가리켜야 합니다.

## Add A New Gauge Mode

게이지는 `ModeSettings`만 바꾸면 끝나지 않고, UI 문구와 마이그레이션까지 연결됩니다.

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - `GaugeMode`를 확장하고 문자열 변환을 맞춥니다.
- `src/app/MenuApp.cpp`
  - `gauge_type_from_mode_string(...)`, `gauge_type_label(...)` 같은 표시 경로를 갱신합니다.
- `src/app/MenuAppSettings.cpp`
  - `Gauge` 행의 순환 순서를 새 모드에 맞춥니다.
- `src/app/ModeManager.cpp`
  - `scale_judge_windows(...)`가 새 게이지 정책과 충돌하지 않는지 확인합니다.
- `src/app/RuntimeConfigMigration.cpp`
  - 기본값을 바꿨다면 정확한 레거시 값만 새 값으로 올리도록 마이그레이션을 추가합니다.

게이지는 결과 화면과 리플레이 메타데이터에도 보이므로, 저장 형식이나 라벨을 바꿨다면 `tests/unit/test_replay_export.cpp`와 결과 화면 경로도 같이 확인해야 합니다.

## Add A New Random Mode

랜덤 계열은 `ModeSettings`의 한 필드이지만, 실제 동작은 `ModeApplier`가 담당합니다.

- `src/gameplay/ModeSettings.h` / `src/gameplay/ModeSettings.cpp`
  - 새 랜덤 토큰과 파서를 추가합니다.
- `src/gameplay/ModeApplier.cpp`
  - `apply_full_random(...)`, `apply_super_random(...)` 같은 실제 변형 분기를 넣습니다.
  - 새 랜덤이 키 모드 변환보다 먼저 또는 나중에 적용되는지 순서를 명확히 해야 합니다.
- `src/app/ModeResolver.cpp`
  - 잘못된 토큰에 대한 경고와 기본값 처리를 추가합니다.
- `src/app/MenuAppSettings.cpp`
  - `Random` 행의 표시/순환을 갱신합니다.

랜덤은 같은 시드에서 항상 같은 결과가 나와야 하므로, 새 모드가 생기면 `tests/unit/test_mode_applier.cpp`에 결정성 테스트를 넣는 것이 좋습니다.

## Add A New Mod

모드는 보통 `ModeManager` 레지스트리에 들어갑니다. 신규 모드를 추가할 때 가장 중요한 것은 토큰, 카테고리, 배율, 변형, 저장 정책을 한 번에 맞추는 것입니다.

- `src/app/ModeManager.cpp`
  - `ModeModDescriptor` 레지스트리에 새 항목을 추가합니다.
  - `category_token`, `category_label`, `score_multiplier`를 함께 정합니다.
  - 필요하면 차트 구조 변형 함수를 추가하고 `manage_modes(...)`에서 호출합니다.
- `src/app/ModeManager.h`
  - 외부에 노출할 헬퍼가 있으면 선언을 추가합니다.
- `src/app/MenuAppSettings.cpp`
  - `populate_mode_mods_render_data(...)`는 레지스트리를 순회하므로 대부분 자동 반영되지만, 새 카테고리 설명이 필요하면 도움말 문구를 추가합니다.
- `src/app/PersistedRuntimeConfig.cpp`
  - 세션 전용 모드라면 저장용 설정에서 제거해야 합니다.
- `src/app/MenuAppTail.inl`
  - 결과 화면의 `Mods:` 표시와 현재 배율 문구가 새 모드를 잘 읽는지 확인합니다.

새 모드가 점수에 영향을 주면 `rate_score_multiplier(...)`, `mod_score_multiplier(...)`, `final_score_multiplier(...)`의 우선순위를 반드시 확인해야 합니다. 현재 구조는 최종 점수에서 `rate` 배율과 `mod` 배율 중 더 낮은 값을 사용합니다.

## Config, Migration, And Save Policy

설정이 추가되면 세 군데를 같이 봐야 합니다.

- `src/config/Config.cpp`
  - 로드: JSON에서 토큰을 읽고 기본값을 보존합니다.
  - 저장: 정규화된 토큰만 다시 써 넣습니다.
- `src/app/RuntimeConfigMigration.cpp`
  - 기존 사용자 설정이 새 기본값과 정확히 같을 때만 교체합니다.
  - 사용자 커스텀 값을 덮어쓰지 않도록 비교 조건을 좁게 유지합니다.
- `src/app/PersistedRuntimeConfig.cpp`
  - 세션에서만 의미 있는 모드를 저장 파일에서 제거합니다.

이 단계에서 자주 하는 실수는 UI에서만 토큰을 순환시키고, 저장/마이그레이션 쪽은 그대로 두는 것입니다. 그러면 다음 실행에서 설정이 되돌아갑니다.

## Replay, Result, And Records Impact

모드 변경은 결과 파일과 기록 화면에도 그대로 드러나야 합니다.

- `src/gameplay/Replay.cpp` / `src/gameplay/Replay.h`
  - 저장되는 replay/result JSON에 `mode`, `raw_score`, `final_score`, `rate_multiplier`, `score_multiplier`가 일관되게 들어가야 합니다.
- `src/app/GameSession.cpp`
  - 실제 플레이 종료 시 `ModeManager` 결과를 결과/리플레이 메타데이터에 반영합니다.
- `src/app/MenuRecordUtils.cpp`
  - 저장된 결과와 리플레이를 다시 읽어 `Records` 화면과 상세 패널에 보여줍니다.
- `src/app/MenuAppTail.inl`
  - 결과 화면 문구, 점수 배율, 모드 요약, replay/result 경로를 렌더링합니다.

리플레이나 결과 포맷이 바뀌면 `tests/unit/test_replay_export.cpp`를 꼭 갱신해야 합니다. 결과 파일은 다른 시스템에서 읽을 가능성이 높아서, 필드 이름과 기본값을 함부로 바꾸지 않는 편이 안전합니다.

## Tests And Docs Sync

신규 모드 작업은 테스트를 빼먹으면 나중에 UI나 저장 쪽에서 깨지기 쉽습니다.

- `tests/unit/test_mode_applier.cpp`
  - 키 모드 변환, 랜덤 결정성, hold 메타데이터 보존을 검증합니다.
- `tests/unit/test_mode_manager.cpp`
  - mod 레지스트리, 카테고리 충돌, 판정창 스케일, 점수 배율을 검증합니다.
- `tests/unit/test_config.cpp`
  - 저장/로드, 대소문자 정규화, 기본값 마이그레이션을 검증합니다.
- `tests/unit/test_replay_export.cpp`
  - replay/result JSON 필드와 복원 가능성을 검증합니다.
- `tests/smoke/bms_mode_smoke.cpp`
  - 실제 BMS 차트에서 mode 조합, lane 변형, 기대한 lane remap이 깨지지 않는지 확인합니다.

문서 동기화는 보통 `docs/current-state.md`, `docs/config.md`, `docs/README.md`, 그리고 필요하면 새 기능 문서 순서로 생각합니다. 이번 턴처럼 문서만 추가할 때는 새 개발자 가이드 파일들을 먼저 만들고, 이후 코드 변경이 생기면 현재 상태 문서를 따로 갱신하는 방식이 안전합니다.

공개 소스 패키지까지 함께 손대는 작업이라면 여기서 한 단계 더 가야 합니다. `opensource-Tenriff-source/TenRiff-<version>-source`를 다시 스테이징한 뒤에는, 그 폴더 안에 repo 전용 보조 파일(`tools/`, `10k-calc/`, 기존 `profiles/`)이 없다는 전제에서 raw `cmake` configure/build와 최소 `bms_parser_tests` 실행까지 확인해야 합니다.

## Common Mistakes

- 토큰을 `ModeSettings`에만 추가하고 `ModeResolver`와 `MenuApp` UI를 잊는 경우
- `ModeManager`의 레지스트리에는 넣었는데 `score_multiplier`나 카테고리 설명을 비워 두는 경우
- 세션 전용 모드를 `PersistedRuntimeConfig.cpp`에서 걸러내지 않는 경우
- 마이그레이션을 새 기본값 전체가 아니라 이전 shipped default와의 정확한 일치로 제한하지 않는 경우
- replay/result 필드를 바꿨는데 `MenuRecordUtils.cpp`와 `test_replay_export.cpp`를 안 고치는 경우
- `none` / `auto` / 대소문자 alias를 서로 다른 의미로 취급하는 경우
- 새 키 모드를 추가했는데 키맵, 결과, 리플레이 lane count가 함께 안 따라오는 경우

## Verification Checklist

- `ModeSettings` enum/parse/to_string round-trip이 된다.
- `ModeResolver`가 잘못된 토큰에 대해 경고를 내고 안전한 기본값으로 떨어진다.
- `ModeManager`가 새 mod를 올바른 카테고리와 배율로 등록한다.
- `ModeApplier`가 새 키 모드와 랜덤 규칙을 결정적으로 적용한다.
- `MenuApp`의 `Mode Settings`와 `Mod Manager` UI가 새 항목을 노출한다.
- `Config` 저장/로드와 `RuntimeConfigMigration`이 이전 사용자 설정을 깨지 않는다.
- `Replay` / `Result` JSON이 새 모드 정보를 포함한다.
- `tests/unit`과 `tests/smoke`가 모두 통과한다.
- 필요하면 `docs/current-state.md`와 `docs/config.md`를 별도 문서 패스로 갱신한다.
- 공개 소스 패키지를 갱신했다면 `opensource-Tenriff-source/TenRiff-<version>-source` 자체에서 standalone `cmake` configure/build와 `bms_parser_tests` 실행까지 확인한다.
