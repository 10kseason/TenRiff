# TenRiff native menu vectors

Eighteen original vector assets for TenRiff Studio: mark, prism, chevron, spark,
wave, audio, display, input, network, sliders, folder, exit, keys, keymap, skin,
latency, profile and keytest. The shapes were authored with AI coding assistance;
no third-party game art, image generation service or downloaded media is used.
The 10/waveform mark is a vector reconstruction of the user-provided TenRiff
reference emblem. The existing TenRiff wordmark and menu layout are retained.

`tools/generate_native_menu_assets.py` is the source of truth. It writes these
editable SVGs and `src/render/NativeMenuAssets.h` from the same geometry.
The game embeds the geometry and caches its Direct2D paths on the GPU.

Run `python tools/generate_native_menu_assets.py --check` to verify parity.
For a custom skin, replace any role through `native.assets` with a relative
PNG/JPEG/BMP/TGA path. SVGs are authoring sources; export to PNG for runtime use.
The offline editor and vector sources are included in the Windows package.
The bundled `TenRiff_Studio` skin connects 17 roles to portable PNG exports and
uses the embedded vector for the mark, so its logo stays in sync with the client.
Its earlier mark PNG is preserved as an unused legacy asset. `tools/skin_editor/sync_catalog.py`
also embeds the same vector geometry into the offline editor preview catalog.

The art uses the repository's MIT license. Gameplay sprites are unchanged.
