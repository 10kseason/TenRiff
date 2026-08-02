# Mode System (Key/Gauge/Random/Mods)

이 문서는 현재 구현된 모드 시스템, 레인 변형/랜덤 규칙(Mirror/FR/SR), 노트 구조 mod를 요약합니다.

## 설정 위치
- 전역: `config/config.json`의 `mode` 섹션
- 프로필: `profiles/<name>/config.json`의 `mode` 섹션

```json
"mode": {
  "key_mode": "none",
  "key_conversion_algorithm": "krrcream",
  "key_conversion_note_add_mode": "default",
  "enable_osu_charts": false,
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe",
  "calculate_song_index_difficulty": false
}
```

## 모드 의미
- `enable_osu_charts`: 기본 `false`; Mode Settings의 `OSU Charts`로 켜면 osu!mania 4K~10K `.osu`를 인덱싱·플레이하고 라이브러리를 재스캔함
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 12k | 14k | 16k`
- `key_conversion_algorithm`: `krrcream | nk2` (기본 `krrcream`; 게임 내 Key Converter에서 선택하며 실제 키수 변환 때만 적용)
- `key_conversion_note_add_mode`: `default | add_25_plus` (기본 `default`; 실제 건반 수가 바뀔 때만 기본 변환 결과보다 최소 25% 노트 추가를 요청)
- `gauge`: `normal | hard | ex_hard | easy | shift`
- `random`: `off | mirror | rr | fr | sr`
- `random_seed`: RR/FR/SR, 강제 key-mode 변환, Note Add, LN Mix 대상 선택의 고정 시드 (0도 고정 값으로 취급)
- `mods`: Mod Manager에서 정규화해 저장하는 mod token 배열
- `ghost_battle_enabled`: `false | true`
  - 기본값은 `false`
  - `true`: 선택한 차트의 최고 호환 replay를 자동으로 ghost 비교에 사용
  - `false`: ghost 비교 없이 일반 단일 필드 플레이
- `autoplay_enabled`: 판정 가능한 노트를 자동 처리하고 결과를 `ASSIST`로 표시
- `practice_no_fail_enabled`: 게이지 기반 조기 실패를 막고 차트 끝까지 진행
- `one_miss_fail_enabled`: 첫 OD8 환산 객체 `MISS`에서 즉시 실패하는 `Sudden Death (1 MISS)`
  - 네이티브 `BAD`만으로는 즉사하지 않으며 빈 키 입력의 `POOR`도 즉사 조건이 아님
  - Mode Settings에서 Practice No-Fail과 상호 배타적
- `song_index_profile`: `safe | fast`
  - `safe`: large-library RAM high-water를 우선 낮추는 기본값
  - `fast`: 32GB+ 환경에서 더 빠른 재인덱싱을 노리는 선택값
- `calculate_song_index_difficulty`: `false | true`
  - 기본 `false`: BMS `#PLAYLEVEL`을 유지하고 자체 LV/CR 계산 생략
  - `true`: 전체 재인덱싱에서 Revive LV/Circus Rating 계산

`Rate`는 `mode`가 아니라 `speed.rate`에 저장됩니다. Mode Settings에서 조정할 수 있고, Song Select에서는 검색 입력 중이 아닐 때 `-` / `+`로 다음 플레이 값을 바로 바꿀 수 있습니다.

## 레인 변형 / 랜덤 규칙
- **DP Flip**: DP 좌우 플레이어 영역 전체를 맞바꾸며 스크래치와 건반의 내부 순서는 유지
- **Mirror**: key-mode 변환이 끝난 최종 레인을 고정 반전
  - DP 레이아웃은 두 플레이어 영역을 서로 바꾸지 않고 각 영역 안에서 독립적으로 반전
  - Mirror 자체는 `random_seed`를 사용하지 않지만, 먼저 실행되는 강제 key-mode 변환은 seed를 사용할 수 있음
