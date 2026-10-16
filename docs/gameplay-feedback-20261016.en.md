# TenRiff 1.9.3 gameplay, skin and model compatibility

This describes the 1.9.3 implementation following released 1.9.2 and the general ONNX work. Published assets and the fixed 1.9.0 baseline are preserved. See the [1.9.3 release scope](release-1.9.3.en.md) for publication and final-verification boundaries.

- Song Select shows authored BMS judgement difficulty at the album art's lower right: RANK 4/3/2/1/0 → VEZ/EZ/NM/HD/VHD. Missing or invalid values use EZ; osu! has no BMS badge. Cache 20 refreshes this metadata.
- Classic, Studio and battle HUD headers reserve a red level cell beside the artist, including long artist names. Editor practice and direct launch use the actual launched chart's PLAYLEVEL, never an unrelated selected song's level.
- BMS measure lines follow authored lengths, BPM, STOP, rate and scroll positions, behind notes. They do not add judgement or audio events.
- Always FAST/SLOW retains the last real timing text and marker. Conditional mode retains its 750ms expiry; text and bar switches stay independent. There is no invented initial FAST value. Existing PG suppression remains.
- New or missing indexing settings default to Safe. Explicit saved Fast choices remain. Native LV/CR calculation is still separately OFF by default.
- Empty Poor still drains gauge and counts as POOR, but no longer reduces ordinary, detailed or segment accuracy. Actual missed notes still reduce accuracy. New ordinary/Hard and EX records use R7 and R8; old R1–R6 replays retain their original behavior.
- Supported external Safetensors weights are converted locally to ONNX before the existing GPU path. Python with torch, safetensors, onnx and onnxruntime is required; other architectures need an optionally installed Spandrel. Nothing is installed or downloaded automatically. See the [model contract](../tools/onnx_upscaler/README.md).

RANK 4's VEZ badge is authored metadata; the existing Easy timing-window fallback is unchanged. Measure lines currently apply to BMS. Safetensors support is limited to known LunaSR2 C32 R8/R12/R16 x2 and exportable Spandrel RGB x1–x4 architectures. Unsupported models keep native image rendering and report a log diagnostic. BGA weights never ship; OFF/empty-path defaults and disabled video AI remain.

The matching production leaderboard version 19 was deployed with user approval on 2026-10-16. R7/R8 use separate boards while old board IDs, records and the fixed Stable grade ladder remain. Guest upload exclusion and the client-submitted trust boundary are preserved; this does not add server replay verification.
