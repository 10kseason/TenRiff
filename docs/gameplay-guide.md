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
- `Left` / `Right`: 좌측 메뉴 포커스 전환
- `Esc`: 이전 화면으로 복귀
- `F5`: 곡 라이브러리 재인덱싱

### Song Select에서 자주 쓰는 화면
- `Mode`
  - Gauge, Random, Rate, Hi-Speed, OSU Charts, Chart Filter 조정
- `Audio`
  - Master/BGM/Keysound 볼륨과 BMS keysound 정책 조정
- `Graphics`
  - VSync, Refresh Hz, Performance HUD, Display Offset 조정
- `Skins`
  - 판정선 위치, 노트 크기, LN 몸통 폭, lane color 조정
- `Keymap`
  - 키 배치 변경과 NKRO 테스트

## 4. 기본 추천 설정

처음에는 아래 정도로 시작하면 무난합니다.

- `Mode > Gauge`: `normal`
- `Mode > Rate`: `1.0x`
- `Mode > Hi-Speed`: 기본값 그대로 시작
- `Graphics > Performance HUD`: 필요할 때만 켜기
- `Graphics > Display Offset`: 기본값 `0ms`에서 시작
- `Audio > Keysound Mode`: BMS는 `follow` 권장

노트가 너무 느리거나 빠르게 보이면 우선 `Hi-Speed`만 조정하고, 판정은 맞는데 화면만 늦거나 빠르게 보이면 `Display Offset`을 조정하는 식으로 접근하면 됩니다.

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
- 기본 필터는 BMS 중심입니다.
- osu!mania는 옵션에서 활성화하면 `4K`부터 `10K`까지 인덱싱/플레이할 수 있습니다.

### 로딩
- 곡 시작 직후에는 차트 로딩 진행 상태가 표시될 수 있습니다.
- 로딩 중 `Esc`를 누르면 시작을 취소하고 Song Select로 돌아갑니다.

### 카운트다운
- 로딩이 끝나면 `3 / 2 / 1` 카운트다운이 먼저 보입니다.
- 카운트다운 중 입력은 점수 판정에 반영되지 않습니다.

## 7. 플레이 중 조작

- 차트 키 입력: 현재 키맵 기준
- `Esc`: 플레이 중단
- `F3`: Hi-Speed 감소
- `F4`: Hi-Speed 증가
- `F5`: Hi-Speed 크게 감소
- `F6`: Hi-Speed 크게 증가
- `F9`: 현재 화면 스크린샷 저장

Hi-Speed는 시각 스크롤 속도만 바꾸고, 판정 타이밍 자체를 바꾸지는 않습니다.

## 8. HUD 읽는 법

인게임 HUD에서는 보통 아래 정보를 확인하게 됩니다.

- 제목 / 아티스트
- BPM
- 현재 `Rate`
- 현재 `Hi-Speed`
- Gauge 값과 현재 Gauge 종류
- Combo
- 최근 판정(`PG / GR / GD / BD / PR`)
- 타이밍 편차(ms)

`Graphics > Performance HUD`를 켜면 프레임 그래프와 평균 FPS, low FPS, gameplay timing 디버그 정보도 볼 수 있습니다.

## 9. 판정과 게이지

### 판정
현재 기본 판정 표시는 아래 약어를 사용합니다.

- `PG`: Perfect Great
- `GR`: Great
- `GD`: Good
- `BD`: Bad
- `PR`: Poor / Miss

### 게이지
선택 가능한 기본 게이지는 아래 세 가지입니다.

- `hard`
- `normal`
- `easy`

선택한 게이지는 곡 시작 시 항상 `100%`에서 시작합니다.

- `hard`: `0%`가 되는 즉시 Game Over
- `normal`: `0%`가 되는 즉시 Game Over
- `easy`: `0%`가 되는 즉시 Game Over

플레이 중 자동 단계 시프트는 없습니다.

## 10. Result 화면

플레이가 끝나면 Result 화면에서 아래 내용을 확인할 수 있습니다.

- Clear / Game Over 상태
- Rank
- Score
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
- 필요하면 `Skins > Note Width / Note Height / Judge Line`도 같이 조정합니다.

### 판정은 맞는 것 같은데 화면이 늦거나 빠르게 보인다
- `Graphics > Display Offset`을 조정합니다.
- 양수는 노트를 더 일찍 그립니다.

### BMS에서 건반음이 너무 크거나 작다
- `Audio > Keysound Volume`을 조정합니다.
- 배경음과 분리해서 조절할 수 있습니다.

### 입력이 불안하거나 키 충돌이 의심된다
- `Options > Keymap > NKRO Test`에서 여러 키를 동시에 눌러 확인합니다.
- 필요하면 키 배치를 바꿔 손 배치가 겹치지 않게 조정합니다.

## 12. 추천 적응 순서

처음 시작하면 아래 순서가 가장 편합니다.

1. `Song Select`에서 쉬운 곡 하나 선택
2. `Mode`에서 `Gauge=Normal`, `Rate=1.0x` 확인
3. 플레이 후 `Hi-Speed`만 먼저 조정
4. 그래도 어색하면 `Display Offset` 조정
5. 손이 불편하면 `Keymap` 수정
6. 마지막에 `Skins`로 노트 크기와 판정선 위치 조정

## 13. 관련 문서

- 현재 구현 상태: [`docs/current-state.md`](current-state.md)
- 설정 구조: [`docs/config.md`](config.md)
- 메뉴 구조: [`docs/menu.md`](menu.md)
- 플레이 루프/엔진 구조: [`docs/core-loop.md`](core-loop.md)
