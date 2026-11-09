# Studio / Modern

이 스킨은 Modern Common 공통 디자인으로 전면 재구성했습니다. 메뉴·선곡·결과·스킨 미리보기·기어·노트·LN·글꼴을 공통 규격으로 맞추고 색상 테마로 구분합니다. 모든4~16키와 기존1~3키를 지원합니다. 폴더 ID와 v1 형식은 유지합니다.

판정선 아래 노트 가림과 판정선 명도 조절을 그대로 지원합니다. 점수·판정·오디오·입력·리플레이에는 영향을 주지 않습니다. 키 크기·판정선 위치·명도·노트 크기는 사용자 스킨 설정을 사용합니다.

원래 manifest는 `skin.legacy.json`에 보존했고 기존 이미지와 생성기는 그대로 남겼습니다. **구 디자인 복원:** 현재 skin.json을 별도 보관한 뒤 skin.legacy.json을 skin.json으로 복사하면 됩니다. 구 생성기는 기존 이미지용입니다. Modern 디자인 재생성은 저장소에서 `python -B tools/generate_modern_skins.py --write`, 무수정 검증은 `--check`를 사용합니다. 실행/게임 플레이에는 Python이나 인터넷이 필요하지 않습니다.

Modern Common uses the shared native menu/gameplay components with a theme accent. Original assets and credits remain unchanged. Restore the original design by copying skin.legacy.json to skin.json after preserving your current edits. Existing generators only rebuild the legacy artwork. The new geometric backdrop and component recipes are original TenRiff code under the repository MIT license.

[공통 디자인·설정 안내](../../docs/modern-skins-20261109.ko.md)

---

아래는 보존한 이전 디자인의 설명입니다. / Historical design notes follow.

# TenRiff Studio

메뉴용 스킨 / Menu skin / メニュースキン.

TenRiff 1.8.1 이상에서 Options → Skins → **TenRiff Studio**를 선택하세요.
기본 메뉴와 같은 기능 아이콘 18종을 교체 가능한 투명 PNG로 제공합니다.
플레이는 기본 렌더러를 사용합니다. 스킨 폴더 전체를 복사하면 다른 PC에서도
이미지 경로가 유지됩니다. 검정 악기 패널 배경과 얇은 중립색 테두리를 사용합니다.
실행에 Python이나 인터넷은 필요하지 않습니다.

옵션 아이콘 10종은 노랑·시안·보라·파랑·빨강·초록·주황·분홍·라임·청록을
사용합니다. 검정 카드의 모서리는 12px, 테두리는 기본 1.5px/선택 2.25px이며
선택 테두리는 아이콘 색을 따릅니다. 기본 벡터 아이콘은
스킨 편집기의 `options.icon.*` 색상으로 바꾸고, 이 PNG 스킨은 해당 PNG를
교체하거나 아래 생성 명령으로 다시 만듭니다.

Select **TenRiff Studio** in Options → Skins on TenRiff 1.8.1 or newer.
The manifest maps 18 menu asset roles to local 256×256 PNGs. Copy the complete
folder to keep these paths portable. Gameplay uses the native default renderer.
No Python or network access is needed at runtime.

The ten Options icons use distinct saturated colors while cards stay black, with
12px corners and 1.5px/2.25px borders. Selection follows the icon color.
`options.icon.*` edits compiled vector colors; supplied PNGs retain their authored
pixels. Replace those PNGs or regenerate them to change this portable skin.

The geometric artwork is original to TenRiff, authored with AI coding assistance
and licensed under the repository's MIT license. No generated raster artwork or
third-party game art is used. PNGs are raster exports of the editable vectors.

Source checkout only: `python skins/TenRiff_Studio/generate.py` regenerates PNGs
from `tools/generate_native_menu_assets.py`, using the renderer's Options icon
color defaults; `--check` verifies decoded pixels.
Pillow is required only for this authoring step.
