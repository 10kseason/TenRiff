# TenRiff native menu vectors

Twelve original vector assets drawn for TenRiff: mark, prism, chevron, spark,
wave, audio, display, input, network, sliders, folder and exit. No third-party
game art, image generation service or downloaded media is used.

`tools/generate_native_menu_assets.py` is the source of truth. It writes these
editable SVGs and `src/render/NativeMenuAssets.h` from the same geometry.
The game embeds the geometry and caches its Direct2D paths on the GPU.

Run `python tools/generate_native_menu_assets.py --check` to verify parity.
For a custom skin, replace any role through `native.assets` with a relative
PNG/JPEG/BMP/TGA path. SVGs are authoring sources; export to PNG for runtime use.
The offline editor and vector sources are included in the Windows package.

The art uses the repository's MIT license. Gameplay sprites are unchanged.
