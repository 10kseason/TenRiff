# 곡 목록·폴더·난이도표 로딩 개선

이 로컬 후보는 1.9.9.5 인디케이터·리절트 후보를 그대로 계승합니다. 곡 선택 후 플레이 준비는 이번 범위에서 제외합니다. 공개 버전과 일반 고정 기준선1.9.6은 유지합니다.

- 난이도표 전환: 로딩이 끝난 곡 목록의 해시에 새 표만 적용합니다. 이전에는 곡 캐시 전체와 로컬 기록/리플레이 검증을 다시 읽었습니다. 같은 URL의 새 내용은 계속 가져와 적용하며, 단일 폴더와 ALL SONG 모두 지원합니다.
- Fast→Safe 변경, 빈 목록, 실행 중 또는 아직 전달되지 않은 스캔 결과가 있으면 기존 전체 갱신 경로를 사용합니다. F5 수동 재색인과 폴더 갱신의 검증은 유지합니다. 단순 표 전환은 외부에서 바뀐 곡/기록 파일을 새로 읽는 동작이 아니므로 필요하면 명시적으로 새로 고칩니다.
- 폴더의 파일 수를 세는 단계에서 경로 정규화/크기/수정시간을 중복 조회하지 않습니다. 실제 읽기 단계의 경로 처리, 권한 오류, 취소, 배치 메모리 제한과 메타데이터 검증은 유지합니다.
- 캐시 파싱 결과를 복사 대신 이동하고, 선택 곡 유지에 전체 메타데이터 복사 대신 표시 인덱스를 사용합니다. 정렬·필터·선택 경로 보존 규칙은 같습니다.

파일 형식, 점수/판정/입력/오디오/리플레이/온라인 계약과 모델은 변경하지 않았습니다. 별도 library-loading-local 프로필을 사용하며 기존 설치와 후보를 보존합니다.

성능 비교는 합성 BMS 2,000개/200폴더와 10,000항목 캐시를 사용합니다. HTTP는 로컬 응답 함수이고 기록은 비어 있으며 OS 파일 캐시는 지우지 않습니다. 실제 사용자 라이브러리, 디스크의 완전한 콜드 캐시, 네트워크 지연, 장시간 사용 및 다른 PC에 대한 측정은 아닙니다. 수치와 실패/재검증은 후보 폴더의 evidence/VERIFICATION.ko.md와 benchmark-comparison.json을 참고하세요.

## Benchmark

Build the explicit `library_loading_benchmark` target, then run `build/Release/library_loading_benchmark.exe <new-output-directory>`. It refuses existing output paths and uses no real profile or network. The synthetic 10,000-entry cache includes copied metadata entries whose extra paths do not exist; only the first 2,000 charts are used to benchmark filesystem scans. This tool is excluded from default builds and end-user packages.

## 반복 측정 중앙값

| 구간 | 이전(ms) | 수정 후(ms) | 단축률 |
|---|---:|---:|---:|
| 폴더 탐색(2,000곡) | 1774.08 | 1320.67 | 25.6% |
| 저장 목록 열기(10,000곡) | 233.99 | 230.95 | 1.3% |
| 난이도표 적용(10,000곡) | 382.94 | 103.05 | 73.1% |
| 기본 LV 복귀(10,000곡) | 283.67 | 20.68 | 92.7% |

3회 전후 쌍을 순차 실행하고 두 번째 쌍은 순서를 뒤집었습니다. 폴더 탐색/목록 열기는 각 9개, 표/기본 LV는 각 15개 표본입니다. 저장 목록 열기의 1.3% 차이는 작고 변동 범위 안이므로 체감 향상으로 주장하지 않습니다.
