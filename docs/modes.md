# Mode System (Format/Key/Gauge/Random/Mods)

이 문서는 현재 구현된 모드 시스템, 레인 변형/랜덤 규칙(Mirror/FR/SR), 노트 구조 mod를 요약합니다.

## 설정 위치
- 전역: `config/config.json`의 `mode` 섹션
- 프로필: `profiles/<name>/config.json`의 `mode` 섹션

```json
"mode": {
  "format": "auto",
  "key_mode": "none",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "mods": [],
  "enable_osu_charts": false,
  "ghost_battle_enabled": false,
  "autoplay_enabled": false,
  "practice_no_fail_enabled": false,
  "one_miss_fail_enabled": false,
  "song_index_profile": "safe"
}
```

## 모드 의미
- `format`: `auto | bms | osu`
- `key_mode`: `none | auto | 4k | 5k | 6k | 7k | 8k | 9k | 10k | 16k`
- `gauge`: `normal | hard | ex_hard | easy`
- `random`: `off | mirror | fr | sr`
- `random_seed`: FR/SR, 강제 key-mode 변환, LN Mix 대상 선택의 고정 시드 (0도 고정 값으로 취급)
- `mods`: Mod Manager에서 정규화해 저장하는 mod token 배열
- `enable_osu_charts`: `false | true`
- `ghost_battle_enabled`: `false | true`
  - 기본값은 `false`
  - `true`: 선택한 차트의 최고 호환 replay를 자동으로 ghost 비교에 사용
  - `false`: ghost 비교 없이 일반 단일 필드 플레이
- `autoplay_enabled`: 판정 가능한 노트를 자동 처리하고 결과를 `ASSIST`로 표시
- `practice_no_fail_enabled`: 게이지 기반 조기 실패를 막고 차트 끝까지 진행
- `one_miss_fail_enabled`: 첫 osu!mania OD8 객체 `MISS`에서 즉시 실패하는 `Sudden Death (1 MISS)`
  - 네이티브 `BAD`만으로는 즉사하지 않으며 빈 키 입력의 `POOR`도 즉사 조건이 아님
  - Mode Settings에서 Practice No-Fail과 상호 배타적
- `song_index_profile`: `safe | fast`
  - `safe`: large-library RAM high-water를 우선 낮추는 기본값
  - `fast`: 32GB+ 환경에서 더 빠른 재인덱싱을 노리는 선택값

## 레인 변형 / 랜덤 규칙
- **Mirror**: key-mode 변환이 끝난 최종 레인을 고정 반전
  - 10K/16K는 두 플레이어 영역을 서로 바꾸지 않고 각 절반 안에서 독립적으로 반전
  - Mirror 자체는 `random_seed`를 사용하지 않지만, 먼저 실행되는 강제 key-mode 변환은 seed를 사용할 수 있음
- **FR(Full Random)**: 레인 전체를 랜덤 **퍼뮤테이션**으로 치환
- **SR(Super Random)**: 노트별 랜덤 배치
  - 동일 레인에 **겹침(동시 시각 포함)**이 없도록 후보 레인을 선택
  - **롱노트는 헤드/테일을 동일 레인에 유지**
  - 후보 레인이 없을 경우 원래 레인을 유지하며 경고를 기록

## 노트 구조 Mod
- **Full LN**: 변환 가능한 단노트를 다음 동일 레인 노트 직전까지의 일반 롱노트로 바꿈
- **LN Mix 10%~90%**: 기존 롱노트를 보존하고 같은 레인의 기존 span과 겹치는 head는 제외하며, 50ms 이상 길이와 다음 동일 레인 노트 전 50ms 여유를 확보할 수 있는 단노트 중 설정 비율을 `random_seed`로 결정적으로 선택
- **Full Tap**: 모든 롱노트 tail을 제거해 단노트로 바꿈
- 세 항목은 같은 `Note Structure` 카테고리라 하나만 활성화되며, 같은 seed와 차트에서는 LN Mix 결과가 재현됨

## 키모드 처리
- `none`은 차트 레인 수와 기본 패턴 레이아웃을 그대로 사용
- `auto`는 legacy alias로 남아 있으며 현재는 `none`과 같은 동작
- `4k..16k`는 N2NC 기반 lane remap으로 키 수를 맞춤
- key-mode 변환은 Mirror/FR/SR보다 먼저 적용

## 게이지 규칙
- 모든 게이지는 `100%`에서 시작하고 `0%`에 도달하면 즉시 실패합니다.
- `ex_hard`는 Hard보다 회복이 낮고 `BAD`/`POOR` 손실이 더 큰 도전용 게이지입니다.
- clear status는 `EX-HARD CLEAR`, `HARD CLEAR`, `CLEAR`, `EASY CLEAR` 순으로 구분됩니다.
- `Sudden Death (1 MISS)`는 게이지 종류가 아니라 첫 osu!mania OD8 객체 `MISS`에서 현재 게이지를 0으로 만들고 즉시 종료하는 별도 실패 규칙입니다.

## 구현 위치
- 모드 파싱: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- 모드 적용: `src/gameplay/ModeApplier.*`
- mod 등록/노트 구조 변환: `src/app/ModeManager.*`
