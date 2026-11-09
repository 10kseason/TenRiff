# Neon Step / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# NEON STEP // Arcade Matrix 4K–10K

TenRiff Skin Format v1용 StepMania 감성의 오리지널 아케이드 스킨입니다.

## 핵심
- 4K 기본 배열: `← ↓ ↑ →`
- 5K~10K: 기본 4방향 뒤에 대각선·보조 화살표를 순서대로 추가
- 노트, 롱노트, 리셉터를 10개 레인 배열로 구성
- `column_widths`와 `column_spacings`를 생략해 현재 차트의 기본 레이아웃 사용
- `gear.png`에는 고정 레인 경계선을 넣지 않아 키 수 전환 시 어긋나지 않음
- 로비는 네온 아케이드, 체커보드 원근, 대형 화살표 모티프 중심

## 설치
1. ZIP 압축을 풉니다.
2. TenRiff에서 `Options > Skins > Import Skin`을 엽니다.
3. 이 폴더 또는 `skin.json`을 선택합니다.
4. `Skin Source`를 `TenRiff`로 바꾸고 스킨을 활성화합니다.

## 권장 설정
- Judge Line: `0.82`
- Note Width / Height: `0.94`
- Full Lane Receptors: `On`
- 1920×1080 권장

## 호환성
배열은 왼쪽부터 적용됩니다.
- 4K: 1~4번
- 7K: 1~7번
- 10K: 1~10번

실제 TenRiff 렌더러에서 4K~10K를 한 곡씩 스모크 테스트하는 것을 권장합니다.

## 저작권
StepMania의 일반적인 화살표 리듬게임 문법에서 영감을 받은 오리지널 리소스입니다.
StepMania, DDR 또는 제3자 노트스킨의 이미지·로고·사운드를 포함하지 않습니다.
