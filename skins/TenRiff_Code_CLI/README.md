# TenRiff Code / CLI

터미널에서 코딩 에이전트와 세션을 진행하는 느낌으로 TenRiff를 플레이하는 스킨입니다.
따뜻한 검정 터미널, 코랄 강조색, 고정폭 글꼴, 둥근 박스 프레임, 스피너 글리프(· ✢ ✳ ✶ ✻ ✽),
`⏺ Read(…)` / `⎿` 도구 호출 로그와 diff 색을 TenRiff 화면에 옮겼습니다.

Claude Code CLI의 분위기를 참고한 **비공식 팬 스킨**입니다. Anthropic의 로고·마스코트·상표 이미지는
쓰지 않았고, 화면 속 문구는 TenRiff용으로 새로 쓴 것(`TenRiff Code`, `Riffing…` 등)입니다.

## 화면 구성

| 위치 | 내용 |
|---|---|
| 메뉴 전체 | Cascadia Mono 글꼴, 코랄 강조색, 낮은 대비의 따뜻한 검정 패널, 둥근 6px 카드 |
| 메뉴 배경 (영상) | `cli/lobby-loop.mp4` 16초 반복: 떠다니며 회전하는 스피너 글리프, 숨 쉬는 창 조명 |
| 타이틀 배경 (영상) | `cli/title-loop.mp4` 16초 반복: 입력창에 `play --single`이 타이핑되고 `✶ Riffing…` 스피너가 돎 |
| 인게임 배경 (영상) | `cli/gameplay-loop.mp4` 24초 반복: 왼쪽에 세션 로그가 타이핑되고 오른쪽에 `Update(combo.log)` diff와 판정 로그가 나타남 |
| 노트 | 터미널 블록 커서 모양의 각진 단색 블록. 흰색·라벤더·코랄(가운데/스크래치) |
| 롱노트 | 레인 색 테두리와 어두운 채움으로 된 선택 영역 모양 |
| 건반 | 어두운 키캡과 레인 색 커서 밑줄. 누르면 커서 블록이 켜짐 |
| 판정선·이펙트 | 코랄 판정선, `pixels` 히트 효과 |

지원 키 모드: 1K~16K 전체와 BMS 7+1(스크래치 코랄)입니다.

## 영상 배경과 호환성

움직이는 배경은 TenRiff 1.9.9.9-final3에 추가된 `lobby.background_video`,
`lobby.screen_videos`, `gameplay.background_video` 필드를 사용합니다.

- 영상이 첫 프레임을 내기 전, 시스템 애니메이션 끄기(동작 줄이기) 상태, 디코딩 실패 시에는 같은 이름의 정지 이미지
  `cli/lobby.png`, `cli/title.png`, `cli/gameplay.png`를 씁니다.
- 인게임 영상은 차트에 기본 BGA 레이어가 없을 때만 디코딩·표시합니다. BGA가 있는 곡에서는 BGA가 우선합니다.
  인게임 영상은 차트 시계를 따라가므로 일시정지·배속·재시작에 맞춰 멈추고 움직입니다.
- 공개 final2나 Modern 후보처럼 영상 필드를 모르는 클라이언트에서는 스킨 설정에 "지원하지 않는 필드" 경고가 뜨고
  정지 배경으로 동작합니다. 기능 자체는 그대로 쓸 수 있습니다.

## 설치

- 이 후보 클라이언트에는 내장 스킨으로 들어 있습니다. 설정 → 스킨 → 스킨 소스를 TenRiff로 바꾸고
  `TenRiff Code / CLI`를 고르세요.
- 다른 설치본에는 `TenRiff_Code_CLI` 폴더(또는 압축을 푼 폴더)를 설정 → 스킨 → 스킨 가져오기로 가져오거나
  스킨 화면에 끌어다 놓습니다. 영상 3개도 함께 복사됩니다.
- 메뉴 글꼴은 Windows 11에 기본 설치된 Cascadia Mono를 씁니다. 없으면 Windows 글꼴 대체 규칙을 따릅니다.
- HUD 배치(Studio/Classic), 게이지 폭, 키 오버레이 같은 프로필 설정은 바꾸지 않습니다.

## 자산 출처

모든 이미지와 영상은 `generate/make_skin.py`, `generate/make_videos.py`가 Pillow로 직접 그렸습니다
(Claude Code로 작성, AI 이미지 생성 서비스나 내려받은 이미지 없음). 문자는 Windows 글꼴
Cascadia Mono(SIL OFL 1.1), Segoe UI Symbol, 맑은 고딕으로 래스터화했고 글꼴 파일은 포함하지 않습니다.
영상은 ffmpeg(libx264)로 인코딩한 무음 1920×1080 30fps H.264 MP4입니다.

재생성: Windows에서 Pillow·fontTools와 libx264가 있는 ffmpeg를 준비한 뒤 이 폴더에서
`python -X utf8 generate/make_skin.py --out .` 와 `python -X utf8 generate/make_videos.py --out cli`를 실행합니다.

## English summary

An unofficial terminal-session skin for TenRiff: monospace type, coral accent, rounded box frames, spinner
glyphs and agent-style tool logs. Menus, title and gameplay use silent looping videos through the new
`lobby.background_video`, `lobby.screen_videos` and `gameplay.background_video` fields of this local candidate;
the same-named PNG stills remain the fallback. Supports 1K–16K and BMS 7+1. All art is procedurally drawn by
the bundled generators; no third-party logos, mascots or downloaded images are used.
