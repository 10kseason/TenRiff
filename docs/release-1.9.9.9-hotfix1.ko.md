# TenRiff 1.9.9.9-hotfix1 — OGG 보안 수정·범용 BGA·스킨 에디터

- 내장 stb_vorbis의 comment 크기·개수, 곱셈·정렬, codebook 제한과 부분 할당 실패 정리를 수정했습니다. probe와 decode 모두 같은 수정 파서를 사용합니다. [적용한 upstream SHA와 의존성](../external/stb_vorbis.TENRIFF.md)을 기록하고 라이선스를 보존했습니다.
- 옵션 > 그래픽에서 **범용 BGA 영상**을 고르고 **범용 BGA**를 켜면 모든 곡의 원래 BGA와 오버레이를 같은 무음 반복 영상으로 대체합니다. BGA 표시가 꺼져 있으면 재생하지 않습니다. 기본값은 OFF/빈 경로입니다. [사용법](common-bga-20261104.ko.md).
- 오프라인 스킨 에디터의 버전·그래픽 14개 항목·선곡/리절트 예시와 한국어/영어/일본어 도움말을 갱신했습니다. 범용 BGA는 게임 프로필 설정이며 skin.json v1에는 넣지 않습니다. 브라우저 미리보기는 예시이므로 실제 스킨은 게임에서 확인하세요.

기존 Media Foundation fallback을 유지합니다. 이 PC에는 Opus-in-OGG 핸들러가 없어 실제 Opus 재생 호환성은 확인하지 못했습니다. 정상 Vorbis mono/stereo·여러 sample rate·65초 음원은 수정 전후 PCM이 같았습니다. 특이하게 큰 codebook은 upstream 제한에 따라 거절합니다.

Windows ZIP을 새 폴더에 풀어 `launch_win.bat`으로 실행하세요. `tools/skin_editor/Launch-Skin-Editor.cmd`는 오프라인 편집기를 엽니다. 기존 설치·프로필·기록을 보존하세요. 고정 기준선은 **1.9.6**입니다. 판정·리플레이·온라인 기록·오디오 타이밍 변경이나 웹/DB 배포는 없습니다.

[Verification / 검증](release-1.9.9.9-hotfix1-gate.md)
