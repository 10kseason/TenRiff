# TenRiff 1.7.1 로컬 수정본 r2 — 2026-09-10

이 빌드는 [앞선 로컬 수정본의 모든 기능](local-1.7.1-update.ko.md)에 재개 카운트다운, 세션 믹스 Esc 무시, 대표 BPM 계산을 추가한다. 공식 릴리스 번호는 1.7.1로 유지하며 배포 파일명에 `local-20260910-r2`를 붙인다.

## 동작 변경

- **일시정지 후 재개**: 일반 싱글플레이에서 Esc는 즉시 일시정지 메뉴를 연다. 계속 또는 다시 Esc를 누르면 3·2·1을 표시한 뒤 재개한다. 카운트다운 중 Esc는 일시정지 메뉴로 돌아간다. 연속 Enter나 방향키로 대기를 건너뛸 수 없다.
- **오디오와 판정**: 재개 카운트다운 동안 오디오 출력과 차트·판정 시계를 멈춘다. 오디오 장치의 샘플 시계로 3초를 재므로 렌더 FPS에 영향을 받지 않는다. 재개는 다음 오디오 버퍼 경계에서 이루어진다. 대기 중 입력은 모아서 판정하지 않으며, 현재 키 상태를 다시 맞춰 일시정지 중 놓은 LN이 계속 눌린 상태로 남지 않게 한다.
- **세션 믹스**: 로딩, 시작 카운트다운, 플레이 중 Esc 입력을 무시한다. Esc로 일시정지하거나 코스를 중단하지 않는다. 곡 사이 결과 화면에서 나가는 기존 조작은 유지한다.
- **대표 BPM**: 선곡 표시와 플레이 Hi-Speed 기준은 곡에서 누적 진행 시간이 가장 긴 BPM이다. 같은 BPM이 여러 번 나오면 시간을 합산하고, STOP 대기는 제외한다. 동률이면 실제로 먼저 쓰인 BPM을 고른다. 노트 수나 박자 수로 결정하지 않는다.
- **범위와 호환성**: BPM 계산은 0번 위치부터 차트에 선언된 마지막 마디 끝까지 포함한다. SCROLL 0·역방향도 진행 시간에는 포함하며, 오디오 파일에만 있는 뒤쪽 무음은 포함하지 않는다. 타이밍과 기존 리플레이·키/LN 변환은 원래 시작 BPM을 보존한다. Safe/Fast 인덱싱과 오디오 로더가 같은 타임라인 계산을 사용한다. [계산 규칙](reference-bpm.md).

곡 캐시 버전이 14에서 **15**로 바뀌므로 기존 소스를 열 때 캐시를 다시 만든다. 저장된 곡 파일이나 기록을 지우는 동작은 없다.

## 변경 위치와 이유

| 파일 | 역할 |
|---|---|
| `src/app/GameSessionTail.inl`, `GameSession.h`, `GameplayPauseMenu.h` | 기존 일시정지 시계에 재개 마감 샘플을 추가해 음원·판정·화면을 함께 정지 |
| `src/render/MenuWindow_draw_gameplay_body.inl` | 기존 시작 카운트다운 표시를 재개에도 사용 |
| `src/app/MenuAppTail.inl` | 세션 믹스 로딩 중 Esc 취소 경로 차단 |
| `src/chart/BmsTimeline.cpp`, `BmsTimeline.h` | 샘플 반올림 전에 BPM별 실제 진행 시간을 합산 |
| `src/chart/BmsParser.cpp`, `BmsParser.h`, `BmsChartNorm.cpp` | 저메모리 파싱에서도 마지막 선언 마디를 보존해 Safe/Fast 계산 일치 |
| `src/app/ChartLoader.cpp`, `ChartLoader.h`, `SongIndex.cpp` | 원래 BPM과 대표 BPM을 분리하고 기존 인덱싱 타임라인을 재사용 |
| `tests/unit/test_game_session_audio.cpp`, `test_bms_timeline.cpp`, `test_chart_loader.cpp`, `test_song_index.cpp` | 실제 오디오 콜백, BPM 경계 조건, 캐시·로더 일치 회귀 검사 |

새 타이머 스레드나 별도 차트 파싱 단계를 만들지 않고 기존 오디오 시계와 타임라인을 사용했다. 이전 리플레이를 새 BPM으로 다시 해석하지 않도록 `base_bpm`과 `reference_bpm`을 분리했다.

## 검증 결과

| 검사 | 결과 |
|---|---|
| MSVC Windows x64 Release 빌드 | 게임·리플레이 검증기·단위 테스트·NK3 smoke·판정 benchmark 성공 |
| Release CTest | **3/3 통과**, 단위 **749개 실행**, 통합 제외 0 |
| MSVC AddressSanitizer 빌드·CTest | **2/2 통과**, 단위 **739개 실행**, 기존 통합 검사 10개 제외 |
| 새 회귀 검사 | 카운트다운·Esc·LN 상태 4개, 타임라인·BPM·캐시 7개 통과 |
| 설치/임시 경로 | 한글이 포함된 경로에서 Release와 ASan 검사 통과 |

카운트다운 검사는 실제 `GameSession::audio_callback`을 합성 장치 시간으로 호출한다. 3·2·1 각 구간과 마지막 무음 버퍼, 정확한 차트 샘플에서의 음원 재개, 입력 누적 방지, 카운트다운 취소, 세션 믹스 Esc 무시를 확인했다. BPM 검사는 반복·분수 BPM, 채널 03/08, STOP, LN 꼬리와 마지막 미디어 마디, 동률, 빈 차트, 샘플레이트와 Rate, Safe/Fast, 캐시 무효화를 포함한다.

ASan 구성은 기존대로 NK3 ONNX 및 ONNX 업스케일러 통합을 제외한다. Release에는 해당 NK3 검사가 포함된다. 공개 소스에 없는 외부 `10k-calc` Python 참조는 양쪽에서 기존 선택적 skip 메시지를 출력한다. 새 검사 제외는 추가하지 않았다.

검증 로그는 작업 폴더의 `evidence-r2/build-release-final.log`, `ctest-release-final.log`, `build-asan-final.log`, `ctest-asan-final.log`다. 첫 Release 빌드의 Windows `max` 매크로 충돌은 `(std::max)` 호출로 수정했으며 실패 로그도 보존했다. 최종 빌드 로그에는 컴파일 오류·경고가 없다.

실제 오디오 장치에서 키를 눌러 수행하는 플레이와 화면 녹화, 장치별 지연·지속 FPS 검증은 수행하지 않았다. LR2 원본 실행 파일과의 게이지 A/B 및 프리셋 이식 범위는 [기존 보고서](local-1.7.1-update.ko.md)의 제한을 그대로 따른다. 기존 배포본과 설치 폴더는 보존한다.
