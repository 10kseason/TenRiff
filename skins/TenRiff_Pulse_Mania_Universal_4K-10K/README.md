# Pulse Mania / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# PULSE MANIA // Universal 4K–10K

TenRiff Skin Format v1용 범용 키 스킨입니다.

## 콘셉트
- osu!mania 계열의 빠른 판독성을 참고한 오리지널 디자인
- 흰색 바 노트 + 시안 발광 + 핑크 타이밍 포인트
- 어두운 플레이필드와 밝은 리셉터
- 외부 HUD와 BGA를 방해하지 않는 저밀도 배경

## 4K–10K 호환 방식
- `note`, `hold_*`, `key_*`를 **문자열 하나**로 지정해 모든 레인에 공용 적용합니다.
- `column_widths`와 `column_spacings`를 지정하지 않아 현재 키 수의 기본 레이아웃을 그대로 사용합니다.
- `gear.png`에는 고정 레인 경계선을 넣지 않아 4K, 5K, 6K, 7K, 8K, 9K, 10K에서 어긋나지 않습니다.
- 중앙 전용 레인이나 홀수 키 전용 에셋이 없으므로 키 수 전환 시 배열 잘림이 없습니다.

## 설치
1. ZIP을 풉니다.
2. TenRiff에서 `Options > Skins > Import Skin`을 선택합니다.
3. 이 폴더 또는 `skin.json`을 지정합니다.
4. `Skin Source`를 `TenRiff`로 설정하고 `PULSE MANIA // Universal 4K-10K`를 활성화합니다.

## 권장 설정
- Judge Line: `0.84`
- Note Width: `0.94`
- Note Height: `0.86`
- Full Lane Receptors: `On`
- 1920×1080 또는 1280×720

## 검증 권장
- 4K / 5K / 6K / 7K / 8K / 9K / 10K
- 1080p / 720p windowed
- Performance HUD Off / On
- Gameplay Loading / Countdown / Live HUD / Result
- 짧은 LN, 긴 LN, 연타, 고밀도 동시치기

## 라이선스 메모
이 팩의 이미지와 설정은 이번 TenRiff 팩을 위해 새로 제작한 오리지널 리소스입니다.
osu! 또는 제3자 스킨의 이미지·로고·사운드를 복사하거나 포함하지 않습니다.
