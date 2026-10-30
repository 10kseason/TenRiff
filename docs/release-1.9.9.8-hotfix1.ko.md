# TenRiff 1.9.9.8 hot fix 1 — 2026-10-30

오디오 출력 초기화에 실패해 선곡 화면으로 돌아오면 하단에 황색 안내를 표시합니다. **옵션 → 오디오 → 사운드 출력 장치**에서 연결된 장치를 선택한 뒤 다시 시작하세요. ASIO는 설치된 드라이버를 선택합니다. 정상 세션 초기화 후 안내가 사라집니다.

[Details](audio-device-guide-20261030.ko.md) · [Validation](release-1.9.9.8-hotfix1-gate.md)

The old failure text was only drawn inside the difficulty-table URL editor. The fix uses the visible song-selection footer. The saved output choice is retained; there is no silent output switching. Audio processing, input, judgement, scoring, R13/R14 and replay/peer contracts remain unchanged. This does not change the multiplayer loading-error screen. No website or database deployment. Fixed baseline: **1.9.6**.

[Release and downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.8-hotfix1). Extract into a new folder and run launch_win.bat. Preserve existing installations and profiles.