- **RR(R-Random)**: 스크래치는 고정하고 각 플레이 가능 레인 그룹을 seed 기반 원형 회전; DP는 좌우 영역을 독립 처리
- **FR(Full Random)**: 레인 전체를 랜덤 **퍼뮤테이션**으로 치환
- **SR(Super Random)**: 노트별 랜덤 배치
  - 동일 레인에 **겹침(동시 시각 포함)**이 없도록 후보 레인을 선택
  - **롱노트는 헤드/테일을 동일 레인에 유지**
  - 후보 레인이 없을 경우 원래 레인을 유지하며 경고를 기록

## 노트 구조 Mod
- **Note Add 10%~100%**: 기존 노트 시각에 무음 화음 노트를 결정적으로 추가하며 스크래치·LN body·동일 레인 중복·과도한 화음을 피함. 이 결과는 기록 목록에는 남지만 일반 최고기록을 덮지 않음
- **Full LN**: 변환 가능한 단노트를 다음 동일 레인 노트 직전까지의 일반 롱노트로 바꿈
- **LN Mix 10%~90%**: 기존 롱노트를 보존하고 같은 레인의 기존 span과 겹치는 head를 제외한다. base BPM 기준 8비트 LN도 다음 동일 레인 노트보다 50ms 먼저 끝낼 수 있는 단노트 중 설정 비율을 `random_seed`로 선택하고, 선택된 LN 길이는 모든 Mix 단계에서 긴 8비트 60% / 중간 16비트 20% / 짧은 24·32비트 20%로 결정적으로 배분
- **Full Tap**: 모든 롱노트 tail을 제거해 단노트로 바꿈
- 세 항목은 같은 `Note Structure` 카테고리라 하나만 활성화되며, 같은 seed와 차트에서는 LN Mix 결과가 재현됨

## 키모드 처리
- `none`은 차트 레인 수와 기본 패턴 레이아웃을 그대로 사용
- `auto`는 legacy alias로 남아 있으며 현재는 `none`과 같은 동작
- `4k..10k`, `12k`, `14k`, `16k`는 N2NC 기반 lane remap으로 키 수를 맞춤
- `5+1 SP`와 `7+1 SP` 강제 변환은 스크래치를 제외한 건반부만 목표 키 수로 재배치하며, `follow` 스크래치 키사운드는 자동 재생 큐로 이동
- `10+2 DP`와 `14+2 DP` 강제 변환도 두 스크래치를 제외하고 좌우 건반부를 독립적으로 변환
- `add_25_plus`는 기본 키 변환 뒤 기존 시각에 안전한 무음 화음을 최소 25% 추가 요청하며, 더 높은 Note Add Mod가 있으면 그 비율만 한 번 적용. 해당 결과는 기록 목록에는 남지만 일반 최고기록을 덮지 않음
- 적용 순서: key-mode 변환 → DP Flip → Mirror/RR/FR/SR → Note Add → LN/Full Tap 구조 변환

## 게이지 규칙
- 고정 게이지(`ex_hard / hard / normal / easy`)는 `100%`에서 시작하고 `0%`에 도달하면 즉시 실패하며 타입이 바뀌지 않습니다.
- `shift`는 EX-Hard / Hard / Normal / Easy를 각각 100%에서 병렬 계산하고, 현재 tier가 탈락하면 같은 판정 이력을 누적한 다음 생존 tier를 선택하며 종료 시 가장 높은 생존 tier로 확정합니다.
- `ex_hard`는 Hard보다 회복이 낮고 `BAD`/`POOR` 손실이 더 큰 도전용 게이지입니다.
- clear status는 고정 게이지 결과와 `GAUGE SHIFT EX-HARD / HARD / NORMAL / EASY CLEAR`로 최종 생존 tier를 구분합니다.
- `Sudden Death (1 MISS)`는 게이지 종류가 아니라 첫 OD8 환산 객체 `MISS`에서 현재 게이지를 0으로 만들고 즉시 종료하는 별도 실패 규칙입니다.

## 구현 위치
- 모드 파싱: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- 모드 적용: `src/gameplay/ModeApplier.*`
- mod 등록/노트 구조 변환: `src/app/ModeManager.*`
