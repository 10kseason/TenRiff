# 1.9.9.8 hot fix 1 validation

The pre-release candidate reproduced missing WASAPI output and missing ASIO driver failures through the real MenuApp/GameSession path. The previous release failed all eight new visible-guidance checks. The corrected candidate passed 625 native checks, including four languages, saved-device preservation and successful play/retry.

Release acceptance requires a fresh Release build, CTest 3/3 (1463 unit/integration cases), native 625/625, source/archive byte verification, clean defaults, privacy review, PR/main CI and fresh uploaded-asset readback. The release attachment checksums identify the delivered files.

Initial sandbox localhost connection failures passed with the same build outside the sandbox. Optional external 10k-calc Python reference is unavailable. Physical device unplug/replug, a physically device-less PC, listening, long play, arbitrary skins and other PCs remain unverified. Missing endpoint IDs/drivers exercise the failure boundary without changing system devices. No production score or account writes are part of verification.
