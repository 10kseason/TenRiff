# TenRiff 판정 범위

기준: **1.8.5 / 2026-10-05 / `tenriff-native-score-v2-ruleset-4`**. 이 문서는 현재 기본 설정의 native 판정 범위입니다. 결과 화면의 별도 osu!mania OD8 환산 판정과 구분합니다.

## BMS RANK별 기본 범위

`Judge Normal`(판정 MOD 없음) 기준입니다. BMS 파일의 `#RANK` 이름과 옵션의 `Judge Easy / Normal / Hard`는 서로 다른 설정입니다. 게이지의 Easy / Normal / Hard도 이 표를 선택하지 않습니다.

| BMS `#RANK` | 파일 판정 등급 | PG | GR | GD | BAD |
| --- | --- | ---: | ---: | ---: | ---: |
| `3` / 헤더 누락 | EASY | ±21ms | ±65ms | ±115ms | ±210ms |
| `2` | NORMAL | ±18ms | ±55.714286ms | ±98.571429ms | ±180ms |
| `1` | HARD | ±15ms | ±46.428571ms | ±82.142857ms | ±150ms |
| `0` | VERYHARD | ±8ms | ±24.761905ms | ±43.809524ms | ±80ms |

지원하지 않는 `#RANK` 값도 EASY로 처리합니다. EASY는 이전 기본값에서 PG만 20ms → 21ms로 변경했으며, GR/GD/BAD는 65/115/210ms를 유지합니다. 다른 RANK는 EASY의 네 판정 경계에 각각 `18/21`, `15/21`, `8/21`을 곱합니다. 소수는 표시용으로 여섯 자리까지 반올림했으며, 계산에는 원래 비율을 사용합니다.

### ±와 경계 읽는 법

`Δ = 입력 시각 − 노트 시각`입니다. 음수는 FAST, 양수는 SLOW이며, 표의 숫자는 **앞뒤 각각의 허용 시간**입니다. 따라서 ±21ms의 전체 폭은 42ms입니다.

| 판정 | 조건 (`abs(Δ)`는 시간 차의 절댓값) |
| --- | --- |
| PG | `abs(Δ) ≤ PG 경계` |
| GR | `PG 경계 < abs(Δ) ≤ GR 경계` |
| GD | `GR 경계 < abs(Δ) ≤ GD 경계` |
| BAD | `GD 경계 < abs(Δ) ≤ BAD 경계` |

경계값 자체는 더 좋은 판정에 포함됩니다. 예를 들어 EASY에서 정확히 +21ms는 PG, PG 경계 다음 샘플부터 GR입니다. 실제 엔진은 각 경계를 `llround(ms × sample_rate / 1000)`으로 가장 가까운 정수 샘플에 변환해 비교하므로, 표시한 ms와 최대 반 샘플 차이가 날 수 있습니다.

## Judge 모드 적용 순서

1. 프로필의 기본 `judge.pg/gr/gd/bd`를 읽습니다. 정식 기본값은 `21/65/115/210ms`입니다.
2. BMS `#RANK` 비율을 적용합니다.
3. `Judge Easy / Normal / Hard` 정책을 적용합니다.
4. 최종 경계를 오디오 샘플 단위로 변환합니다.

| Judge 모드 | PG / GR / GD | BAD | 별도 설정 |
| --- | --- | --- | --- |
| Normal | RANK별 기본 표 그대로 | RANK별 기본 표 그대로 | 기본값 유지 |
| Easy | RANK 적용 후 세 경계를 `1.35배` | 모든 RANK에서 **210ms 고정** | `hold_grace`·`hold_break`도 1.35배; `mask` 유지 |
| Hard | PG는 `17.5/21배`, GR/GD는 `18/21배` | 모든 RANK에서 **225ms 고정** | 홀드 설정 유지; 자동 미스를 간접 POOR로 기록 |

Hard의 EASY 기준 PG는 17.5ms이며 GR/GD는 일반 판정의 BMS NORMAL과 같은 55.714286/98.571429ms입니다. 이 세 경계는 다른 RANK에서 기존 비율로 줄어듭니다. Easy/Hard의 BAD는 RANK와 기존 BAD 값에 곱하거나 상한을 씌우지 않고 각각 210/225ms로 설정합니다. 임의 커스텀 PG/GR/GD에도 모드 비율을 적용하되 커스텀 기록은 계속 비공식입니다.

