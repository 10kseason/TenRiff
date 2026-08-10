# TenRiff Gameplay Guide

이 문서는 "TenRiff를 처음 실행한 사람이 실제로 곡을 고르고 플레이하고 결과를 확인하는 방법"에 집중한 사용자용 가이드입니다.

세부 설정 구조는 [`docs/config.md`](config.md), 현재 구현 범위는 [`docs/current-state.md`](current-state.md)를 참고하면 됩니다.

## 1. 처음 실행하기

Windows 기준으로 보통 아래 두 방법 중 하나를 사용합니다.

```powershell
.\launch_win.bat
```

또는

```powershell
.\build-dist\Release\TenRiff.exe --songs .\songs --profile default
```

첫 실행이면 기본 프로필이 자동으로 생성됩니다.

## 2. 기본 흐름

TenRiff의 기본 플레이 흐름은 아래 순서입니다.

1. Title 화면 진입
2. Song Select에서 곡 선택
3. 필요하면 Mode / Audio / Graphics / Skins / Keymap 조정
4. 곡 시작
5. `3 / 2 / 1` 카운트다운 후 플레이
6. Result 화면 확인
7. `Enter` 또는 `Esc`로 Song Select 복귀

## 3. 기본 메뉴 조작

### Title
- `Up` / `Down`: 메뉴 이동
- `Enter`: 선택
- `Esc`: 종료

### Song Select
- `Up` / `Down`: 곡 이동
- `PageUp` / `PageDown`: 빠른 이동
- 마우스 휠: 곡 이동
- 좌클릭: 곡 선택
- 더블클릭: 곡 시작
- `Enter`: 현재 곡 시작
- 우측 `BEST SCORE` 카드 좌클릭: 현재 곡의 최고 Result 열기; Result의 `WATCH REPLAY` 또는 `F1`로 리플레이 재생
- 좌측 `RECORDS`: 현재 곡의 로컬 기록을 둘러보고 `OPEN RESULT`로 선택한 결과 열기
- `Left` / `Right`: 좌측 메뉴 포커스 전환
- `Esc`: 이전 화면으로 복귀
- `-` / `+`: 다음 플레이의 Rate 즉시 조정
- `F5`: 곡 라이브러리 재인덱싱; 실행 중에는 stage/퍼센트/ETA와 progress bar가 화면 중앙에 표시됨
- `Browse > Difficulty Table`: 링크를 복사한 뒤 `Enter`로 BMSTable HTML/header 가져오기, `Right`로 로컬 JSON 선택, `Left`로 해제

### Song Select에서 자주 쓰는 화면
- `Mode`
  - Ghost Battle, Autoplay, Practice, Sudden Death, Key Mode, Gauge, Random, Mods, Rate, Hi-Speed 조정
- `Audio`
  - Master/BGM/Keysound 볼륨과 BMS keysound 정책 조정
- `Graphics`
  - VSync, Refresh Hz, Performance HUD, BGA 표시, 외부 ONNX BGA Upscaler 조정
  - `BGA`를 끄면 게임플레이 이미지/영상과 관련 디코더·업스케일러 작업이 꺼지며 Song Select 미리보기는 유지됨
  - 모델 선택 후 Upscaler를 직접 켜고 고사양 경고를 확인해야 함. `저전력 DirectX(실험)`은 DirectXMinPower 요청일 뿐 NPU를 명시 선택하거나 검증하지 않음
- `Skins`
  - native/LR2 스킨 전환, 비주얼 레이턴시, native 하단 디지털 피아노 건반(홀드 눌림·타격 글리치), LR2 폴더 하나 이식 또는 독립적인 non-IIDX `LR2files/Theme` 일괄 이식(IIDX 의존 테마 제외), 필드 크기에 연동해 확대되고 판정선 아래로 clip되는 원본 종횡비 하단 Gear 프레임, 고정 레인선 기준 노트 간격·크기, 검은 플레이필드, 판정선 위치, LN 몸통 폭, lane color 조정
- `Keymap`
  - 키 배치 변경과 NKRO 테스트

## 4. 기본 추천 설정

처음에는 아래 정도로 시작하면 무난합니다.

- `Mode > Gauge Shift Start`: `normal`
- `Mode > Sudden Death`: 첫 OD8 환산 `MISS` 즉사 도전을 원할 때만 켜기
- `Mode > Rate`: `1.0x`
- `Mode > Hi-Speed`: 기본값 그대로 시작
- `Graphics > Display`: Discord 음성 오버레이를 쓸 때는 `Borderless` 권장
- `Graphics > Performance HUD`: 필요할 때만 켜기
- `Skins > Visual Latency`: 기본값 `0ms`에서 시작
- `Audio > Keysound Mode`: BMS는 `follow` 권장

