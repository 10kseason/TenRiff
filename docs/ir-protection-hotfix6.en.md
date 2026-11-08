> Historical local-candidate note; superseded by [1.9.9.9-final1](release-1.9.9.9-final1.en.md).

# TenRiff hotfix6 local IR protection candidate

Based on hotfix5. The server rejects a new score if any individual playable keysound lasts at least 45 original seconds, or at least 85% of actual playable heads are silent. The duration is not a sum, song length, BGM-only audio, or playback-rate-adjusted length.

A background upload worker reconstructs the original BMS/selected branch at replay sample rate, speed and note modes. Taps and LN heads count once; tails do not count. A head is silent when no associated keysound has any nonzero decoded PCM. Source duration is independent of rate; values at or above45 seconds saturate to45.0 to bound decoding memory and remain rejected by the server. Missing files, decode failures, changed charts and absent scan data fail closed.

Native BMS is supported. Converted charts are not submitted. Historical osu metadata cannot be scanned by the current BMS-only loader. Native PCM/float WAV, complete single-stream Vorbis OGG and strict Media Foundation MP3 are checked. MF-only WAV and non-Vorbis OGG may be unavailable and stay local. This is digital-zero detection and client-submitted evidence, not perceptual silence detection or server anti-cheat validation.

Use a separate extracted folder. Input, judgement, scoring and historical replay format are unchanged. Existing website connection can be reused; older clients lack the new scan data and cannot submit after the server gate is enabled.

Confirmed exclusions show `IR EXCLUDED` (`IR 제외` in Korean) on the selected list row, detail and result. Known blocked charts and incompatible current play conditions show immediately. A separate worker scans only the selected pattern, never the entire library or periodically while idle. Selection/revisit, F5 and relevant settings changes trigger a fresh scan. After replacing audio while keeping the same selection, reselect the pattern or press F5.

Unknown/checking states never claim eligibility. Authored RANDOM charts remain check-required until the actual branch is selected. Results inspect the completed replay even for guests, independently of current menu settings and connection status. Missing/changed/uninspectable audio shows an unavailable reason; network errors do not blacklist patterns. No badge promises successful registration, and submission always runs a fresh scan.
