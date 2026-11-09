> **Integrated in 1.9.9.9-final3.** The original candidate report below is historical. See [release guide](release-1.9.9.9-final3.ko.md) for final behavior and validation scope.

# 움직이는 스킨 배경 영상과 TenRiff Code / CLI 스킨 (2026-11-09 로컬 후보)

[English](skin-video-background-20261109.en.md) · [日本語](skin-video-background-20261109.ja.md) · [简体中文](skin-video-background-20261109.zh-CN.md)

사용자 요청: Claude Code CLI를 플레이하는 느낌의 스킨과 움직이는 배경. Modern Common 후보를 그대로 이어받은 별도 로컬 후보입니다.

## 스킨 배경 영상

- `lobby.background_video`: 모든 메뉴 화면의 공통 반복 영상. 내장 곡 선택/결과 반복 영상과 스킨 정지 배경을 대신합니다.
- `lobby.screen_videos.<화면 ID>`: 화면별 영상. 화면 → 대체 화면 → 공통 영상 순서이며, 빈 문자열은 그 화면을 정지 배경으로 둡니다.
- `gameplay.background_video`: 인게임 영상. 차트에 기본 BGA 레이어가 없을 때만 디코딩·표시하고 BGA와 같은 차트 시계(일시정지·배속·재시작)를 따릅니다. 모드별 덮어쓰기를 지원합니다.
- 형식은 MP4/M4V/MPG/MPEG/WMV/AVI/MOV/WEBM/MKV이며 이미지와 같은 상대 경로 규칙을 씁니다. 자동 탐색은 하지 않습니다. 오디오는 재생하지 않습니다.
- 영상은 같은 화면의 배경 불투명도로 정지 배경 자리에 그립니다. 첫 프레임 전, 시스템 애니메이션 끄기·메뉴 동작 줄이기, 디코딩 실패 때는 정지 배경을 씁니다. 스킨을 다시 불러오면 실패 기록을 지우고 다시 엽니다.
- 가져오기와 `.trskin` 내보내기는 참조된 영상도 복사합니다. 스키마·동봉 에디터(검증·폴더 열기·ZIP 내보내기)도 새 필드를 지원합니다. 에디터 미리보기 캔버스는 정지 배경을 표시합니다.

## 영상 디코더 수정

H.264 1080p는 내부적으로 1088줄로 디코딩되는데, Media Foundation 경로가 표시 영역(1080줄)으로 자르지 않아 화면 맨 아래에 초록 줄 4개가 보였습니다. 내장 곡 선택/결과 반복 영상에서도 재현되었습니다. 디코더가 `MF_MT_MINIMUM_DISPLAY_APERTURE`(없으면 geometric aperture)를 읽어 잘라내도록 고쳤습니다. 표시 영역 정보가 없거나 잘못되면 기존처럼 전체 버퍼를 씁니다.

## TenRiff Code / CLI 스킨

내장 스킨 `TenRiff_Code_CLI`: Cascadia Mono 글꼴, 코랄 강조색, 따뜻한 검정 패널, 블록 커서 노트, 선택 영역 모양 LN, 커서 밑줄 건반, `pixels` 히트 효과를 씁니다. 메뉴는 스피너 글리프가 떠다니는 16초 영상, 타이틀은 입력창 타이핑 영상, 인게임은 세션 로그가 타이핑되는 24초 영상을 씁니다. 1K~16K와 7+1을 지원합니다. Claude Code CLI 분위기를 참고한 비공식 팬 스킨이며, Anthropic 로고·마스코트·상표 이미지는 쓰지 않았습니다. 모든 이미지·영상은 동봉 생성기가 Pillow로 그렸습니다.

## 바뀌지 않는 것

판정·점수·게이지·입력·오디오·리플레이·랭킹 정책, `version: 1`, 기존 스킨과 프로필 설정은 그대로입니다. 영상 필드가 없는 스킨의 동작도 같습니다. 단, 1080p H.264 메뉴 영상의 하단 4줄 잘라내기는 내장 영상에도 적용됩니다. 실제 물리 입력, 장시간 재생, 다른 PC의 코덱 환경은 별도 검증 대상입니다.