노트가 너무 느리거나 빠르게 보이면 우선 `Hi-Speed`만 조정하고, 판정은 맞는데 화면만 늦거나 빠르게 보이면 `Visual Latency`를 조정하는 식으로 접근하면 됩니다.

### Discord 음성 오버레이

Discord의 `User Settings > Game Overlay`에서 오버레이와 Voice 위젯을 켠 뒤, TenRiff는 `Graphics > Display > Borderless` 또는 `Windowed`로 실행하세요. 현재 Discord Game Overlay는 DXGI 독점 전체 화면에서는 표시되지 않습니다. 플레이 정보와의 겹침을 줄이려면 Voice 위젯을 좌하단에 고정하고 TenRiff의 `Performance HUD`는 꺼 두는 구성을 권장합니다. Discord가 TenRiff를 자동 인식하지 못하면 `Registered Games`에서 실행 중인 `TenRiff.exe`를 추가해야 합니다.

Discord 설정 방법은 [공식 Game Overlay 안내](https://support.discord.com/hc/en-us/articles/217659737-Game-Overlay-101)를 참고하세요.

## 5. 기본 키 배치

기본 키맵은 차트 키 수에 따라 자동 선택됩니다.

- `4K`: `D F L ;`
- `5K`: `D F K L ;`
- `6K`: `S D F J K L`
- `7K`: `W E R M I O P`
- `8K`: `W E R V M I O P`
- `9K`: `A S D F Space H J K L`
- `10K`: `Q W E R V M I O P [`
- `16K`: `Q W E R A S D F U I O P J K L ;`

원하는 배치가 아니면 `Options > Keymap`에서 바꿀 수 있습니다.

## 6. 플레이 시작 전 알아둘 점

### 차트 형식
- 차트 인덱싱과 플레이는 BMS 계열(`.bms/.bme/.bml/.pms`) 전용이며 `.osu`는 지원하지 않습니다.

### 로딩
- 곡 시작 직후에는 차트 로딩 진행 상태가 표시될 수 있습니다.
- 로딩 중 `Esc`를 누르면 시작을 취소하고 Song Select로 돌아갑니다.

### 카운트다운
- 로딩이 끝나면 `3 / 2 / 1` 카운트다운이 먼저 보입니다.
- 카운트다운 중 입력은 점수 판정에 반영되지 않습니다.

## 7. 플레이 중 조작

- 차트 키 입력: 현재 키맵 기준
- `Esc`: 싱글플레이에서는 일시정지 메뉴(계속하기 / 재시작 / 나가기), 멀티플레이에서는 플레이 중단
- F3: Hi-Speed 감소
- `F4`: Hi-Speed 증가
- `F5`: Hi-Speed 크게 감소
- `F6`: Hi-Speed 크게 증가
- `F9`: 현재 화면 스크린샷 저장

Hi-Speed는 시각 스크롤 속도만 바꾸고, 판정 타이밍 자체를 바꾸지는 않습니다.
Rate는 곡 재생 속도와 차트 스케줄만 바꾸며, 같은 Hi-Speed에서 시각 스크롤 속도를 바꾸지 않습니다.
BPM 변속이 있어도 곡 시작 BPM에 맞춘 초당 스크롤 속도를 유지하며, 차트의 명시적 `#SCROLL`·정지·역주행 효과는 그대로 적용됩니다.

## 8. HUD 읽는 법

인게임 HUD에서는 보통 아래 정보를 확인하게 됩니다.

- 제목 / 아티스트
- BPM
- 현재 `Rate`
- 현재 `Hi-Speed`
- Gauge 값과 현재 Gauge 종류
- TenRiff native Score
- Combo
- 최근 판정(`PG / GR / GD / BD / PR`)
- 타이밍 편차(ms)

정타에서 벗어난 입력은 판정명 아래에 조기 입력 `FAST -12 ms`, 지연 입력 `SLOW +18 ms`처럼 부호와 함께 표시됩니다. `0 ms`로 반올림되는 입력은 타이밍 문구를 생략합니다.

`Graphics > Performance HUD`를 켜면 프레임 그래프와 평균 FPS, low FPS, gameplay timing 디버그 정보도 볼 수 있습니다.

## 9. 판정과 게이지

### 판정
현재 기본 판정 표시는 아래 약어를 사용합니다.

- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

OD8 환산 통계는 Sudden Death 판정과 기존 replay 호환을 위해 JSON 내부에 유지되지만 게임플레이·결과 화면에는 표시하지 않습니다.

일반 Score는 최대 10,000점이며 `PG 6 / GR 3 / GD 1 / PR 0 / FAIL 0` 비율로 계산합니다. LN은 머리와 꼬리를 각각 0.5 가중치로 계산해 한 객체가 되며, 상세 점수는 기존의 별도 계산을 유지합니다.

Accuracy는 `PG / GR / GD / BD = 100 / 80 / 50 / 20%`를 기준으로 각 판정 구간 안의 실제 타이밍에 따라 최대 0.5%p를 더 감점합니다. 전부 `PG`여도 PG 타이밍 범위가 8ms를 넘으면 99.5%를 초과하지 않습니다.

Rank는 `<75 F`, `75 B`, `80.5 A`, `86.5 A+`, `90 S`, `95.5 S+`, `98 AA`, `99 SS`, `99.75 SSS` 경계를 사용합니다.

### 게이지
선택 가능한 게이지는 아래 다섯 가지입니다.

- `ex_hard`
- `hard`
- `normal`
- `easy`
- `shift`

고정 게이지(`ex_hard / hard / normal / easy`)는 곡 시작 시 `100%`에서 시작하며 플레이 중 타입이 바뀌지 않습니다.

- `ex_hard`(화면 표기 `EX`): Hard보다 회복이 낮고 `BAD`/`POOR` 손실이 더 큰 도전용 시작 게이지. Hard의 붉은색과 다른 짙은 흑회색으로 표시
- `hard`: `0%`가 되는 즉시 Game Over
- `normal`: `0%`가 되는 즉시 Game Over
- `easy`: `0%`가 되는 즉시 Game Over
- Gauge Shift는 항상 적용되며 선택한 시작 등급부터 Easy까지를 각각 100%에서 병렬 계산합니다. 현재 tier가 0%로 탈락하면 같은 판정을 누적해 온 다음 생존 tier를 선택하고, 종료 시 가장 높은 생존 tier로 확정합니다.

기존 `shift` 설정값은 EX 시작으로 자동 해석됩니다.
`Sudden Death (1 MISS)`는 게이지 종류가 아니라 첫 OD8 환산 객체 `MISS`에서 게이지를 0으로 만들고 즉시 종료하는 규칙입니다. 네이티브 `BAD`만으로는 즉사하지 않고 빈 키 `POOR`도 세지 않으며 Practice No-Fail과 동시에 켤 수 없습니다.

## 10. Result 화면

플레이가 끝나면 Result 화면에서 아래 내용을 확인할 수 있습니다.

- Clear / Game Over 상태
- Rank
- TenRiff native Score
- 해당 플레이의 퍼즈 사용 여부
- Accuracy
- Max Combo
- PG / GR / GD / BD / PR 집계
- 평균 타이밍 편차와 분산
- 최종 Gauge와 게이지 기록
- 저장된 replay/result 파일명

복귀 키는 아래와 같습니다.

- `Left`: 같은 차트를 바로 다시 시작
- `F1`: 저장된 replay가 있으면 replay 재생
- `Enter`: Song Select 복귀
- `Esc`: Song Select 복귀

## 11. 자주 하는 조정

### 노트가 너무 촘촘하거나 읽기 어렵다
- `Hi-Speed`를 올립니다.
- 필요하면 `Skins > Note & Field Size / Note Height / Judge Line`도 같이 조정합니다.

### 판정은 맞는 것 같은데 화면이 늦거나 빠르게 보인다
- `Skins > Visual Latency`를 조정합니다.
- 양수는 노트를 더 일찍 그립니다.

### BMS에서 건반음이 너무 크거나 작다
- `Audio > Keysound Volume`을 조정합니다.
- 배경음과 분리해서 조절할 수 있습니다.
- `follow`인데도 키음이 안 났던 늦은 입력 경로는 1.2.6에서 판정 시각은 유지하고 가청 시작점만 현재 쓰기 가능한 버퍼에 맞추도록 수정했습니다.
- 계속 무음이면 `Keysound Mode=follow`, 0이 아닌 볼륨, 차트의 `#WAV` 파일 경로를 확인합니다.

### 입력이 불안하거나 키 충돌이 의심된다
- `Options > Keymap > NKRO Test`에서 여러 키를 동시에 눌러 확인합니다.
- 필요하면 키 배치를 바꿔 손 배치가 겹치지 않게 조정합니다.

## 12. 추천 적응 순서

처음 시작하면 아래 순서가 가장 편합니다.

1. `Song Select`에서 쉬운 곡 하나 선택
2. `Mode`에서 `Gauge=Normal`, `Rate=1.0x` 확인
3. 플레이 후 `Hi-Speed`만 먼저 조정
4. 그래도 어색하면 `Skins > Visual Latency` 조정
5. 손이 불편하면 `Keymap` 수정
6. 마지막에 `Skins`로 노트 크기와 판정선 위치 조정

## 13. 관련 문서

- 현재 구현 상태: [`docs/current-state.md`](current-state.md)
- 설정 구조: [`docs/config.md`](config.md)
- 메뉴 구조: [`docs/menu.md`](menu.md)
- 플레이 루프/엔진 구조: [`docs/core-loop.md`](core-loop.md)
