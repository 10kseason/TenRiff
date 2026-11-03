# TenRiff 1.9.9.9 — 선곡·기록 피드백과 리절트 영상 배경

2026-11-03

- 제공된 `continue_result_bga_ko_1080p60.mp4`를 기본 리절트 배경으로 사용합니다. 1920×1080·60fps·60초 무음 반복이며 기존 리절트 배치·지표·서체를 유지합니다. 배경 문구가 결과 수치를 가리지 않도록 낮은 불투명도로 합성합니다. 사용자 지정 배경 우선, 모션 감소 시 정지, 화면 이탈/최소화 시 디코더 정리를 적용합니다.
- 선곡은 10행 목록과 기존 클리어 로고를 사용합니다. 목록 숫자 점수 대신 EZ/NM/HD/EX CLEAR, 미플레이는 빈칸이며 폴더 곡 수·우측 MY BEST는 유지합니다. BPM/키 수/Lv/노트 수/판정 난이도와 실제 NPS MIN/AVG/MAX 막대, 12초·1080p30 선곡 배경을 포함합니다. 상세 정확도는 로비에서만 숨깁니다.
- NPS는 원본 차트의 TAP/LN 시작을 기준으로 빈 1초 구간까지 포함해 계산합니다. AVG는 산술 평균이며 막대만 최대96개로 집계합니다. 선택 변경 취소·비동기 완료 게시·미리듣기 OFF를 지원합니다. 랜덤 분기는 고정 예시이며 실행 중 분기와 다를 수 있습니다.
- 오디오에서 메인/클리어/실패 BGM과 버튼·옵션·슬라이더·시작·드래그·불러오기 완료 효과음을 선택할 수 있습니다(MP3/OGG/WAV/FLAC). 파일 선택기 스레드 초기화 충돌을 수정했습니다. Options는 Main Menu 3 음악, Clear는 이전 Main Menu 5 음악을 사용하고 Main Menu 4는 제외합니다.
- 점수가 1점 이상 오를 때 1.2초 점수 효과와 NEW RECORD를 표시합니다. 공식 BEST 저장은 높은 클리어 우선 정책을 유지하며, 클리어만 개선되고 점수가 오르지 않은 경우를 점수 신기록으로 표시하지 않습니다. 랭크 상승과 FULL COMBO 효과, 자동 게이지 하강의 리절트 그래프 연속성을 포함합니다.
- 선택 가능한 스킨 비교기는 호환 BEST 리플레이의 같은 시점 점수와 비교합니다. S/S+/S++/S+++ 줄은 95/97/98/99% 정확도 기준과의 차이(%p)입니다. 호환 BEST가 없으면 `--`, 기본값은 OFF이며 스킨에서 위치를 바꿀 수 있습니다. 유효한 일시정지 로컬 BEST 제외와 비교값 단독 갱신 누락을 수정했습니다.
- 폴더 곡 수/좌우 이동, 10 심볼, 입력 백엔드 문구 숨김, 긴 기록 제목 표시, 30프레임·1초 전환을 포함합니다. 리절트는 HTML 시안 적용 전 배치로 복원한 상태를 유지합니다.

[Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9) · [Validation](release-1.9.9.9-gate.md)

Extract into a new folder and run `launch_win.bat`. Keep existing installations, profiles and records. Existing scoring/judgement and R13/R14 replay/online contracts remain compatible. No website or database deployment. Fixed baseline: **1.9.6**. Physical input/listening, arbitrary skins/charts, long play and other PCs remain unverified.
