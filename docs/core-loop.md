# 코어 플레이 루프(초기 구현)

이 문서는 현재 구현된 **코어 플레이 루프**의 구성과 데이터 흐름을 정리합니다.

## 핵심 흐름
1. **InputThread**가 RawInput 또는 폴링 입력을 수집 → `SPSCQueue`로 전달
2. **AudioThread** 콜백에서 `ClockSync`로 입력 타임스탬프를 샘플 타임으로 변환
3. **GameplayEngine**가 입력/타임라인을 소비하여 판정·게이지·통계를 갱신
4. 오디오 버퍼는 현재 **무음(fill 0)** 처리 (키사운드/음원 믹싱은 후속)

## 주요 구성 요소
- `config/Config.*`
  - `config.json`을 **SimpleJson** 파서로 로드
  - `audio/input/judge/speed/gauge/ui/offsets` 섹션 반영
- `config/Keymap.*`
  - `keymap.json` 로드 및 기본 키맵 생성
  - `KeycodeMap`으로 키 문자열 → 키코드 변환
- `gameplay/GameplayChart.*`
  - BMS/OSU 타임라인을 **샘플 타임 기반 노트 이벤트**로 변환
  - `rate` 적용 시 스케줄을 `t' = t / rate`로 스케일
- `gameplay/GameplayEngine.*`
  - 판정 윈도우( PG/GR/GD/BD )와 마스크(30ms) 적용
  - POOR 발생 시 레인 마스크 적용
  - **Hold 규칙**: 조기 릴리즈는 BAD
  - **Hold Tail 규칙**: osu!mania hold와 BMS `#LNMODE 2` charge note만 릴리즈 타이밍을 일반 판정 윈도우로 평가(헤드/테일 50:50)
  - 일반 BMS long note는 끝까지 유지하면 tail이 자동 처리되고, tail release timing 판정은 사용하지 않음
  - 결과 통계(콤보, 판정 카운트, 평균/표준편차) 수집
- `app/GameSession.*`
  - CLI 옵션 → 설정 적용 → 차트 로드 → 입력/오디오 스레드 시작
  - 오디오 콜백에서 입력 큐 소비 + 판정 갱신

## 판정 관련 초기 정책(확인 필요)
- **Hold Tail 판정**은 osu!mania hold와 BMS `#LNMODE 2` charge note에만 적용되며, 조기 릴리즈는 BAD 처리

## 향후 연결 예정
- 메뉴 상태 머신(Title/SongSelect/Gameplay/Result)
- SongIndexerThread + 캐시
- 키 리맵 UI + NKRO 테스트 화면
- ✅ Result 화면 + 리플레이/결과 JSON 저장
- 런처 확장 및 로그/환경 진단
