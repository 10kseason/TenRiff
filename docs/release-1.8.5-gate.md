# TenRiff 1.8.5 릴리스 검증

기준일: 2026-10-06. 1.8.42 이후 노트 렌더링 CPU 비용을 단계적으로 줄이는 클라이언트 업데이트입니다.

## 변경과 선택 이유

VSync OFF 플레이 화면에서 지원되는 노트 이미지를 원래 순서대로 GPU sprite batch에 제출합니다.
이전에는 각 이미지를 별도 DrawBitmap 호출로 보냈습니다. 겹침 순서와 클립 영역을 보존하면서
반복 제출만 줄입니다. 8개 미만의 작은 묶음, 지원하지 않는 변환·crop·이미지,
절차적으로 그리는 LN body·mine은 기존 경로를 유지합니다. VSync ON도 기존 경로입니다.
캐시는 소스 이미지를 포함하여 32MiB로 제한하고 스킨·장치 변경 시 무효화합니다.

텍스트 캐시·정적 화면 캐시·대기 추정기 실험은 전체 CPU 또는 프레임 꼬리 지연이 개선되지 않아
이번 릴리스에 포함하지 않습니다. 노트 표시량, FPS, 입력·오디오 주기, 스크롤·판정 및
R1/R2/R3/R4 리플레이·고스트 규칙은 유지합니다. 기존 R4 랭킹 계약을 그대로 사용하며
웹 배포나 DB 마이그레이션은 필요하지 않습니다.

릴리스 전 복귀 경로 검사에서 발견한 결과 화면 마우스 버튼 문제도 수정합니다.
결과 화면 버튼은 일반 설정 목록과 별도의 데이터로 구성되므로 설정 행 검증을 거치지 않고
결과 화면의 활성화·버튼 번호·연출 완료 조건으로 검사합니다. 일반 설정 행의 선택/조절 정책은 유지합니다.

## 성능 근거와 한계

개발 중 RTX 4060 Ti, FHD, 10K 합성 BMS 160 notes/s, 활성 창의 재생 3~18초를 측정했습니다.
300Hz/VSync OFF 한 쌍에서 전체 프로세스 CPU는 0.410884→0.364589 core equivalents
(-11.27%), Present/s는 298.449→298.471, 프레임 p99는 3.7514→3.7393ms였습니다.
1050 FPS에서는 CPU 감소가 0.90%였으며 조건별 개선 폭은 다릅니다. 이는 제보된 RTX 4070이나
6코어 12스레드 PC의 측정이 아니며, 90% 감소·물리 입력 지연 개선·끊김 완전 해결을 뜻하지 않습니다.
VSync ON 실험은 p99 악화로 채택하지 않았습니다.

이미지 batch는 정수 색 양자화에서 작은 차이가 생길 수 있습니다. 픽셀 비교는 그 차이와
실제 배치/클립/레이어 오류를 구분하며, 작은 묶음과 fallback은 기존 출력과 비교합니다.

## 공개 전 검증 기준

최종 결과와 바이너리 해시는 릴리스 노트 및 로컬 배포 증거에 기록합니다.

- Release 빌드·CTest와 에디터 검사, 압축 해제한 소스의 독립 빌드·CTest.
- Title, Song Select/로비, Sources, Records, Result, 옵션의 Audio/Mode/Graphics/Skins/Profile/Keymap 내부 화면과 조작 영역.
- 메뉴→플레이→메뉴 왕복 및 실제 MenuApp 로딩·플레이·일시정지·복귀 경로.
- FHD 창/독점 전체화면 × VSync OFF/ON의 Present와 화면 모드 복원, 노트/고스트/LN 출력 비교.
- ZIP CRC·추출 SHA-256·정확한 병합 Git 소스 일치·개인정보 검사.
- PR 및 main의 ASan/OpenVINO CI, 초안과 익명 공개 첨부 파일 재다운로드·해시 확인.

이전 릴리스·설치·프로필은 보존합니다. 실제 사용자 파일 대화상자, 물리 입력/오디오 지연,
장시간 플레이와 다중 PC 검증은 자동 fixture와 별도입니다. 로비/메뉴 회귀가 발견되면
CPU 개선 여부와 관계없이 수정과 재검증을 먼저 수행합니다.

## 개발용 재검증

Windows에서 `menu_visual_preview`와 `menu_app_smoke`는 명시적으로 빌드하는 개발 도구이며
실행 패키지에 포함하지 않습니다. 소스 패키지에서는 다음과 같이 빌드합니다.

```powershell
cmake --build build --config Release --target menu_visual_preview menu_app_smoke
.\build\Release\menu_visual_preview.exe --graphics-settings --small --reduced-motion --verify-hits --hitmap hitmap.json --benchmark-frames 30 --benchmark-json benchmark.json
.\build\Release\menu_app_smoke.exe .\new-smoke-sandbox
```

각 실행은 새 출력 폴더를 사용합니다. 앱 smoke는 기존 경로를 거절하고 격리한 합성 차트·프로필을 사용합니다.
`--mode-settings`는 실제 Mode 설정 화면, `--menu-roundtrip`은 같은 렌더러의 메뉴/플레이/메뉴 왕복을 검사합니다.
`--no-gpu-sprites`로 기존 노트 경로를 비교할 수 있습니다. `gpu_sprite_stats.submissions`는
실제 batch 제출 수이고 `draw_calls`에는 작은 묶음의 기존 방식 제출도 포함됩니다.
