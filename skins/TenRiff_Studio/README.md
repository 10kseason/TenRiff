# TenRiff Studio

메뉴용 스킨 / Menu skin / メニュースキン.

TenRiff 1.8.1 이상에서 Options → Skins → **TenRiff Studio**를 선택하세요.
기본 메뉴와 같은 기능 아이콘 18종을 교체 가능한 투명 PNG로 제공합니다.
플레이는 기본 렌더러를 사용합니다. 스킨 폴더 전체를 복사하면 다른 PC에서도
이미지 경로가 유지됩니다. 검정 악기 패널 배경과 얇은 중립색 테두리를 사용합니다.
실행에 Python이나 인터넷은 필요하지 않습니다.

Select **TenRiff Studio** in Options → Skins on TenRiff 1.8.1 or newer.
The manifest maps 18 menu asset roles to local 256×256 PNGs. Copy the complete
folder to keep these paths portable. Gameplay uses the native default renderer.
No Python or network access is needed at runtime.

The geometric artwork is original to TenRiff, authored with AI coding assistance
and licensed under the repository's MIT license. No generated raster artwork or
third-party game art is used. PNGs are raster exports of the editable vectors.

Source checkout only: `python skins/TenRiff_Studio/generate.py` regenerates PNGs
from `tools/generate_native_menu_assets.py`; `--check` verifies decoded pixels.
Pillow is required only for this authoring step.
