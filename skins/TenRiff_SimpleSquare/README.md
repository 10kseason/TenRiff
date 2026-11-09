# Simple Square / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# TenRiff Simple Square / 심플 네모

스킨 설정에서 소스를 **TenRiff**, 스킨을 **TenRiff Simple Square**로 선택하세요.
검은 필드, 평평한 네모 노트·롱노트 끝·키와 단색 롱노트 몸통으로 구성됩니다. 레인 색과 크기는 프로필 설정을 사용합니다.
외부 이미지 없이 Native 벡터로 그리므로 4K–16K와 스크래치 배치에서도 크기를 바꿀 수 있습니다.

Select **TenRiff → TenRiff Simple Square** in Skins. Flat rectangular notes,
hold caps, solid hold bodies and keys use the profile's lane colors and dimensions. No image assets
are needed; edit `gameplay.native.sprites` to customize the shapes.
