# TenRiff 1.8.9.7 — 입력 스케줄링·로비 테두리·자체 음악

## 변경 범위

- 입력 생산자 스레드에서 MMCSS `Games`/High를 요청한다. 기존 Above Normal을 fallback으로 설정하고 MMCSS 적용 후 일반 우선순위를 다시 덮어쓰지 않는다.
- 입력 스레드의 `THREAD_POWER_THROTTLING_EXECUTION_SPEED`를 끈다. 이전 상태를 읽어 성공 시 저장하고, 스레드 종료 시 복원한다. API 미지원/실패는 한 번 로그로 남기고 기존 입력 동작을 계속한다.
- RawInput과 Polling의 공통 진입점에 수명을 묶어 in-place fallback, 초기화 실패, 정상 종료를 모두 처리한다. 전역 전원 계획·레지스트리·affinity·오디오 우선순위는 바꾸지 않는다.
- 메인 로비의 네 메뉴 테두리에 아이콘과 같은 하늘색 `#79D8FA`, 붉은색 `#F06B7B`, 흰색 `#F2F4F7`과 은은한 glow를 적용한다. 네 캐시 슬롯을 재사용하며 선택 시에는 밝기만 변경한다. 클릭 영역과 배치는 유지한다. 외부 PNG 아이콘은 기존처럼 원본 픽셀을 유지하며 이미지에서 색을 추출하지 않는다.
- 텐리프 자체 음악(타이틀·메뉴·멀티 로비·선곡 화면 배경음·결과)의 재생 지점에만 0.4배를 적용한다. BMS 곡의 배경음·키음·변환된 소리·선곡 및 편집기 미리듣기, UI 효과음은 원래 음량 스케일을 유지한다. 슬라이더 표시/저장값과 기존 범위도 유지한다. Windows 볼륨 40%의 청감 곡선을 복제하지 않는다.

입력 timestamp, 타이머 주기, 1.8.9.6의 정밀 대기, 판정 수치와 R1~R5, 점수·게이지·리플레이·BMS 오디오 믹싱·인게임 렌더링은 유지한다. 고정 기준선 `1.8.7`의 태그/자산/문서는 변경하지 않는다. 웹사이트·API·DB 변경은 없다.

## 검증 항목

릴리스 게이트는 Windows Release 빌드 및 CTest, 오디오 PCM 회귀, 에디터 검사, 실제 MenuApp/GameSession 무음 WASAPI 검사, 네 로비 선택 상태의 Direct2D 캡처와 hit geometry, 입력 스케줄링의 API 성공/실패·정리·재시작 검사를 확인한다. ZIP CRC·추출 해시·Git 소스 동등성·독립 추출 소스 빌드·개인정보 검사 후 PR/main CI와 공개 파일 readback을 확인한다.

로컬 선행 시험에서는 1,169 tests, 앱/세션 220 checks, 로비 세 fixture 360 measured Presents와 12 PNG가 통과했다. 선택적 외부 Python reference 비교는 이 테스트 개수와 별도로 생략될 수 있으며 실행 로그에서 구별한다. 실제 측정 증거는 각 릴리스 작업의 별도 검증 기록에 보존하며 이 수치를 물리 입력/청감/성능 개선의 증명으로 사용하지 않는다.

## 확인하지 못한 범위

MMCSS와 전원 스로틀링 해제는 동시에 적용되어 개별 효과가 분리되지 않았다. CPU 사용률만으로 원인이나 해결 여부를 판정할 수 없다. 실제 키 누름→판정→소리 지연, CPU/전력 변화, 스피커 청감, 장시간·다른 PC의 안정성은 별도 검증 대상이다. Preview hit geometry는 물리 마우스 입력과 구별한다. 캐시 재사용 설계가 특정 FPS 향상을 보장하지 않는다.

## English scope

The input producer requests MMCSS Games/High and opts out of execution-speed power throttling with owner-thread cleanup and failure fallback. Home borders use icon-matched cached glow. Only TenRiff scene music uses a 0.4 reference gain; BMS songs, keysounds and previews retain their previous scale. Fixed baseline 1.8.7 and gameplay/replay contracts are preserved. No universal stutter, physical-latency or CPU/power improvement is claimed.

## API 근거

- [Microsoft MMCSS](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service)
- [SetThreadInformation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadinformation)
- [AvRevertMmThreadCharacteristics](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avrevertmmthreadcharacteristics)
