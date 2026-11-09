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
