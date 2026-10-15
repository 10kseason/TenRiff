# Portable skin presets

For 1.8.8 target-lane number shortcuts and their visual left-to-right order, see [skin lane controls (Korean)](release-feedback-1.8.8.ko.md#스킨의-숫자-레인-선택). These shortcuts select an editing target; the `.trskin` format is unchanged.

In **1.9.2**, `Options > Skins` retains **Import Skin Preset** and removes
**Export Skin Preset**. Existing `.trskin` files remain supported; dropping one
onto the Skins screen also imports it. The imported appearance is applied
immediately and saved to the active profile. Full-profile export and its internal
skin packaging remain available; the container format is unchanged.

**1.9.2에서는 스킨 화면의 스킨 프리셋 내보내기를 제거합니다.** 기존 `.trskin`
가져오기·파일 드롭과 전체 프로필 내보내기는 유지합니다. 아래 내용은 기존 파일과
프로필 내부 스킨 패키징의 형식·검증 규칙입니다.

프리셋 파일 하나에 모든 키 모드의 스킨 설정과 현재 활성 스킨의 이미지가 함께
들어갑니다. 받는 컴퓨터에서 **스킨 프리셋 가져오기**를 선택하면 같은 설정을
적용합니다. 원래 스킨 폴더가 없는 컴퓨터에서도 사용할 수 있습니다.

Included: `SkinConfig` (lane palettes and widths, note appearance, judgement and
combo positions, scratch position, UI font choice, and the other skin settings),
plus the active TenRiff manifest and referenced assets for 1K–16K and 7+1, or a
self-contained LR2 playskin tree. Inactive skin catalog choices are omitted.
TenRiff license/readme companions are retained when present.

Audio volume, device selection, input/keymaps, account details, chart sources,
display timing calibration and graphics resolution are not included. System font
choices use the fonts available on the receiving computer. Presets use the same
TenRiff/LR2 renderer support as the normal skin import; this does not add missing
LR2 renderer features.

Imports reserve a new directory under `profiles/<profile>/skins/tenriff` or `lr2`;
name collisions get a numeric suffix. Existing skins remain selectable. The new
configuration is applied only after extraction and validation succeed. Native
presets have no external assets. The internal skin-container writer requires a
new filename and never replaces an existing preset.

LR2 includes/images must resolve inside the selected skin folder. Internal
`../` references are supported when they remain inside that folder; missing,
absolute or external assets cause a visible failure. Make an LR2 theme
self-contained before sharing it. Only LR2 text and supported image/document
files are packaged, not programs or scripts. The portable loader accepts at most
128 LR2 include commands to bound recursive parsing.
Wildcard references must use local paths without `..` or `LR2files/Theme/...`,
whose legacy fallback can select a different sibling skin after import. Rewrite
those patterns relative to the selected skin folder before exporting.
The existing Windows LR2 path decoder's UTF-8/CP932 fallback is retained. UTF-16
LR2 text is rejected with a conversion message because the current renderer does
not parse it. This is separate from the BMS chart text decoder.

The dependency-free version 1 container has an eight-byte `TRSKIN\r\n` signature,
little-endian version and JSON length, the skin JSON, then a little-endian file
count. Each entry has a UTF-8 relative path length/path, 64-bit byte length,
uncompressed asset bytes and CRC32. Limits: 1 MiB settings/manifest JSON, 4,096
files, 1,024-byte paths, 128 MiB per asset and 512 MiB total assets. JSON nesting
is bounded before parsing. Asset data is streamed in 64 KiB blocks.

Import rejects path traversal, rooted/device/alternate-stream filenames,
duplicate paths, symlinks/junctions, invalid lengths, truncated assets and CRC
mismatches. An unsuccessful import cleans up only its newly reserved directory.
The CRC detects file damage; it is not a publisher signature.
