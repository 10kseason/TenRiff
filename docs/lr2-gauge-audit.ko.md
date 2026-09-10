# LR2 게이지 비교와 단위인정 규칙

확인일: 2026-09-10. 일반 Easy/Normal/Hard는 TenRiff 1.7.1 기본값을 유지했다. LR2와 다른 생존·회복 체계이므로 호환도를 백분율 하나로 표현할 수 없다.

| 항목 | TenRiff Easy | LR2 Easy | TenRiff Normal | LR2 Normal | TenRiff Hard | LR2 Hard |
|---|---:|---:|---:|---:|---:|---:|
| PG 회복 | 0.25 | 1.2T/N | 0.19 | T/N | 0.16 | 0.1 |
| GR 회복 | 0.20 | 1.2T/N | 0.15 | T/N | 0.09 | 0.1 |
| GD 회복 | 0.01 | 0.6T/N | 0.01 | 0.5T/N | 0.01 | 0.05 |
| BAD 손실 | 4.1 | 3.2 | 6.25 | 4 | 10 | 6D |
| 놓친 POOR 손실* | 1.6 | 4.8 | 2 | 6 | 2 | 10D |
| 빈 POOR 손실 | 1.6 | 1.6 | 2 | 2 | 2 | 2D |
| 시작 HP | 100 | 20 | 100 | 20 | 100 | 100 |

수치는 HP 퍼센트포인트다. T는 BMS TOTAL, N은 노트 수, D는 LR2의 TOTAL·노트 수 기반 손실 보정(최소 1)이다. TenRiff 회복은 TOTAL과 무관하다. *TenRiff 일반 플레이는 판정 HARD에서만 놓친 노트를 POOR로 처리하며, 그 외에는 BAD 손실을 사용한다. 게이지 Hard와 판정 HARD는 서로 다른 설정이다.

TenRiff는 각 게이지를 병렬 계산하고 0에서 다음 생존 게이지로 이동한다. LR2 Easy/Normal은 바닥 2, 종료 80 이상 클리어다. LR2 Hard는 HP 32 미만에서 모든 판정 손실을 0.6배로 줄인다. TenRiff Hard는 30 이하 POOR만 0.6배, Easy는 25 이하 BAD만 0.9배다. [원본 동작 보존 소스: 일반 수치](https://github.com/GOMazk/OpenLR2/blob/c7e4c9f92634619487c5a6a9d6999ccd7ccdad31/LR2f.cpp#L27629), [HP 처리](https://github.com/GOMazk/OpenLR2/blob/c7e4c9f92634619487c5a6a9d6999ccd7ccdad31/LR2f.cpp#L1954).

## 변경한 단위인정

- Auto Mix·초안·LR2 코스 모두 최초 100, 다음 곡에 실제 HP 그대로 이월한다.
- PG/GR +0.1, GD +0.04, BAD -2, 놓친 POOR -3, 빈 POOR -2.
- 간접미스를 항상 유지한다. 놓친 POOR는 노트 소비/콤보 단절, 빈 POOR는 비소비/콤보 유지다.
- 판정 직전 HP가 32 미만이면 손실 0.6배. 정확히 32는 보정하지 않는다. HP 2 미만에서 실패하며 회복·게이지 전환으로 부활하지 않는다.
- 일반 LN은 머리에서 회복하지 않고 완주/해제 시 머리 판정으로 한 번 적용한다. GOOD 허용범위보다 빠른 해제는 BAD 한 번, 재입력으로 환급하지 않는다.

근거는 [LR2 beta3 단위 수치](https://github.com/GOMazk/OpenLR2/blob/c7e4c9f92634619487c5a6a9d6999ccd7ccdad31/LR2f.cpp#L27706), [실패 조건](https://github.com/GOMazk/OpenLR2/blob/c7e4c9f92634619487c5a6a9d6999ccd7ccdad31/LR2f.cpp#L12915), [LN 처리](https://github.com/GOMazk/OpenLR2/blob/c7e4c9f92634619487c5a6a9d6999ccd7ccdad31/LR2f.cpp#L7538)다. OpenLR2의 원본 보존 브랜치는 역공학으로 재작성된 비교 근거이며, LR2 개발사의 공식 사양서는 아니다. beatoraja `CLASS_LR2`의 GD +0.05와 HP <30 근사치를 그대로 채택하지 않았다.

## 호환 범위와 검증

게이지 수치·경계·이월·일반 LN의 게이지 적용을 맞췄다. 판정창, 점수, LN 머리/꼬리 점수 계산은 TenRiff 규칙을 유지한다. LR2 beta3에 없는 `LNMODE 2` 차지 노트는 TenRiff의 머리/꼬리 0.5 가중치를 유지한다. Practice·Sudden Death 같은 명시적 모드는 자체 종료 규칙을 가진다. 실제 LR2 실행 파일과 입력별 A/B는 별도 수동 검증 항목이다.

새 코스 리플레이만 선택적 `mode.course_gauge="lr2_grade_v1"`, `mode.course_gauge_initial_value`를 기록한다. 재생/고스트에 동일 규칙을 복원하며 `ruleset_id="custom"`을 유지해 일반 ranked 증거로 승인하지 않는다. 기존 리플레이는 필드가 없으므로 재해석하지 않는다.

회귀 검사는 `test_gauge_manager.cpp`, `test_gameplay_engine.cpp`, `test_replay_export.cpp`의 `LR2 course*` 테스트다. 32 양쪽·2 양쪽, 회복, 두 POOR, LN 완주/놓침/해제/재입력, 곡간 이월, JSON 왕복/재생/잘못된 evidence를 검사한다.
