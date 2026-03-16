# Mode System (Format/Key/Gauge/Random)

이 문서는 현재 구현된 모드 시스템과 랜덤 규칙(SR/FR)을 요약합니다.

## 설정 위치
- 전역: `config/config.json`의 `mode` 섹션
- 프로필: `profiles/<name>/config.json`의 `mode` 섹션

```json
"mode": {
  "format": "auto",
  "key_mode": "auto",
  "gauge": "normal",
  "random": "off",
  "random_seed": 0,
  "song_index_profile": "safe"
}
```

## 모드 의미
- `format`: `auto | bms | osu`
- `key_mode`: `auto | 7k | 8k | 10k`
- `gauge`: `normal | hard | easy`
- `random`: `off | fr | sr`
- `random_seed`: 랜덤 고정 시드 (0도 고정 값으로 취급)
- `song_index_profile`: `safe | fast`
  - `safe`: large-library RAM high-water를 우선 낮추는 기본값
  - `fast`: 32GB+ 환경에서 더 빠른 재인덱싱을 노리는 선택값

## 랜덤 규칙
- **FR(Full Random)**: 레인 전체를 랜덤 **퍼뮤테이션**으로 치환
- **SR(Super Random)**: 노트별 랜덤 배치
  - 동일 레인에 **겹침(동시 시각 포함)**이 없도록 후보 레인을 선택
  - **롱노트는 헤드/테일을 동일 레인에 유지**
  - 후보 레인이 없을 경우 원래 레인을 유지하며 경고를 기록

## 키모드 처리
- `auto`는 차트 레인 수를 그대로 사용
- `7k/8k/10k`는 레인 수를 맞추되, **감소 시 범위를 벗어난 노트는 드롭**
- 증가(예: 10k → 7k) 외의 복잡한 레인 매핑은 추후 확장 예정

## 구현 위치
- 모드 파싱: `src/gameplay/ModeSettings.*`, `src/app/ModeResolver.*`
- 모드 적용: `src/gameplay/ModeApplier.*`