기본 설정에서 모드 적용 후의 수치는 다음과 같습니다. 모두 **±ms**입니다.

| BMS RANK | Judge Easy PG | GR | GD | BAD |
| --- | ---: | ---: | ---: | ---: |
| EASY | 28.35 | 87.75 | 155.25 | 210 |
| NORMAL | 24.3 | 75.214286 | 133.071429 | 210 |
| HARD | 20.25 | 62.678571 | 110.892857 | 210 |
| VERYHARD | 10.8 | 33.428571 | 59.142857 | 210 |

| BMS RANK | Judge Hard PG | GR | GD | BAD |
| --- | ---: | ---: | ---: | ---: |
| EASY | 17.5 | 55.714286 | 98.571429 | 225 |
| NORMAL | 15 | 47.755102 | 84.489796 | 225 |
| HARD | 12.5 | 39.795918 | 70.408163 | 225 |
| VERYHARD | 6.666667 | 21.22449 | 37.55102 | 225 |

Rate는 음악·차트 진행 속도를 바꾸지만 판정 창은 **실제 재생 시간 기준 ms**를 유지합니다. Hi-Speed와 스킨의 Visual Latency는 판정 창을 변경하지 않습니다.

## BAD, 무입력 미스, 공POOR

- 단노트·LN 머리를 직접 맞힐 수 있는 마지막 경계는 RANK와 Judge 모드가 적용된 BAD입니다.
- 입력 없이 노트를 놓친 경우 노트 시각에서 **+340ms를 초과**하면 자동 미스가 확정됩니다. 기본 설정의 모든 RANK와 Judge 모드에서 같습니다. Judge Normal/Easy는 BAD, Judge Hard는 간접 POOR로 기록하며 둘 다 콤보가 끊깁니다. 단위인정의 LR2 코스 게이지는 별도로 간접 POOR를 사용합니다.
- BAD 밖이지만 +340ms 이전인 늦은 입력도 BAD 적중이 아닙니다. 만료된 앞 노트를 미스 처리하고 같은 입력으로 다음 노트를 검사합니다. 앞 노트가 BAD이고 바로 다음 노트가 GD 이내인 조밀한 연타에서도 앞 노트를 미스 처리해 다음 노트를 판정합니다.
- 공POOR는 다음 노트보다 **BAD 경계 밖부터 1000ms 이내로 이른 입력**에서 발생합니다. 다음 노트를 소비하지 않고 콤보를 유지하며, 이미 지나간 노트 뒤에 별도의 뒷공POOR를 추가하지 않습니다.
- 입력 마스크 `mask`의 기본값은 **30ms**이며 RANK와 Judge 모드로 비례 축소하지 않습니다. 마스크가 활성화된 레인의 추가 누름은 해당 기간 동안 판정하지 않습니다.

사용자가 BAD를 340ms보다 크게 지정한 커스텀 설정에서는 자동 미스 경계가 `max(340ms, 최종 BAD)`입니다. 위의 340ms 설명은 정식 기본 설정 기준입니다.

## 롱노트 머리와 꼬리

LN 머리는 단노트와 같은 판정 창을 사용합니다. R3/R4에서는 일반 LN과 CN 모두 키를 떼는 순간 꼬리 시각과의 편차를 판정하고 홀드를 종료합니다. 다시 눌러도 이미 확정된 해제 판정을 취소하지 않습니다.

- 꼬리 해제도 RANK와 Judge 모드가 적용된 **PG/GR/GD** 경계를 사용합니다. GD 밖의 해제는 BAD이며, 머리의 BAD 경계보다 훨씬 일찍 놓아도 즉시 BAD·콤보 단절·게이지 감소가 발생합니다.
- 일반 LN은 끝까지 누르면 꼬리 시각에 자동 PG로 완료합니다.
- CN(`release_required`, 예: `#LNMODE 2`)은 꼬리를 떼어야 합니다. 떼지 않고 꼬리 뒤 `hold_break`를 초과하면 BAD입니다. 기본 200ms, Judge Easy에서는 270ms이며 RANK로 축소하지 않습니다.
- `No LN Release`는 CN도 끝까지 누르면 자동 완료하게 합니다. 중간에 놓아도 성공하는 모드는 아닙니다.
- `hold_grace` 기본 80ms는 설정 호환용으로 유지되지만 현재 native 꼬리 PG/GR 경계를 정하지 않습니다. `hold_break`의 하한으로 사용합니다. 꼬리를 별도 80/200ms 창으로 PG/GR 판정한다는 이전 문서 설명은 현재 구현과 다릅니다.

## 기존 리플레이와 기록

| 정식 ruleset | 기본 PG | BMS RANK | Judge Easy | Judge Hard BAD | 무입력 자동 미스 | LN 해제 |
| --- | ---: | --- | ---: | --- | --- | --- |
| R1 | 20ms | 미적용 | 1.25배 | 340ms | 해당 BAD 초과 | 이전 해제 동작 복원 |
| R2 | 20ms | 미적용 | 1.35배 | 최대 180ms | 340ms 초과 | 이전 해제 동작 복원 |
| R3 (1.8.3/1.8.4) | RANK별 21/18/15/8ms | 적용 | 1.35배 | 최대 180ms | 340ms 초과 | 즉시 해제 판정 |
| R4 (1.8.41) | RANK별 21/18/15/8ms | 적용 | PG/GR/GD 1.35배, BAD210ms 고정 | 225ms 고정 | 340ms 초과 | 즉시 해제 판정 |

R1/R2 기본 GR/GD/BAD는 65/115/210ms이며 해당 Judge 모드가 추가 적용됩니다. 이 표는 정식 규칙의 동작 비교입니다. 메타데이터가 오래된 리플레이는 별도로 `legacy-unverified`일 수 있습니다. R3/R4를 명시한 리플레이는 검증 메타데이터가 부족해도 각각 당시 판정으로 재생합니다. R3의 Easy BAD 1.35배·Hard PG/GR/GD 유지/BAD 최대180ms를 보존하며, R4의 Hard PG/GR/GD 비율을 과거 기록에 소급 적용하지 않습니다. 판정 규칙과 기록의 검증 상태를 구분해야 합니다.

커스텀 기본 판정에도 같은 RANK 비율을 적용하지만 정식 규칙 기록으로 인정하지 않습니다. 기존 기록을 R4로 소급 계산하지 않으며, 웹에서는 R3/R4의 `ruleset_id`와 `timing_profile`로 `bms-easy / bms-normal / bms-hard / bms-veryhard`를 구분합니다. 자세한 기록 계약은 [점수 무결성](score-integrity.md)과 [RANK·웹 호환 설명](audio-ln-rank.ko.md)을 참고하세요.

## 구현 근거와 갱신 위치

- [Config.h](../src/config/Config.h): 기본 판정 설정.
- [JudgeTimingPolicy.h](../src/app/JudgeTimingPolicy.h), [ModeManager.cpp](../src/app/ModeManager.cpp): RANK 비율과 Judge 모드 적용 순서.
- [BmsGameplayBuilder.cpp](../src/app/BmsGameplayBuilder.cpp): `#RANK` 읽기와 누락·미지원 값 처리.
- [GameplayEngine.cpp](../src/gameplay/GameplayEngine.cpp): 샘플 반올림, 경계 포함, 미스·공POOR·LN 해제.
- [ReplayVerifier.cpp](../src/app/ReplayVerifier.cpp): 기존 ruleset 호환과 커스텀 판정 구분.
- 관련 회귀: [mode manager](../tests/unit/test_mode_manager.cpp), [gameplay engine](../tests/unit/test_gameplay_engine.cpp), [replay export](../tests/unit/test_replay_export.cpp).

판정 정책을 바꾸면 이 문서의 표·경계 설명과 [설정 문서](config.md), [현재 상태](current-state.md), 관련 번역본을 함께 갱신합니다. `baseline-*`·`release-*`는 해당 버전의 기록으로 보존합니다.
