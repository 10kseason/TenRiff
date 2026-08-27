# Changelog

TenRiff의 사용자/배포 관점에서 의미 있는 변경만 간단히 기록합니다.

## [1.5.1 Hotfix 3] - 2026-08-27

### Fixed

- `F10` 계정 창의 TenRiff 메인 API 기본값과 기존 로컬 기본 프로필을 `https://121.174.18.181:27303`으로 이전했습니다. 게임 코디네이터는 `121.174.18.181:27301`을 계속 사용합니다.

### Release

- stable baseline과 앱 버전은 `1.5.1`을 유지하며, 수정 자산은 `1.5.1-hotfix.3` 태그로 별도 배포합니다.

## [1.5.1 Hotfix 2] - 2026-08-27

### Changed

- 멀티플레이 메뉴의 기본 접속값을 TenRiff 메인 코디네이터 `121.174.18.181:27301`로 변경했습니다.

### Fixed

- Windows ARM64 빌드가 x86 전용 processor pause intrinsic을 포함해 실패하던 문제를 수정했습니다.

### Release

- stable baseline과 앱 버전은 `1.5.1`을 유지하며, 수정 자산은 `1.5.1-hotfix.2` 태그로 별도 배포합니다.

## [1.5.1 Hotfix 1] - 2026-08-27

### Fixed

- 혼합 실시간 입력 소스의 audio sample이 일시적으로 역전될 때 v3 replay가 비정렬 상태로 저장되어 서버 랭킹 검증이 거부되던 문제를 수정했습니다.
- 동일 BMS의 재구성 길이가 Windows와 Linux에서 1 sample 다르게 반올림될 때 서버 검증이 거부되던 문제를 수정했습니다. lane 수는 계속 정확히 일치해야 하며 허용 오차는 1 sample로 제한됩니다.

### Release

- stable baseline과 앱 버전은 `1.5.1`을 유지하며, 수정 자산은 `1.5.1-hotfix.1` 태그로 별도 배포합니다.

## [1.5.1] - 2026-08-27

### Added

- `F10` 계정 창에서 메인 TenRiff 서버 또는 사용자가 입력한 사설 API 서버를 선택한 뒤 로그인·회원가입할 수 있습니다. 비밀번호 칸은 공백을 보존한 `Ctrl+V` 붙여넣기를 지원하고, 서버에서는 PBKDF2-HMAC-SHA256 600,000회로 해시하며 클라이언트의 저장 계정 자격 증명은 Windows DPAPI로 보호합니다.
- 로그인 계정 기반 글로벌 채팅을 추가하고 `F8`로 메뉴·플레이·결과 어디서나 하단 오버레이를 열고 닫을 수 있습니다.
- 글로벌 채팅의 `/np` 명령은 현재 재생 중인 곡명과 작곡가를 전송합니다.
- 채팅 URL은 즉시 열지 않고 부드러운 전환이 있는 위험 경고 창에서 주소를 확인하고 명시적으로 승인한 뒤 엽니다.
- 새 BMS는 기록 제출 시 서버 catalog에 등록되고 검증 후 리더보드에 반영됩니다. 관리자는 제외 목록으로 특정 BMS SHA-256을 언제든 차단할 수 있습니다.
- 관리자 계정과 서버 단위 글로벌 채팅, 로그인 세션, BMS catalog 갱신 도구를 자체 호스팅 서버에 추가했습니다.
- 멀티플레이 메뉴는 로그인한 계정이 선택한 메인/사설 서버에서 인증된 방 목록을 검색하고, 로그아웃 상태에서는 기존 LAN 방 검색을 유지합니다.

### Changed

- 대량 인덱싱은 로컬 곡 검색만 담당하고 랭킹 catalog를 자동으로 채우지 않습니다. 실제로 제출된 새 BMS만 등록 후보가 됩니다.
- 멀티플레이 기본 포트를 `27301/TCP`, 로컬 API를 `127.0.0.1:27302`, HTTPS 게이트웨이를 `27303/TCP+UDP`로 고정했습니다. `27304~27305`는 예약하고 기존 `27300`, `80`, `443` 공개 매핑을 제거했습니다.
- 온라인 기록 제출은 로그인 계정과 서버 challenge에 묶인 replay를 전송하고, 실패 시 로컬 기록을 그대로 유지합니다.

### Fixed

- 필터가 적용된 곡 선택 화면에서 온라인 기록이 제출되지 않던 경로를 수정했습니다.
- 게임 포트 `27301`과 API 포트 `27302`를 혼동하던 로컬 서버 주소 정규화를 수정했습니다.

### Release

- `1.5.1`은 이후 작업이 보존해야 하는 고정 stable baseline입니다. 기준 문서는 `docs/baseline-1.5.1.md`입니다.
- 정식 자산은 `TenRiff-1.5.1.zip`, `TenRiff-1.5.1-source.zip`, `TenRiff-1.5.1-SHA256SUMS.txt`입니다.
- 호환 자체 호스팅 서버는 별도 `TenRiff Server v1.1.0` 릴리즈로 배포합니다.

## [1.5.0] - 2026-08-26

### Added

- 완성된 TenRiff `skin.json` 스킨을 루트 `skins/`에서 자동 발견하고 빌드 출력과 Windows 배포 패키지에 함께 넣습니다. 프로필에 같은 이름의 스킨이 있으면 사용자 사본이 번들 사본보다 우선합니다.
- 검증 가능한 랭킹 서버, 로컬/온라인 기록 화면, BMS 전용 랭크 자격과 osu 관련 실행 경로의 랭크 제외를 단계적으로 도입하는 신뢰 계획을 추가했습니다.
- 일반 8K와 분리된 `7+1` 스킨 설정 및 TenRiff 매니페스트 오버라이드, 스크래치 좌우 표시 위치를 추가했습니다.
- 난이도표·멀티플레이 `host:port`·인랭 기록 서버 주소를 클립보드에서 바로 가져오거나 Ctrl+V로 교체할 수 있습니다.
- 별도 공개 TenRiff Server가 계정·만료 세션, 승인 BMS SHA-256 catalog, 일회성 challenge, replay 업로드와 외부 `tenriff-replay-verifier` 재실행, SQLite 기록, HMAC 검증 영수증을 제공합니다. `.osu`와 클라이언트 점수 claim은 등록하지 않습니다.
- `Random (Scratch Fixed)`를 추가해 스크래치는 고정하고 건반만 섞습니다.
- StepMania 계열의 대담한 로비·선곡·결과 화면을 갖춘 번들 스킨과 AI 스킨 제작용 `AGENTS.md`, 제작 가이드, JSON Schema를 제공합니다.
- Local/Online Records 화면, HTTPS 서버 주소 설정, 검증 상태·거부 사유·서버 정보 표시와 실패 시 로컬 기록 유지 동작을 추가했습니다.
- 곡 필터 상태 저장, BMS 패턴 파일의 노트 수 표시, 스킨별 프로필 아바타 크기 조절을 추가했습니다.
- 멀티플레이 메뉴 어디서든 F8로 채팅 입력을 열 수 있으며 플레이·결과 중에는 입력 충돌을 막습니다.

### Changed

- `examples/skins/`는 최소 제작 템플릿 `TenRiff-Example` 하나만 유지하고, 기존 완성 예제 9개를 배포용 `skins/`로 이동했습니다.
- 마지막 노트 판정 뒤에는 기본적으로 곡 오디오가 끝날 때까지 기다리며, 그 구간에서 레인 키를 누르면 점수·리플레이에 입력을 추가하지 않고 결과 화면으로 바로 넘어갑니다.
- 숫자가 포함된 난이도표 레벨은 별도 `level_order`가 없을 때 `1, 2, 3, ... 10, 11`의 자연 순서로 정렬합니다.
- 스킨·옵션의 마우스 증감 버튼은 키보드 선택 행을 옮기지 않고, 바뀐 행만 3회 점멸해 변경을 알립니다.
- 일반 Random은 플레이/재도전마다 새 세션 seed를 만들며 replay에는 실제 seed를 저장해 재생과 서버 검증의 결정성을 유지합니다.

### Fixed

- Velocity Circuit / Dance Grid를 포함한 스킨 메뉴·결과 레이아웃의 긴 문구를 칸 너비에 맞춰 자동 축소하고 경계 밖으로 넘치지 않게 했습니다.
- 원격 인랭 기록 서버는 HTTPS만 허용하고 localhost 개발 주소에만 HTTP를 허용하며, HTTPS에서 HTTP로 내려가는 리다이렉트를 거부합니다.

### Release

- 정식 자산은 `TenRiff-1.5.0.zip`, `TenRiff-1.5.0-source.zip`, `TenRiff-1.5.0-SHA256SUMS.txt`입니다.
- 자체 호스팅 서버는 별도 [`10kseason/TenRiff-Server`](https://github.com/10kseason/TenRiff-Server) 저장소와 `v1.0.0` Windows/Linux/GHCR 자산으로 배포합니다. 중앙 공식 서버 주소는 이 릴리스에 포함하지 않습니다.

## [1.4.5.3] - 2026-08-25

### Added

- Skin Settings에 `Create New Skin`, `Open Skin Folder`, `Reload Skin`을 추가하고 표준 파일명 자동 감지, `F5` 핫 리로드, 레인 파일 패턴과 `1k..16k` 모드별 덮어쓰기를 지원합니다.
- TenRiff 스킨이 20개 화면의 개별 배경/투명도, 메뉴 팔레트와 셰이더 장면 색, Title/Song Select/Result 및 화면별 목록 레이아웃, 게임플레이 시각 옵션을 지정할 수 있습니다.
- 공식 `10kseason/TenRiff` 경로의 JSON Schema, 확장 예제, 알 수 없는 키/타입/범위 경고를 제공합니다.
- AI 에이전트용 `skins/AGENTS.md`, 제작 가이드, 1K~16K 범용 `Agent Prism Universal` 완성 예제를 제공합니다.
- NK3 P64와 2K~18K 일반화 MLP를 ncnn FP32 모델로 변환해 Vulkan을 지원하는 AMD/NVIDIA GPU에서 실행합니다.

### Changed

- TenRiff 매니페스트는 선택/새로고침 시 키 모드별 불변 스냅샷으로 해석하며 렌더 경로에서 JSON이나 폴더를 다시 읽지 않습니다. 가져오기는 모든 모드가 참조한 자산을 함께 복사합니다.
- Skin Settings 행을 안정적인 ID 순서로 구성해 선택적 LR2 해상도 행이 나타나도 키보드와 마우스 대상이 어긋나지 않게 했습니다.
- Skin Settings의 긴 기술 설명을 제작·가져오기·폴더 열기·F5 리로드 중심의 짧은 안내로 줄여 설정 목록 공간을 넓혔습니다.
- NK3 기본 `AUTO` 백엔드는 ncnn Vulkan을 우선 사용하고, `TENRIFF_NK3_BACKEND`와 `TENRIFF_NK3_VULKAN_DEVICE`로 런타임과 GPU 인덱스를 선택합니다. OpenVINO는 선택형 호환 폴백으로 유지합니다.
- MSVC 빌드에서 C++ 예외 처리를 명시하고 ncnn 의존성을 내부 링크로 한정해 릴리즈 빌드 경고를 제거했습니다.
- 게임플레이 판정 결정성 벤치마크를 CTest에 포함하고, 릴리즈 패키징은 등록된 테스트가 모두 통과한 뒤에만 진행합니다.

### Release

- 정식 자산은 `TenRiff-1.4.5.3.zip`, `TenRiff-1.4.5.3-source.zip`, `TenRiff-1.4.5.3-SHA256SUMS.txt`입니다. 기존 `1.4.5.2` 태그와 자산은 변경하지 않습니다.

## [1.4.5.2] - 2026-08-23

### Fixed

- 인게임 곡 진행 바를 고정된 화면 상단 전체 폭에 겹쳐 그리지 않고, 현재 1P/고스트 노트필드와 필드 이동 핸들 밖에서 가장 넓은 안전 여백을 골라 표시합니다. 여백이 좁으면 시간/퍼센트 문구를 단계적으로 줄여 노트를 가리지 않습니다.
- BPM, `#SCROLL`, STOP이 포함된 변속 구간의 HUD 노트 수집을 최대 6초 시간 창으로 자르지 않고 실제 시각 이동량 한 화면분까지 추적합니다. 저속 구간에서도 노트가 같은 화면 상단 진입점부터 보이며 변속 속도 자체는 평준화하지 않습니다.
- 기존 `Skin Settings > FAST/SLOW Display`를 `FAST/SLOW Indicator`로 명확히 표시하고, 끄면 판정 등급은 유지한 채 실시간 FAST/SLOW 문구와 타이밍 기록 인디케이터를 모두 숨깁니다.
- `No LN Release`가 charge LN의 떼는 판정을 끄면서 점수 배율을 깎던 값을 `0.90x`에서 정확히 `1.00x`로 바꿨습니다.
- LR2식 앞공POOR는 유지하되 이미 소비되거나 지나간 노트 뒤 입력에는 뒷공POOR가 추가되지 않는 판정 경계를 회귀 테스트로 고정했습니다.

### Release

- 정식 자산은 `TenRiff-1.4.5.2.zip`, `TenRiff-1.4.5.2-source.zip`, `TenRiff-1.4.5.2-SHA256SUMS.txt`입니다. 기존 `1.4.5.1` 태그와 자산은 변경하지 않습니다.

## [1.4.5.1] - 2026-08-23

### Fixed

- 플레이 종료 직전에 저장된 과거 리플레이 전체를 동기식으로 다시 검증하며 NK3 P64/MLP를 반복 실행하던 문제를 수정했습니다. 현재 세션 결과만 증분 캐시에 반영하고 전체 리플레이 재구성은 프로필 로드 시점에만 수행해 결과 화면 진입 중 NPU/GPU 재사용과 긴 대기를 제거합니다.
- 기본 결과 전환 대기 시간을 3초에서 0.5초로 줄이고, 기존 3초 기본값을 사용 중인 프로필만 0.5초로 마이그레이션하며 사용자 지정 값은 유지합니다.
- 일반화 MLP 실행 범위를 `source != 10K && target == 10K`로 제한했습니다. 10→10 리마스터와 10K 이외의 모든 목표는 P64 ONNX + host beam 안전 솔버만 사용합니다.

### Release

- 정식 자산은 `TenRiff-1.4.5.1.zip`, `TenRiff-1.4.5.1-source.zip`, `TenRiff-1.4.5.1-SHA256SUMS.txt`입니다. 기존 `1.4.5` 태그와 자산은 변경하지 않습니다.

## [1.4.5] - 2026-08-19

### Added

- NK3에 lane-shared schema-v3 일반화 패턴 MLP를 결합하고 2K부터 18K까지 목표 키 수별 고정 ONNX로 배포합니다. v3는 원본 chord 크기 비율을 입력으로 사용하고 addition-role 점수를 target lane 그룹 안에서 중심화합니다.
- 패턴 MLP는 28개 관계 feature와 8개 후보 role을 batch32로 평가하며, 실제 `EXECUTION_DEVICES`를 확인해 NPU, GPU, CPU 순서로 시도합니다. 1K 목표는 모델 schema 경계 때문에 기존 P64만 사용합니다.
- 1K–18K source와 1K–18K target의 324개 NK3 경로를 변환 완료와 구조 안전성 기준으로 회귀 검사합니다.

### Changed

- MLP 점수는 최대 1.0, host weight 0.15의 제한된 후보 정렬 residual로만 사용합니다. P64 validity와 host beam/safety retry/final quality 검사가 충돌, 롱노트 겹침, 최소 간격, 불가능 chord, 새 연타 방지의 최종 권한을 유지합니다.
- 같은 millisecond로 반올림되지만 서로 다른 sample인 노트를 충돌로 오판하지 않도록 최종 검사를 exact source sample 기준으로 수정했습니다.

### Limitations

- 교체된 일반화 MLP는 10K 학습 목표를 선언하지만 제공 NPZ에는 데이터셋 manifest나 학습 metric이 포함되지 않았습니다. 324개 자동 테스트는 통합과 구조 안전을 검증하지만 모든 키 수의 음악적 패턴 품질을 보증하지 않으므로 실제 플레이 검증은 별도입니다.

### Release

- 정식 자산은 `TenRiff-1.4.5.zip`, `TenRiff-1.4.5-source.zip`, `TenRiff-1.4.5-SHA256SUMS.txt`입니다. Windows ZIP은 TenRiff 본체, NK3 P64/일반화 MLP 모델, OpenVINO 2026.2.1 GPU/CPU runtime, 사용 가능한 NPU runtime 구성요소, 원본 라이선스/서드파티 고지를 포함하며 standalone BMS key converter 실행 파일은 포함하지 않습니다.

## [1.4.4] - 2026-08-16

### Added

- Mode Settings와 standalone BMS 변환기에 `NK3`를 추가했습니다. NK3는 64-slice P64 ONNX 결정 그래프와 host beam32 안전 솔버를 결합하며, 같은 키 수를 선택해도 리마스터를 실행합니다.
- OpenVINO 실행 장치는 기본적으로 엄격한 `GPU`를 사용합니다. `TENRIFF_NK3_DEVICE=CPU`로 CPU를 명시할 수 있으며 NPU와 자동 장치 폴백은 지원하지 않습니다.
- 주변 1초 구간의 롱노트 비율과 길이를 참고해 NK3 보조 노트를 롱노트로 생성하고, 새 보조 노트는 같은 손 영역에서 연타가 생기지 않는 레인으로 이동하거나 안전한 레인이 없으면 버립니다.
- 공개 소스와 Windows 패키지에 학습 데이터나 체크포인트를 사용하지 않는 59KB 결정론적 `NK3-P64-hybrid.onnx`를 포함합니다.

### Fixed

- 고밀도 롱노트와 키 수 축소에서 host beam 전이가 사라지던 경우를 additions 제거/beam256 재시도, 축소 전용 안전 드롭, 확장·동일 키 수의 deterministic base fallback으로 복구합니다.
- 변환 뒤 동일시각 충돌, 롱노트 겹침, 최소 간격 위반, 불가능한 chord와 원본에 없던 cross-source 연타를 최종 검사해 안전하지 않은 결과를 게임에 적용하지 않습니다.
- OpenVINO 모델을 변환마다 다시 컴파일하던 문제를 process-local evaluator 재사용으로 수정해 연속 변환의 장치 세션을 안정화했습니다.

### Release

- 정식 자산은 `TenRiff-1.4.4.zip`, `TenRiff-1.4.4-source.zip`, `TenRiff-1.4.4-SHA256SUMS.txt`입니다. Windows ZIP은 NK3 모델, OpenVINO 2026.2.1 GPU/CPU runtime과 원본 라이선스/서드파티 고지를 포함하며 NPU 플러그인은 번들하지 않습니다.

## [1.4.3-test] - 2026-08-15

이 빌드는 사운드 오프셋, 단일 색상 스킨, replay evidence v3와 로컬 스코어
재검증을 먼저 검증하기 위한 GitHub 테스트 프리릴리스입니다.

### Added

- Audio Settings와 Calibration Wizard에 `-500..+500ms` 사운드 오프셋을 추가했습니다. 차트 BGM과 자동재생 키음만 1ms 단위로 앞뒤 이동하며 판정, 노트/BGA 타이밍, 입력 연동 키음은 그대로 유지합니다.
- Skin Settings에 `Single Color`를 추가했습니다. `Off` 또는 팔레트 색상을 고르면 모든 키 모드와 스크래치 레인을 한 색으로 표시하며, `Off`로 돌아갈 때 기존 레인별 색상 구성을 복원합니다.
- 새 replay/result에 evidence format v3를 추가했습니다. 차트 SHA-256, 고정 ruleset ID, replay 파일 SHA-256을 결합하고 headless 게임 엔진으로 입력 trace를 재실행해 점수·판정·게이지·clear를 다시 계산합니다. 저장된 `final_score`나 판정 수를 편집해도 공식 로컬 베스트에는 재계산 결과만 사용합니다.
- 기존 replay, custom judge/gauge ruleset, Autoplay, Practice, 노트 수 변경 기록은 삭제하지 않고 Records에 `unverified` 상태로 보존하되 공식 베스트에서 제외합니다. v3 trace는 lane/sample/state 순서와 크기 상한을 엄격히 검사합니다.
- 직접 P2P 점수 claim에 native 점수·판정 수·combo·gauge 범위 검사를 추가하고 Result에서 `UNVERIFIED CLAIM`으로 표시합니다. 이는 비정상 패킷 방어이며 replay proof나 서버 권위 안티치트는 아닙니다.

## [1.4.2] - 2026-08-12

### Fixed

- BMS `#BPM`, 채널 `03`, `#BPMxx`/채널 `08` 변속이 노트 스크롤 속도에도 반영됩니다. 시작 BPM 기준 Hi-Speed와 Rate 시각 중립성은 유지하면서 `#SCROLL`, 정지, 역주행과 함께 실제 타임라인 속도로 렌더링합니다.

## [1.4.1] - 2026-08-11

### Added

- 같은 LAN의 TenRiff 호스트를 `27301/UDP`로 자동 검색하고, `LAN 방 자동 검색`에서 IP를 직접 입력하지 않고 기존 TCP 방에 참가할 수 있게 했습니다. 직접 IP 참가 경로는 유지되며 인터넷 중계나 NAT traversal은 포함하지 않습니다.
- Mode Settings에 `Pacemaker`를 추가했습니다. Accuracy 또는 최종 표시 Score 목표를 고르면 게이지로 조기 종료하지 않고 차트 끝에서 목표 달성 여부를 CLEAR/FAILED로 판정합니다.
- BMS `#RANDOM/#IF/#ELSEIF/#ELSE`와 `#SWITCH/#CASE/#SKIP/#DEF` 조건 분기를 고정 seed로 해석하고, MGQ 형식의 `#LNTYPE 2` 롱노트를 measure 경계까지 이어서 지원합니다.
- `CMakePresets.json`의 MSVC AddressSanitizer 구성과 같은 프리셋을 실행하는 GitHub Actions CI를 추가했습니다. 결정적 단위 코어는 공유 포트 간섭 없이 ASan으로 실행하고, MSVC ASan에서 종료가 멎는 Windows RawInput/localhost 통합 8건은 Release 테스트에서 계속 검증합니다.

### Changed

- 차트 시작 시 전체 곡의 BGM을 모두 디코드하지 않고 첫 3초에 필요한 오디오만 필수 준비합니다. 뒤쪽 BGM과 키음은 재생 전에 비동기 prefetch하여 긴 차트의 시작 대기 시간을 줄였습니다.
- Pacemaker는 Practice·Sudden Death와 상호 배타적이며 리플레이 재생과 공정 규칙이 적용되는 멀티플레이에서는 자동으로 꺼집니다.
- Song Select 미리듣기 decode future, 취소 토큰, gain, 활성 경로의 소유권을 `MenuApp`에서 `SongSelectScreen`으로 분리했습니다. 화면 이탈 시 decode를 취소하고 완료 future를 반드시 회수합니다.

### Fixed

- BMS 명령과 `#bpm/#BPMxx` 사전 키의 대소문자를 구분하지 않고, 채널 `03` 직접 BPM과 채널 `08` 확장 BPM 변속을 끝까지 sample timeline에 반영하는 회귀 테스트를 추가했습니다.

## [1.4.0.1] - 2026-08-10

### Added

- 첫 곡 폴더를 읽는 동안 게임플레이 밖 모든 화면 상단에 인덱싱 진행 바를 표시합니다.
- Song Select 빠른 설정의 비주얼 레이턴시, 스크롤 속도, Gauge Shift 시작 등급, Random을 키보드로 탐색하고 조정할 수 있습니다.
- 옵션 수치 항목에서 방향키를 누르고 있으면 첫 지연 뒤 연속 입력되며, 키를 떼는 즉시 멈춥니다.
- Skin Settings에 FAST/SLOW 타이밍 표시를 끄는 옵션을 추가했습니다.

### Changed

- 마스터 볼륨을 최종 리미터 뒤에 적용해 0~100% 전 구간에서 실제 출력이 선형으로 줄거나 커지게 했습니다.
- Background Sound는 메뉴·결과·곡 미리듣기만 제어하며, 인게임 BMS 배경음에는 더 이상 영향을 주지 않습니다.
- 비주얼 레이턴시 조정 단위를 5ms에서 1ms로 변경했습니다.
- 주사율 선택을 현재 디스플레이에 맞춤과 무제한 두 가지로 단순화하고 기존 고정 Hz 값은 맞춤으로 마이그레이션합니다.
- Skin Settings의 Key Mode를 첫 항목으로 이동했습니다.
- 곡 미리듣기를 AudioThread PCM 재생으로 전환했습니다. 명시된 미리듣기 파일을 우선 사용하고, 없으면 BMS BGM·키음을 최대 30초 구간으로 합성합니다.

### Fixed

- 스킨 미리보기의 롱노트를 인게임과 같은 헤드·바디·테일 자산, 종횡비, 필드 클리핑 규칙으로 렌더링해 잘리거나 뒤집히지 않게 했습니다.
- FAST/SLOW 표시를 꺼도 P-GREAT/GREAT 등 판정 등급 문구는 유지되도록 분리했습니다.
- 곡 미리듣기 오디오 장치 초기화가 실패하면 Song Select 메뉴 음악을 즉시 복원합니다.
- 최신 MSVC에서도 선택형 WinML ONNX 업스케일러 타깃을 기본 설정으로 빌드할 수 있게 호환 정의를 추가했습니다.

## [1.4.0] - 2026-08-10

### Added

- 최소 곡 인덱싱에서도 노트가 존재하는 1초 구간을 기준으로 NPS 최소·중앙·최대값을 계산하고 캐시에 저장합니다. 동시치는 개별 노트로 세며 롱노트 꼬리는 제외합니다.

### Changed

- 일반 점수 만점을 `10,000`으로 바꾸고 판정 배점을 `P-GREAT 6 / GREAT 3 / GOOD 1 / POOR 0 / FAIL 0` 비율로 재구성했습니다. 상세 점수는 기존 계산을 유지합니다.
- 단위인정/Session Mix 혼종 게이지의 회복량을 EX-HARD 기준에서 NORMAL과 HARD 회복량의 중간값으로 완화했습니다. EASY 기준 감소량과 스테이지 간 게이지 이월은 유지합니다.
- 이전 100,000점 체계로 저장된 리플레이와 결과는 불러올 때 10,000점 체계로 비례 환산합니다.
- Song Select의 배속 빠른 설정을 비주얼 레이턴시로 교체하고, 게이지 선택을 항상 켜지는 Gauge Shift의 시작 등급(`Easy / Normal / Hard / EX`) 선택으로 바꿨습니다. 기존 `shift` 저장값은 EX 시작으로 호환됩니다.
- EX-Hard의 플레이어 표기를 `EX`로 줄였습니다. 내부 설정 토큰 `ex_hard`와 과거 리플레이 호환은 유지합니다.
- 롱노트 몸통은 실제 렌더된 헤드 바로 위에서 시작해 테일 시작 경계에서 끝나며, 기본 너비와 밝기를 노트와 같은 100%로 맞췄습니다. 기존 60%/55% 기본값은 자동 마이그레이션됩니다.
- Song Select 기본 로비를 라이브러리·앨범/기록·시작/옵션의 38:35:27 비율로 재배치하고, 키 수/레벨과 레이아웃을 분리해 표시하며 BPM 행의 차트 칸을 중앙 NPS로 교체했습니다.
- 중앙 앨범아트와 최고 기록 블록을 1:1 높이로 맞추고, 기록 제목·점수·정확도·콤보·상세값을 더 크고 굵은 전용 글꼴로 표시합니다.
- `TENRIFF` 배너와 중앙 로비 탭 글자를 확대하고 배너에 액센트 그림자와 밑줄 효과를 추가했습니다.
- Gauge Shift 시작 등급 텍스트는 EX 빨강, Hard 주황, Normal 노랑, Easy 초록의 고대비 색상과 그림자를 사용합니다.
- 메뉴와 Song Select의 하이스피드 조정 단위를 `0.01`로 세분화했습니다. 인게임 조정 단위 `0.25`와 `10.0`은 유지합니다.

## [1.3.10] - 2026-08-10

### Fixed

- LR2 playskin에서 `alpha=0`으로 화면 밖에 숨겨 둔 더미 `#DST_NOTE`를 레인과 Gear 폭 계산에서 제외해, Gear 프레임이 한쪽에 좁게 축소되던 문제를 수정했습니다.

### Changed

- LR2 playskin 파일 선택은 5K에서 5키, 8K에서 7키, 10K에서 10키 파일을 우선하며, 해당 파일이 없을 때만 가장 가까운 레이아웃으로 자동 대체합니다.

### Added

- Added an LR2-style course draft: press `C` on Song Select to append stages in order, undo with `Delete`, play the draft in Session Mix, or save it as `.lr2crs`.
- Dan and Session Mix runs now share a continuous course gauge across stages, using EX-HARD recovery and EASY damage values and failing immediately at zero.

- 세션 믹스에서 LR2 `.lr2crs` 파일을 찾거나 드롭해 코스를 선택할 수 있습니다. 코스의 MD5 스테이지 순서와 반복 곡을 그대로 유지하고, 현재 곡 소스에 누락 채보가 있으면 시작 전에 정확한 누락 수를 표시합니다.

- Song Select에 `Session Mix`를 추가했습니다. 현재 검색·필터 결과에서 중복 없이 15/30/60분 분량의 워밍업·도전·마무리 큐를 만들고, 로컬 최고 기록으로 도전 난이도를 조절합니다. Result의 계속 버튼으로 다음 곡을 진행하며 Esc 또는 Backspace로 세션을 종료할 수 있습니다.

## [1.3.9] - 2026-08-08

### Added

- Skin Settings에 키 입력 폭발 효과 `Prism`, `Ring`, `Spark` 3종을 추가했습니다. 긴 노트를 누르는 동안에는 단노트와 다른 지속형 폭발 효과가 표시됩니다.
- Space로 열 수 있는 4x2 배치의 8개 정사각형 옵션 카드 화면을 추가했습니다. 방향키로 이동하고 Enter로 열며 Esc 또는 Backspace로 돌아갑니다.
- Result와 곡 선택 로비에 기본 만점 `100,000`과 실제 노트 수를 기준으로 한 상세 점수 만점을 함께 표시합니다.

### Changed

- 곡 선택 로비의 키 모드를 상단에 크게 배치했고 최고 기록 랭크가 잘리지 않도록 전용 크기로 중앙 배치했습니다.
- Result 판정 순서를 `P-GREAT`, `GREAT`, `GOOD`, `POOR`, `FAIL`로 정리했고 기존 `BAD`를 `FAIL`로 표시합니다. 한국어 Result 제목과 상세 표현도 확장했습니다.
- Result의 FAST/SLOW 개수에서 최상위 판정인 P-GREAT(20ms)를 제외했습니다.
- 롱노트 바디 기본 불투명도를 높여 연하게 보이지 않도록 했습니다.

### Fixed

- 동영상 BGA가 같은 경로에서 계속 요청될 때 완료된 프레임이 게시되지 않거나, Media Foundation 열기 후 디코딩이 실패하면 영구적으로 멈추던 문제를 고쳤습니다. 재시도와 FFmpeg 대체 경로도 보강했습니다.
- 한글이 포함된 Windows 경로에서 LR2 스킨 가져오기와 해상도 자동 맞춤이 예외로 중단될 수 있던 경로 처리를 보강했습니다.

## [1.3.8] - 2026-08-08

### Added

- nK2 키 컨버터에 `Remaster (65%)` 프리셋을 추가했습니다. 보조 노트 예산을 65%까지 올리면서도 앵커를 최대로 잠가 원곡 배치를 그대로 남기고, 롱노트 구간에 들어가는 보조 노트를 같은 길이의 롱노트로 채웁니다. `Mode Settings > Key Converter`에서 `KeyWeaver nK2`를 고르면 `Native (12%)`·`Transform (35%)`와 함께 선택할 수 있습니다.
- osu!mania `.osu` 채보를 nK2 엔진으로 다른 키 수로 변환하는 `keyweaver_osu` 도구를 소스에 추가했습니다. 원본 파일은 건드리지 않고 같은 폴더에 난이도를 하나 더 만들며, `[HitObjects]` 외의 섹션은 바이트 단위로 보존합니다.

### Changed

- nK2가 원본 채보의 밀도에 맞춰 보조 노트 예산을 조절합니다. 한산한 채보는 예산을 비례해서 줄여 Easy가 Normal이 되어 버리는 일을 막고, 빽빽한 채보는 원래 안전창에 먼저 걸리므로 결과가 달라지지 않습니다.
- nK2가 원본 레인이 하나도 매핑되지 않는 빈 레인(7K→8K의 4번째, 4K→5K의 3번째)에 가산점을 줍니다. 보조 노트만 깔려 있던 죽은 레인이 사라지고 레인 분포가 고르게 나옵니다.

### Fixed

- nK2가 잭(같은 레인 연타)을 검사할 때, 자리를 못 찾아 뒤로 밀린 노트 때문에 그보다 앞선 노트를 건너뛰던 문제를 고쳤습니다.
- nK2 보조 노트가 같은 레인의 기존 롱노트와 구간이 겹치는데도 배치되던 문제를 고쳤습니다.

## [1.3.7] - 2026-08-07

### Added

- Graphics Settings의 주사율 항목에 osu!처럼 `무제한`을 추가했습니다. VSync가 꺼진 게임플레이에서는 렌더 프레임 대기를 제거하고, 메뉴는 기존 300 FPS 제한을 유지합니다.
- 일시정지(ESC) 화면에서도 판정선(F1/F2), 하이스피드(F3/F4, F5/F6), 비주얼 레이턴시(F7/F8)를 조절할 수 있습니다. 현재 값이 일시정지 창에 함께 표시됩니다.
- Result의 `TIMING SUMMARY` 그래프 좌우에 `FAST`/`SLOW` 라벨과 각 판정 수를 표시합니다.
- Result의 `GAUGE CONSISTENCY` 그래프에 0~100% 눈금선과 백분율 라벨을 추가했습니다.

### Changed

- 인게임 `FRAME PACING`을 HUD 업데이트 주기가 아니라 성공한 DXGI `Present()` 완료 프레임 간격으로 측정하도록 변경했습니다. 가려짐·모드 전환·실패한 present는 샘플에서 제외합니다.
- 타격 폭발 이펙트를 훨씬 밝게 바꿨습니다. 외곽 블룸·레인 색 빔·글로우·백색 코어 4겹으로 쌓아 판정선에서 확실히 보입니다.
- 기본 판정창을 `PGREAT 20ms / GREAT 65ms / GOOD 115ms`로 넓혔습니다. 기존 `45/90` 기본값을 쓰던 프로필은 자동으로 옮겨집니다.
- 기본 하이스피드를 `3.00`에서 `10.00`으로, 캘리브레이션 기본 조정 단위를 `5ms`에서 `1ms`로 바꿨습니다.
- 기본 `LN 테일 캡`을 끔으로, 기본 `인게임 커서`를 켬으로 바꿨습니다.
- 롱노트 보정을 완전히 제거했습니다. 꼬리 오차를 0으로 스냅하던 처리와 `hold_grace`/`hold_break` 전용 판정창을 없애고, 꼬리도 일반 노트와 같은 판정창을 씁니다.
- `FAST`/`SLOW` 밀리초 표시를 최고 판정(PGREAT)에서는 띄우지 않습니다.
- Result의 `PERFORMANCE ANALYSIS` 패널 왼쪽 끝을 그 아래 `CONTINUE`/`WATCH REPLAY`/`RETRY` 열과 맞췄습니다. 타이밍 그래프 막대 간격은 패널 폭에서 계산해 항상 안쪽에 들어옵니다.

### Fixed

- 한 화음 안에서 같은 키음이 여러 레인에 배정돼 있으면 소리가 겹쳐 커지던 문제를 고쳤습니다. 같은 차트 시점의 같은 키음은 한 번만 재생합니다.
- 한국어 UI에서 Random 모드 이름이 `??`로 깨져 나오던 문제를 고쳤습니다.

## [1.3.5] - 2026-08-06

### Added

- LR2 스킨의 기어(Gear) 그래픽을 가져옵니다. `Gear` 폴더 밖에서 선언한 패널도 인식하고, 애니메이션의 최종 위치·좌우 반전 표기·전체 이미지 지정(`-1`)을 처리하며, 타이머나 옵션으로 조건부 표시되는 풀콤보·셔터 연출은 제외합니다. 가져온 기어는 원본 스킨에서 레인과 판정선에 대해 놓였던 위치 그대로 배치됩니다.
- Skin Settings에 `노트 여백`을 추가했습니다. 노트 가장자리가 레인 구분선에서 몇 px 떨어질지 직접 지정합니다(0~40px, 기본 12px). 0으로 두면 노트가 구분선에 맞닿습니다.
- `이미지 비율`이 켜기/끄기에서 `늘이기 / 맞추기 / 너비 기준` 3단계가 됐습니다. `너비 기준`은 레인 너비를 그대로 쓰고 높이를 이미지 비율에서 뽑아, 화살표나 원형 노트 아트가 찌그러지지 않습니다.
- 기본 스킨 노트 모양에 `정사각형`, `마름모`, `화살표`를 추가했습니다(기존 사각형·원형·삼각형·오각형·육각형과 함께 8종).
- 곡 검색이 띄어쓰기로 나눈 여러 단어를 모두 만족하는 곡만 찾고, 제목·아티스트·경로에 더해 차트 이름·레이아웃·포맷·난이도표 이름/기호/레벨과 `7k`, `lv12` 같은 표기까지 훑습니다.
- 곡 선택 화면의 검색 버튼에 입력 중인 검색어와 커서가 그대로 보이고, 옆에 검색 결과 곡 수가 표시됩니다.
- 난이도표를 URL로 직접 입력할 수 있습니다. 난이도표 행에서 Enter를 누르면 입력란이 열리고(클립보드에 URL이 있으면 미리 채워집니다), 좌우 키는 각각 해제와 로컬 JSON 선택입니다.
- 플레이 중 판정선 위치(F1/F2, ±1%)와 비주얼 레이턴시(F7/F8, ±1ms)를 조절할 수 있습니다. 값은 HUD에 표시되고 곡이 끝나면 설정에 저장됩니다.
- Skin Settings에 `인게임 커서`를 추가했습니다. 켜면 플레이 중에도 마우스 커서가 보입니다.

### Fixed

- Windows에 Targa 코덱이 없어 `.tga`로만 구성된 클래식 LR2 스킨이 노트·기어를 하나도 표시하지 못하던 문제를 자체 디코더로 해결했습니다.
- LR2 해상도 자동 판별이 레인의 x 좌표만 보던 탓에, 레인이 화면 왼쪽에 몰린 1280x720 스킨을 640x480으로 오판해 노트가 2배 크게 들어오던 문제를 고쳤습니다. 이제 스킨이 그리는 배경 캔버스 크기를 함께 봅니다.

## [1.3.4] - 2026-08-06

### Added

- 게임플레이 필드 오른쪽 위에 `↔` 끌개를 추가했습니다. 플레이 중 마우스로 필드를 실시간 이동할 수 있으며 위치는 설정에 저장됩니다.

### Changed

- 기본 판정 폭을 `PERFECT 20ms / GREAT 45ms / GOOD 90ms`로 조정하고, 이전 기본값을 사용하던 설정은 새 기본값으로 마이그레이션합니다.
- BPM 변속 뒤에도 시작 BPM에 맞춘 기본 스크롤 속도를 유지합니다. 명시적인 `#SCROLL`, 정지 및 역주행 효과는 그대로 적용됩니다.
- `표시 오프셋`을 `비주얼 레이턴시`로 이름을 바꾸고 Graphics Settings에서 Skin Settings로 옮겼습니다. 기존 `offsets.visual` 저장값과 기능은 유지됩니다.
- Skin Settings 목록 폭을 화면의 절반으로 줄이고, 긴 목록을 키보드·마우스 휠·클릭 가능한 스크롤바로 탐색할 수 있게 했습니다.
- 곡 라이브러리 스크롤바를 파스텔톤 흰색으로 조정했습니다.

### Fixed

- Graphics Settings에서 Enter나 마우스 클릭이 간헐적으로 선택 항목에 전달되지 않던 입력 경로를 수정했습니다.

### Performance

- BMP 등 이미지 레이어 BGA를 렌더 스레드 밖에서 비동기로 디코딩하고, 최신 요청 우선 처리와 제한형 LRU 캐시를 적용해 인게임 프레임 끊김과 반복 디코딩을 줄였습니다.

## [1.3.3] - 2026-08-06

### Added

- `Options > Skins`에 `UI Font / UI 폰트` 행을 추가했습니다. `Segoe UI`, `Malgun Gothic`, `Bahnschrift`, `Consolas` 중에서 고르며 즉시 반영됩니다. 로고·랭크·콤보 숫자와 디버그 readout은 레이아웃이 해당 글꼴 폭에 맞춰져 있어 그대로 둡니다.
- 리절트 화면에서 `R` 키로 재시도할 수 있습니다. 기존 `LEFT` 키도 그대로 동작합니다.

### Changed

- 리절트 화면의 `CONTINUE` 화살표를 이모지 화살표로 바꾸고, `WATCH REPLAY`와 `RETRY` 버튼에 아이콘·세미볼드 글꼴·강조 테두리를 넣고 크기를 키워 잘 보이게 했습니다.
- 클릭할 수 있는 요소를 읽기 전용 패널과 구분했습니다. 곡 선택의 배속·하이스피드·게이지·랜덤 셀, 검색·정렬/필터·뒤로 버튼, 리절트의 리플레이·재시도 버튼이 강조 테두리를 갖습니다.
- 곡 선택 상단 탭(`곡 목록`/`소스`/`기록`/`옵션`)과 배속·하이스피드·게이지·랜덤의 라벨과 값을 굵은 글꼴로 키웠습니다.
- 곡 선택의 자켓 이미지를 감싸는 패널의 둥근 모서리에 맞춰 잘라 그립니다. 이전에는 사각형 이미지가 패널 모서리 밖으로 튀어나왔습니다.
- 곡 선택 `내 최고 기록`의 점수·정확도·최대 콤보를 폭이 같은 3단 컬럼으로 나누고 상세 값을 아래 줄로 내렸습니다. 이전에는 `점수 / D 상세` 문자열이 컬럼 폭을 넘겨 옆 항목과 겹쳐 읽기 어려웠습니다.
- 리절트 판정 표의 `P GREAT` 표기를 `P-GREAT`으로 바꿨습니다.

## [1.3.2] - 2026-08-06

### Added

- TenRiff 스킨에 `layout` 섹션을 추가했습니다. 타이틀 화면(`spectrum`, `logo`, `buttons`, `guide`, `footer`)과 Song Select(`top_bar`, `logo`, `nav`, `profile`, `left_panel`, `center_panel`, `right_panel`, `bottom_bar`) 13개 슬롯의 위치를 `[left, top, right, bottom]`(1920x1080 기준 좌표)으로 옮길 수 있습니다. 패널 안의 내용과 클릭 판정 영역이 함께 따라가고, 적지 않은 슬롯은 기존 위치를 그대로 씁니다.
- 화살표 스킨 지원을 추가했습니다. `gameplay.note_aspect`(`stretch`/`contain`/`width`)로 노트 이미지가 레인 사각형을 채우는 방식을 고르고, `gameplay.note_rotations`와 `gameplay.key_rotations`로 레인별 회전 각도(도, 시계 방향)를 지정합니다. 위를 향한 화살표 이미지 하나로 모든 레인을 처리할 수 있어 레인마다 미리 돌려 둔 이미지를 만들지 않아도 됩니다.
- osu!에서 넘어오는 사람들을 위한 로비 전용 예제 스킨 `Tencircle`을 추가했습니다. 삼각형 배경, 히트서클 링, 히트서클 워드마크로 구성했고 `layout`으로 곡 목록을 화면 오른쪽에 배치합니다.
- 노트를 칠 때 판정선에서 터지는 폭발 이펙트의 밝기를 `0~100%`로 조절할 수 있게 했습니다. `Options > Skins`의 기존 `Key Pulse` on/off 행이 `Hit Burst / 폭발 이펙트` 백분율 행으로 바뀌었고 5% 단위로 움직입니다. `0%`가 곧 끄기이며, 기존 config의 `key_pulse_enabled`는 그대로 읽어 켜짐은 `100%`, 꺼짐은 `0%`로 옮깁니다.

### Fixed

- 정사각형에 가까운 화살표 노트 이미지가 노트 사각형(레인 폭 × 노트 높이)에 그대로 늘어나 세로로 눌려 보이던 문제를 수정했습니다. 스킨에서 `note_aspect: "width"`를 지정하면 폭은 레인에 맞추고 높이를 이미지 비율에서 계산합니다. 스킨이 `note_aspect`를 적으면 `Options > Skins > Image Aspect` 토글보다 우선합니다.

## [1.3.1] - 2026-08-05

### Added

- Added BMS `#SCROLLxx` / `#xxxSC` playback visuals, including faster, stopped, and reverse scroll segments without changing judgement or audio timing.
- Added BMS landmine channels `D1-D9` / `E1-E9`, `#WAV00` explosion keysounds, percentage gauge damage, and `ZZ` instant-fail mines. Lane randomization and standalone BMS key conversion preserve mines.
- Added a separate detail score (`PG=5`, `GREAT=3`, `GOOD=1`, `BAD/POOR=0`) and detailed accuracy. Native accuracy remains judgement-grade based, while detailed accuracy averages continuous timing precision so steadier hits rank higher.

### Changed

- Song Select 우측의 배속·하이스피드·게이지·랜덤 셀을 직접 조작할 수 있게 변경했습니다. 좌클릭은 증가/다음, 우클릭은 감소/이전을 적용하고 즉시 저장합니다.
- 게이지는 `Easy → Normal → Hard → EX-Hard → Gauge Shift`, 랜덤은 `Off → Mirror → Random → R-Random → S-Random` 순서로 순환합니다.
- 외부 `.osu` 차트의 파서·인덱싱·로딩·Mode Settings 토글을 제거하고 지원 차트를 BMS 계열(`.bms/.bme/.bml/.pms`) 전용으로 정리했습니다. BMS 난이도와 기존 기록 호환에 쓰이는 내부 OD8/난이도 수학 모델은 유지합니다.

### Fixed

- `현재 차트` 키 수가 128px 랭크 셀용 글꼴로 그려져 패널 밖으로 잘리던 문제를 수정했습니다.
- 저장된 최고 기록이 있는 Song Select 카드에서 정확도가 누락되던 문제를 수정했습니다.
- Song Select 랜덤 모드 내부 값(`off`, `fr`, `rr`, `sr`)을 사용자용 이름으로 표시하도록 수정했습니다.
- Aery BMSTable의 URL·data JSON·CG901B MD5 매칭을 실서버로 검증하고, 난이도표 선택 시 해시가 없는 Fast 인덱스에서 Safe 인덱스로 자동 전환해 재스캔하도록 수정했습니다.

### Release

- Windows `TenRiff-1.3.1.zip`, public source `TenRiff-1.3.1-source.zip`, and `TenRiff-1.3.1-SHA256SUMS.txt` are the formal hotfix release assets; no ONNX model, private checkpoint, user profile, song, log, or local UI-audit artifact is bundled.


## [1.3.0 UI-r2] - 2026-08-04

### Added

- Result 화면을 2.2초 순차 연출로 재구성했습니다. 곡 이미지 전환, 중앙 프리즘 조립, 감속 점수 카운트업, 등급 충격파, 판정 통계 순차 표시, 타이밍·게이지 그래프, ALL PERFECT/FULL COMBO 효과와 Space 스킵 후 입력 잠금 해제를 포함합니다.

- Song Select 프로필 카드와 Profile Setup에 로컬 PNG/JPG 프로필 사진 선택·교체·삭제 기능을 추가했습니다.

### Changed

- Song Select를 상단 탭, 좌측 7곡 재킷 목록, 중앙 선택 이미지·최고 기록, 우측 차트/모드·START 중심의 3열 레이아웃으로 개편했습니다.
- Collection, Store, 재화, 글로벌 랭킹처럼 현재 구현되지 않은 참조 화면 요소는 추가하지 않았습니다.

### Fixed

- Autoplay 완료 결과를 `AUTOPLAY` 비경쟁 기록으로 저장하고 공식 클리어·최고 점수·클리어 램프에서 제외했습니다. 구형 `ASSIST AUTOPLAY ... CLEAR` 결과도 로드 시 같은 규칙으로 교정합니다.

### Release

- Windows `TenRiff-1.3.0-UI-r2.zip`, public source `TenRiff-1.3.0-UI-r2-source.zip`, and `TenRiff-1.3.0-UI-r2-SHA256SUMS.txt` are the UI-r2 release assets; no ONNX model, private checkpoint, user profile, song, log, or local UI-audit artifact is bundled.

## [1.3.0] - 2026-08-04

### Added

- 로비 배경·로고와 인게임 배경·기어·노트·LN·키 이미지를 하나의 `skin.json`으로 교체하는 TenRiff 스킨 포맷 v1, 안전한 프로필 가져오기, 예제·JSON Schema를 추가했습니다.
- Song Select 우측에 익숙한 `BEST SCORE` 카드를 배치하고, 카드 클릭에서 저장 Result를 거쳐 큰 `WATCH REPLAY` 버튼으로 이어지는 기록·리플레이 발견 경로를 추가했다.
- Song Select 좌측 탐색에 곡·소스·검색·필터·기록·설정을 구분하는 심볼 아이콘을 적용하고, Records 화면에도 명시적인 `OPEN RESULT` 동작을 추가했다.
- 카운트다운 화면에 인게임 Hi-Speed 단축키(`F3/F4`, `F5/F6`)를 표시하고, Graphics Settings에 밝은 BGA를 노트 영역 뒤에서만 차단하는 `BGA Behind Notes` 옵션을 추가했다.
- 일시정지 사용 여부를 Result/Replay JSON과 로컬 기록에 저장하고 Result 화면에 `Pause Used`로 표시한다. 이전 JSON은 `false`로 호환 로드한다.

### Changed

- 새 설정의 `Opaque Playfield` 기본값을 켜서 기어 뒤 BGA 방해를 줄였다. 기존 프로필에 명시된 값은 유지한다.
- osu!mania OD8 환산 값은 Sudden Death와 기존 리플레이 호환용 내부 통계로 유지하되 Gameplay/Result UI에서는 제거해 기록되지 않는 보조 점수의 노출을 정리했다.
- 최고 기록 카드와 Records 목록이 하나의 저장 Result 로딩 경로를 공유하도록 통합하고, Gameplay 재시작을 재귀 호출에서 반복 구조로 바꾸며 HUD revision 비교용 중복 상태 복사를 제거했다.

### Release

- Windows `TenRiff-1.3.0.zip`, public source `TenRiff-1.3.0-source.zip`, and `TenRiff-1.3.0-SHA256SUMS.txt` are the formal stable release assets; no ONNX model, private checkpoint, user profile, song, log, or local UI-audit artifact is bundled.

## [1.2.104] - 2026-08-03

### Added

- Mode Settings의 nK2 프리셋에 기본 `Native (12%)`와 `Transform (35%)` 선택을 추가하고 설정·리플레이에 저장한다. 이전 nK2 리플레이는 `Native`로 재현한다.

### Changed

- `Krrcream` 선택 시 nK2 프리셋 행을 잠그고, standalone converter GUI에서도 Krrcream의 Max/Min/Speed/Seed 튜닝 입력을 읽기 전용으로 고정한다.

### Release

- Windows `TenRiff-1.2.104.zip`, public source `TenRiff-1.2.104-source.zip`, and `TenRiff-1.2.104-SHA256SUMS.txt` are the formal hotfix release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.103] - 2026-08-03

### Changed

- Mode Settings의 별도 `변환 노트 추가` 옵션과 신규 저장 메타데이터를 제거했다. Krrcream은 원본 노트만 재배치하고, nK2는 키 수 확장 중 변환된 목표 레이아웃에 안전한 보조 노트를 직접 생성하며, 일반 Note Add Mod는 키 변환 뒤에 별도로 적용된다.

- Mode Settings의 `Indexing: Fast`를 최소 메타데이터 모드로 변경했다. 제목/아티스트/키 수/#PLAYLEVEL/BPM만 유지하고 파일 해시, 배경·오디오 미리보기 탐색, 난이도표 매칭, 자체 LV/CR을 생략하며 Safe/Fast 캐시를 분리한다.

- `Judge Hard`에서 입력 없이 지나간 노트를 BAD 대신 콤보 브레이크 간접 `POOR`이자 OD8 `MISS`로 기록한다.

### Release

- Windows `TenRiff-1.2.103.zip`, public source `TenRiff-1.2.103-source.zip`, and `TenRiff-1.2.103-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.102] - 2026-08-03

### Fixed

- 키모드 변환의 Note Add 순서를 `키컨버터 → 노트 추가`에서 `원본 패턴 노트 추가 → 키컨버터 → 게임플레이`로 수정해, 추가된 노트도 컨버터가 최종 레이아웃에 함께 배치하도록 변경

### Release

- Windows `TenRiff-1.2.102.zip`, public source `TenRiff-1.2.102-source.zip`, and `TenRiff-1.2.102-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.101] - 2026-08-03

### Added

- Mode Settings의 키 변환에 `변환 노트 추가: 기본 / 25% 이상 추가`를 추가; 실제 건반 수가 바뀔 때만 기본 변환 결과보다 안전한 무음 화음을 최소 25% 더 요청하고, 더 높은 Note Add Mod와 중복 적용하지 않으며, 리플레이/결과 메타데이터에 저장해 일반 최고기록에서 제외

### Changed

- 곡 인덱싱의 자체 LV/CR 계산을 기본 비활성화하고 Mode Settings의 `Index Difficulty`에서 선택적으로 켜도록 변경; 끈 상태에서는 BMS `#PLAYLEVEL`을 유지하며 계산 모드가 다른 캐시는 자동 재인덱싱

### Fixed

- 마스터 볼륨 0 상태로 재시작한 뒤 음량을 올려도 메뉴 음악이 복구되지 않던 Windows MCI 경로를 수정; 무음 상태에서는 재생 세션을 만들거나 유지하지 않고 음량이 다시 올라가면 새 세션으로 재생

### Release

- Windows `TenRiff-1.2.101.zip`, public source `TenRiff-1.2.101-source.zip`, and `TenRiff-1.2.101-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.100] - 2026-08-02

### Added

- LR2 스킨 가져오기가 표준 `LR2files`/`Theme` 루트를 인식해 정확한 `IIDX` 폴더와 IIDX 자산 의존 테마를 제외한 하위 테마를 개별 설치하고, 형제 테마 참조와 기존 설치본을 보존하는 안전한 이식 경로를 지원

- LR2 playskin의 `play/Gear` 정적 프레임을 원본 종횡비를 유지하는 단일 하단 오버레이로 이식해 실제 스킨의 키 기어를 gameplay에 렌더링
- native 기본 스킨 하단에 레인별 디지털 피아노 건반을 추가하고, 실제 키 홀드 동안의 눌림 깊이와 타격 순간 cyan/magenta 글리치 펄스를 분리해 렌더링
- protocol v5 멀티플레이 방에 최대 256바이트 UTF-8 메시지와 최근 32개 제한을 둔 로비 채팅창을 추가

### Changed

- EX-Hard 게이지를 Hard의 붉은색과 구분되는 짙은 흑회색 팔레트로 변경
- LR2 Gear를 원본 종횡비 그대로 기본 약 2배 확대하고 Note & Field Size에 따라 1.25~2.8배로 연동하되 판정선 아래 영역에서만 렌더링
- 멀티플레이 채팅에 최근 메시지 수, 본인/리더 표식, 연결 전·빈 메시지·연결 종료 상태 안내를 추가
- 멀티플레이 공정 규칙은 동일 BMS 바이트·Rate·게이지·판정·랜덤/어시스트를 계속 고정하되, 각 플레이어의 로컬 키모드 변환은 허용

### Fixed

- LR2 `#INCLUDE`가 `#CUSTOMFILE` 와일드카드 기본 선택을 적용하도록 수정해 FT 스킨의 실제 Note CSV가 빠지고 `Numbers.png`가 낙하 노트로 사용되던 문제를 해결
- LR2 Gear를 레인별로 늘려 로고와 패널이 찌그러지던 문제를 수정하고, Gear가 없는 스킨은 낙하 노트/LN 머리를 receptor 대용으로 쓰지 않도록 변경
- LR2 눌림 이미지는 실제 홀드 상태가 아니라 짧은 타격 펄스에만 반응하게 해 롱노트 유지 중 판정선에 노트 효과가 계속 남던 문제를 수정
- gameplay 입력도 foreground process gate를 사용하도록 수정해 로딩/플레이 중 다른 창으로 전환했을 때 background의 lane 및 `Esc` 입력을 무시

### Packaging

- Windows TenRiff-1.2.100.zip, public source TenRiff-1.2.100-source.zip, and TenRiff-1.2.100-SHA256SUMS.txt are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.99] - 2026-08-02

### Fixed

- nK2로 키 수를 줄일 때 소스 노트 타이밍을 보존해 실시간 판정과 표시 박자가 어긋나 MISS가 발생하던 문제를 수정

### Changed

- LN Mix의 8·16·24·32비트 길이는 유지하고, 선택된 롱노트의 길이 비율을 긴 60% / 중간 20% / 짧은 20%로 조정

### Packaging

- Windows `TenRiff-1.2.99.zip`, public source `TenRiff-1.2.99-source.zip`, and `TenRiff-1.2.99-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.98] - 2026-08-01

### Added

- Direct-IP multiplayer protocol v4 uses one fixed TCP room coordinator and supports up to eight total players; chart/start leadership rotates in join order after every completed round and skips disconnected leaders
- Multiplayer song inventory is BMS-only and exposes only the exact SHA-256 intersection owned by every connected player; optional osu!mania charts remain available in single-player but are excluded from multiplayer
- Room-wide Ready, chart-load, final-result, and Result-exit barriers coordinate all connected players; full rooms and mid-round joins are rejected
- Multiplayer lobby state and Result standings identify up to eight participants, the local player, and the current rotating leader

### Packaging

- Windows `TenRiff-1.2.98.zip`, public source `TenRiff-1.2.98-source.zip`, and `TenRiff-1.2.98-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled
## [1.2.96] - 2026-08-01

### Added

- `Note & Field Size` now scales the centered playfield, lane dividers, notes, and adjacent gauges together from 50% to 140%
- Multiplayer protocol v3 exchanges bounded SHA-256 inventory chunks and limits the host song list to charts both players own, without sending chart paths or file contents
- Judgement feedback uses a 220ms pop animation and combo numbers use a 150ms pop animation, affecting rendering only

### Packaging

- Windows `TenRiff-1.2.96.zip`, public source `TenRiff-1.2.96-source.zip`, and `TenRiff-1.2.96-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.95] - 2026-08-01

### Added

- Gameplay judgement feedback now shows signed timing detail: early hits use `FAST -12 ms`, late hits use `SLOW +18 ms`, and hits rounded to `0 ms` omit the timing label
- The default `BAD` window is now `210ms`; Judge Easy follows its existing `1.25x` scale (`262.5ms`), while Judge Hard uses an exact `340ms` BAD window without changing PG/GR/GD or long-note tail windows
- Native note spacing now keeps lane-divider geometry fixed, widens the default note-edge gap from 16px to 24px, and applies the per-mode `Note Size (Width)` scale only inside each lane
- Skins now includes a persisted `Black Playfield` toggle that fills the complete player/ghost lane field, including configured spacing gaps, with solid black
- Restored TenRiff's in-tree osu!mania parser as an optional `OSU Charts` Mode Settings path for raw 4K-10K `.osu` files, including indexing, native difficulty, main audio, background image, hold-note runtime conversion, and cache invalidation; BMS remains the default and `.osz`/osu skin import are not restored

### Packaging

- Windows `TenRiff-1.2.95.zip`, public source `TenRiff-1.2.95-source.zip`, and `TenRiff-1.2.95-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.93] - 2026-08-01

### Added

- In-game Mode Settings now exposes `Key Converter` with `Krrcream` and embedded `KeyWeaver nK2` choices; the selection is saved in config/replay metadata and applied to runtime key-mode conversion

### Packaging

- Windows `TenRiff-1.2.93.zip`, public source `TenRiff-1.2.93-source.zip`, and `TenRiff-1.2.93-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.92] - 2026-08-01

### Added

- The standalone BMS key-mode converter can now select either the existing Krrcream path or the deterministic `nK2 Native 50/50` algorithm from both the CLI and Win32 GUI
- Added a self-contained TenRiff adapter and minimal chart model for nK2, covering tap notes, long notes, generated support notes, source metadata recovery, and output duration updates
- Added direct nK2 selection, determinism, invalid-algorithm rejection, and BMS write/reparse regression coverage

### Changed

- Krrcream remains the default; nK2 deliberately ignores the Krrcream-only Max/Min/Speed/Seed controls, which are disabled in the GUI while nK2 is selected
- Generated or shifted nK2 notes are quantized from their converted sample positions when writing BMS output, including long-note tails
- The nK2 build is self-contained and does not depend on another key-converter source tree

### Packaging

- Windows `TenRiff-1.2.92.zip`, public source `TenRiff-1.2.92-source.zip`, and `TenRiff-1.2.92-SHA256SUMS.txt` are the formal release assets; no ONNX model, private checkpoint, user profile, song, or log is bundled

## [1.2.9] - 2026-07-31

### Added

- BMS key-mode conversion now supports `12K` and `14K`; scratch layouts such as `7+1 SP` convert only the seven keyboard lanes into the target key count while keeping scratch out of the lane remap, and DP layouts process both keyboard halves independently
- Added `R-Random`, `DP Flip`, and deterministic `Note Add 10%..100%`; Note Add creates silent chord notes, avoids scratch/LN bodies/duplicates/excessive chord sizes, and its results do not overwrite normal best records
- Song Select starts a chart preview after the selection stays stable for 750 ms, using explicit BMS preview audio first and safe chart-audio fallbacks otherwise
- Result now shows the song image plus chart/key/BPM/LV/CR/difficulty-table metadata
- Quick Setup now edits a UTF-8-safe profile nickname that is saved in the profile and exported to results and multiplayer identity

### Fixed

- Video BGA no longer enters asynchronous real-time ONNX upscaling, preventing frame-to-frame geometry changes that appeared as screen shaking; static image BGA can still use the external upscaler
- Replaced the legacy NPU-success wording with accurate `DirectXMinPower` wording: this path is a low-power DirectX request, not an explicit or verified NPU selection

### Changed

- Expanded per-mode keymaps and skin lane palettes through `14K`, including migration-safe defaults for existing profiles
- Improved existing long-note handling alongside the new lane conversion and Note Add rules

### Packaging

- Windows `TenRiff-1.2.9.zip`, public source `TenRiff-1.2.9-source.zip`, and `TenRiff-1.2.9-SHA256SUMS.txt` are the formal release assets; no ONNX model or private checkpoint is bundled

## [1.2.8] - 2026-07-30

### Added

- `Gauge Shift`를 EX-Hard / Hard / Normal / Easy 병렬 연산 방식으로 변경: 네 게이지를 100%에서 동시에 계산하고 탈락하지 않은 가장 높은 게이지를 실시간 표시·최종 확정하며 멀티플레이도 같은 규칙을 사용
- `Mainmusic/`의 Title/Quick Setup/Options/Song Select/Multiplayer/Result 성공·실패 슬롯을 연결하고, `이름.mp3`와 `이름 2.mp3`~`이름 64.mp3`를 자동 수집해 화면 재진입마다 순환하며 누락 슬롯은 기존 곡으로 폴백
- Graphics Settings에 게임플레이 이미지/영상 BGA와 관련 디코더·업스케일러 작업을 함께 끄는 `BGA` 토글 추가; Song Select 미리보기는 유지
- Browse의 Difficulty Table에서 클립보드에 복사한 BMSTable HTML 페이지 또는 header JSON 링크를 profile-local cache로 가져오는 기능 추가

### Fixed

- 롱노트 머리에서 active hold로 넘어가는 프레임에 synthetic hold가 생략되어 롱노트 전체가 한 프레임 깜빡이던 문제 수정

### Changed

- 네이티브 점수를 판정 90,000점 + 누적 콤보 10,000점의 최대 100,000점 구조로 정규화하고, LN 머리/꼬리 각각 0.5 가중치, 판정 구간 내부 타이밍 정확도, 느슨한 PG 분포의 99.5% 상한, 새 랭크 경계를 결과·리플레이·HUD에 일관 적용
- 긴 설정 화면의 오른쪽 스크롤바를 클릭해 항목으로 바로 이동할 수 있게 하고, 스크롤 클릭만으로 설정값이 실행되지 않도록 선택/활성 동작을 분리
- 화면 공간 때문에 도움말이 잘리면 마지막 표시 줄에 `F1`과 숨겨진 도움말 줄 수를 안내
- LN Mix로 선택된 롱노트 길이를 모든 Mix 단계에서 base BPM 기준 16비트 70% / 8비트 20% / 24·32비트 10%로 결정적 배분

### Packaging

- Windows `TenRiff-1.2.8.zip`, 공개 소스 `TenRiff-1.2.8-source.zip`, `TenRiff-1.2.8-SHA256SUMS.txt`를 정식 배포 자산으로 구성하고 stage/ZIP/해시 일치를 검증

## [1.2.7] - 2026-07-30

### Fixed

- External ONNX Upscaler가 모델의 tensor type을 확인하지 않고 항상 FP32로 바인딩해 FP16 입출력 모델이 즉시 실패하던 문제 수정
- 외부 FP32/FP16 경계를 유지하는 INT8 QDQ 모델의 `tenriff.quantization` metadata를 감지해 내부 INT8 양자화를 로그에 명시하고, raw INT8/UINT8 경계는 scale/zero-point 계약이 없음을 정확히 안내
- 동영상 BGA가 추론 중에도 매 프레임 요청 ID를 덮어써 완료된 GPU 결과를 계속 stale 처리하던 문제를 one-in-flight backpressure로 수정

### Changed

- External ONNX Upscaler 기본 장치를 고성능 DirectX GPU로 변경하고, 저전력/NPU 선호는 Graphics Settings에서 명시적으로 켜는 실험 옵션으로 전환

## [1.2.6] - 2026-07-29

### Added

- 로컬 표준 BMS 난이도표 헤더 JSON을 선택하면 차트 파일의 SHA-256/MD5를 대조해 표 이름·기호·레벨·순서를 Song Index에 연결하고, Song Select 배지·정렬·그룹·숫자 필터에 우선 반영
- External ONNX Upscaler에 `NPU 우선(실험)` 설정을 추가해 Windows의 저전력 DirectX ML 장치를 먼저 요청하고, 세션 생성 또는 첫 추론이 거부되면 고성능 DirectX 경로로 재시도

### Fixed

- 실시간 BMS 입력이 오디오 write cursor보다 늦게 도착했을 때 판정·리플레이 시각은 그대로 유지하면서 가청 키음만 현재 쓰기 가능한 버퍼 경계에 붙여 짧은 키음이 통째로 빠지던 문제 수정
- LR2 playskin의 누락된 `#IMAGE`도 선언 슬롯을 소비하고 include/상위 파일이 같은 슬롯 카운터를 공유하도록 바꿔 이후 `#SRC_*` 이미지 번호가 밀리던 문제 수정
- 난이도표를 바꾼 직후 기존 Song Index 캐시에 저장된 차트 해시를 재사용해 대규모 라이브러리를 다시 파싱하지 않고 표 레벨을 즉시 갱신하며, F5 강제 재검색은 파일 크기와 밀리초 mtime까지 확인해 같은 초에 바뀐 차트의 해시도 다시 계산
- 헤더 형식이 아니거나 읽을 수 없는 난이도표 JSON은 설정에 저장하지 않아 적용되지 않은 표가 선택된 것처럼 남던 상태 표시 문제 수정

### Changed

- 차트 입력을 BMS 계열(`.bms/.bme/.bml/.pms`) 전용으로 정리하고 `.osu` 파서, `.osz` 가져오기, 관련 설정·캐시·테스트·miniz 의존성을 제거
- 스킨 입력을 native/LR2 전용으로 정리하고 `.osk` 가져오기와 osu 스킨 런타임을 제거하되, 기존 사용자 스킨 폴더는 삭제하지 않고 오래된 `skin.source=osu`만 native로 안전 정규화
- ONNX 업스케일러의 자동 FPS 벤치마크/차단을 제거하고, 모델 경로 선택과 수동 ON/OFF 및 기본 `아니오`인 고사양 경고 확인만 남김
- Song Select 인덱싱 stage/퍼센트/ETA 진행 표시를 화면 중앙으로 옮기고, 메뉴에서 `-`/`+` 및 Mode Settings로 다음 플레이 Rate를 조정하도록 연결

### Packaging

- 모델을 포함하지 않는 Windows `TenRiff-1.2.6.zip`, 공개 소스 `TenRiff-1.2.6-source.zip`, SHA-256 manifest를 같은 릴리즈 자산으로 구성

### Verification

- Release 단위 테스트 `438 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과 및 `.osu` 확장자 unsupported 회귀, 키음 지연 입력, 난이도표 해시/캐시/UI 정렬, LR2 이미지 슬롯 회귀를 포함해 확인
- NPU 실제 선택 여부는 Windows/드라이버 정책에 달려 있으므로 Release 빌드와 폴백 경로를 확인하고, NPU 실기 추론은 지원 장치에서 별도 확인 대상으로 유지

## [1.2.5] - 2026-07-29

### Changed

- LunaSR 모델명을 노출하던 배경 업스케일 연동을 모델 중립적인 `External ONNX Upscaler`로 교체하고, 기존 `lunasr` 설정값은 `onnx`로 자동 마이그레이션
- Graphics Settings에 `.onnx` 파일 선택 행을 추가하고, 파일 드롭도 지원하며 선택 경로를 `background_upscale_model_path`에 프로필별 저장
- 선택된 모델 경로마다 WinML session과 35 FPS 성능 게이트를 공유하고, `off` 상태에서는 모델을 로드하거나 벤치마크하지 않도록 유지
- 현재 호환 계약을 `rgb_lr` float32 NCHW 1x3x540x960 입력과 `rgb_residual_x2` float32 NCHW 1x3x1080x1920 residual 출력으로 명시

### Packaging

- Windows `TenRiff-1.2.5.zip`, 공개 소스 `TenRiff-1.2.5-source.zip`, SHA-256 manifest를 모델 비포함 패치 릴리즈 자산으로 구성

### Verification

- Release `TenRiff.exe`, CLI/GUI BMS key converter, External ONNX WinML smoke target 빌드 성공
- 모델 경로가 없는 smoke가 예상 코드 `2`로 종료되어 외부 모델 필수 경계를 확인
- 활성 checkout과 독립 source stage에서 각각 `478 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- 독립 source stage에서도 Release 실행 파일·CLI/GUI converter·External ONNX smoke target을 새 경로에서 전체 빌드
- Windows/source stage와 ZIP의 ONNX·checkpoint·모델 전용 metadata, legacy `tools/lunasr` 경로, 개인 경로·credential 패턴이 모두 0개임을 확인

## [1.2.4] - 2026-07-29

### Changed

- 권리 경계가 불명확한 제3자 게임 촬영 기반 LunaSR ONNX·checkpoint 파생물·모델 전용 검증 메타데이터를 공개 저장소와 Windows/source 릴리즈 패키지에서 제외
- LunaSR 코드는 권리 정리된 사용자 모델을 `lunasr_user_rgb_x2_winml.onnx`로 직접 넣는 opt-in 연동으로 유지하고, 공개 설정 기본값과 잘못된 설정 fallback을 `off`로 변경

### Fixed

- 삭제된 `tools/lunasr/LICENSE.LunaSR`를 계속 복사·해시·고지하던 stale CMake와 배포 문서 참조를 제거해 모델이 없는 공개 Release 빌드가 실패하지 않도록 정리
### Packaging

- Windows `TenRiff-1.2.4.zip`, 공개 소스 `TenRiff-1.2.4-source.zip`, SHA-256 manifest를 모델 비포함 패치 릴리즈 자산으로 구성

### Verification

- 활성 checkout과 공개 소스 stage에서 각각 `478 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- Release `TenRiff.exe`, CLI/GUI BMS key converter, 사용자 모델용 LunaSR WinML smoke target 빌드 성공
- 모델이 없는 LunaSR smoke가 예상 코드 `2`로 native fallback 경계를 확인했고 Release 출력·Windows ZIP·source ZIP의 ONNX/checkpoint/model metadata가 모두 0개임을 확인
- 바이너리 ZIP `20 entries`, 소스 ZIP `339 entries`, source stage `314 files`의 stage/추출본/working tree SHA-256 parity와 개인 경로·credential 패턴 0건을 확인

## [1.2.3] - 2026-07-28

### Changed

- LunaSR 고정 RGB x2 성능 게이트를 `200 FPS`에서 `35 FPS`로 낮춰, 측정값이 35 FPS 이상이면 업스케일을 활성화하고 미만이면 native BGA scaling을 유지
- 권장 GPU를 `RTX 3070급 이상`으로 추정해 안내하되, 실제 활성화 여부는 GPU 이름이 아니라 각 PC의 최초 35 FPS 벤치마크 결과로 결정

### Verification

- Release 증분 빌드, 단위 테스트 `478 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- WinML 반복 측정에서 `25.23 FPS`는 차단되고 `40.29 FPS`는 35 FPS 게이트를 통과해 1920x1080 BGRA 파이프라인까지 완료

## [1.2.2] - 2026-07-28

### Fixed

- procedural `circle / triangle / pentagon / hexagon` 노트가 100% 설정에서도 짧은 note height에 맞춰 막대보다 작게 보이던 문제를 수정하고 lane 전체 폭을 기준으로 정규화
- Media Foundation이 열지 못하는 MPG/MPEG BGA를 persistent `ffmpeg.exe` image-pipe 폴백으로 실시간 표시

### Changed

- LunaSR runtime을 staged32 RGB FP16 residual 모델로 교체하고, 원본 IR 10은 보존하면서 graph·weight가 같은 WinML IR 9 파생본을 사용
- 고정 960x540 RGB x2 추론이 200 FPS 이상인지 최초 1회 벤치마크하고, 미달·오류 시 해당 프로세스에서 LunaSR를 차단한 뒤 native BGA scaling 유지

### Packaging

- Windows 배포물 `baepo/TenRiff-1.2.2.zip`, 공개 소스 번들 `opensource-Tenriff-source/TenRiff-1.2.2-source.zip`, SHA-256 manifest를 새 패치 릴리스 자산으로 구성

### Verification

- Release 전체 빌드, 단위 테스트 `478 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- 실제 320x240 MPEG-1 BGA의 FFmpeg 폴백에서 첫 프레임과 0.23초 이후 프레임 진행 확인
- 원본 IR 10과 WinML IR 9 모델 모두 ONNX checker 통과, IR 필드 정규화 후 graph byte 동일 확인
- WinML RGB FP16 출력 스모크 통과; 반복 벤치마크 `24.09~45.70 FPS`로 200 FPS 미달 차단 정책 확인
## [1.2.1] - 2026-07-27

### Added

- 싱글플레이 중 `Esc`로 논리 재생 시간을 멈추는 pause menu를 열고 `계속하기 / 재시작 / 나가기`를 선택할 수 있도록 추가
- native note skin에 `triangle / pentagon / hexagon` shape와 롱노트 tail cap 표시 토글을 추가

### Changed

- LunaSR runtime 모델을 `basic_v2_final.pt`에서 내보낸 공개 WinML ONNX로 교체하고, gameplay BGA base/overlay뿐 아니라 Song Select에서 선택된 FHD 미만 BGI에도 비동기 보간을 적용
- pause 중 WASAPI stream은 silence로 유지하되 chart/audio/judgement의 논리 sample clock은 정지하도록 변경

### Packaging

- Windows 배포물 `baepo/TenRiff-1.2.1.zip`, 공개 소스 번들 `opensource-Tenriff-source/TenRiff-1.2.1-source.zip`, SHA-256 manifest를 새 패치 릴리스 자산으로 구성

### Verification

- 활성 checkout 단위 테스트 `475 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- Release `TenRiff.exe`, LunaSR WinML GPU smoke와 1920x1080 BGRA pipeline 통과

## [1.2.0] - 2026-07-26

### Added

- BMS 채널 `04`/ `07` 이미지 cue와 osu!mania Events 배경을 gameplay sample timeline에 연결하고, FHD 미만 이미지를 LunaSR/Windows ML worker로 비동기 1920x1080 보간하는 선택형 `BGA Upscale` 설정을 추가
- 원본 ONNX를 보존하면서 Windows ML이 읽을 수 있는 IR v9 호환 사본, GPU smoke target, 모델/license의 Release 후처리 복사를 추가

### Changed

- LunaSR 결과가 준비되기 전이나 모델·디코드·추론 실패 시 기존 native background를 계속 표시하며, PNG 알파를 FHD 출력에도 유지하도록 구성

### Packaging

- Windows 배포물 `baepo/TenRiff-1.2.0.zip`, 공개 소스 번들 `opensource-Tenriff-source/TenRiff-1.2.0-source.zip`, SHA-256 manifest를 새 정식 릴리스 자산으로 구성

### Verification

- 활성 checkout과 공개 소스 ZIP 추출본에서 각각 단위 테스트 `471 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- 양쪽에서 Release `TenRiff.exe`, CLI/GUI BMS key converter, LunaSR WinML smoke target 링크 성공
- 바이너리 ZIP `21 entries`, 소스 ZIP `337 entries`, converter ZIP `3 entries`와 빈 `logs/`·`songs/`, stage/추출본 파일별 SHA-256 일치를 검증

## [1.1.9] - 2026-07-23

### Fixed

- `Sudden Death (1 MISS)`가 모든 네이티브 `BAD`가 아니라 실제 osu!mania OD8 객체 `MISS`에서만 발동하도록 수정해, OD8 기준으로 유효한 롱노트 헤드가 입력 즉시 Miss로 강제 변환되던 문제를 해결

### Packaging

- 공개된 `1.1.8` 태그와 자산은 그대로 보존하고, Windows 배포물 `baepo/TenRiff-1.1.9.zip`과 공개 소스 번들 `opensource-Tenriff-source/TenRiff-1.1.9-source.zip`을 새 패치 릴리스 자산으로 구성

### Verification

- 활성 checkout과 최종 공개 소스 ZIP 추출본에서 각각 단위 테스트 `467 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- Release `TenRiff.exe`, CLI/GUI BMS key converter를 활성 checkout과 독립 소스 추출본에서 모두 링크
- 바이너리 ZIP `18 entries`, 소스 ZIP `327 entries`, converter ZIP `3 entries`를 확인하고, 실제 압축 해제본과 stage의 파일별 SHA-256 및 빈 `logs/`/`songs/`, 금지 경로·개인 로컬 경로·secret 형태 문자열 부재를 검증

## [1.1.8] - 2026-07-23

### Added

- 인게임 HUD와 결과 화면에 실제 입력 타이밍을 osu!mania stable OD8 판정창 및 ScoreV1 기준으로 환산한 `OSU OD8` 보조 점수를 추가하고, 리플레이/결과 JSON에도 변환 점수와 판정 수를 저장
- Mode Settings에 첫 `BAD`에서 게이지를 0으로 만들고 즉시 종료하는 `Sudden Death (1 MISS)`를 추가하며, 빈 키 `POOR`는 제외하고 Practice No-Fail과는 상호 배타적으로 처리
- Mod Manager의 Note Structure에 `LN Mix 10%~90%`를 추가해 기존 롱노트는 유지하고, 50ms 이상 길이와 다음 동일 레인 노트 전 50ms 여유를 확보할 수 있는 단노트 중 설정 비율을 `Random Seed` 기반으로 골라 일반 롱노트로 결정적으로 변환

### Fixed

- LN Mix로 생성한 롱노트 꼬리가 다음 동일 레인 노트의 1 sample 전까지 이어져 겹쳐 보이고 release/repress 여유가 없던 문제를 수정해 최소 50ms 간격을 보장하고, 기존 동일 레인 span과 충돌하는 head는 변환 후보에서 제외
- Full LN, Full Tap, LN Mix처럼 노트 구조 mod가 다른 replay를 기본 ghost로 선택하거나 현재 차트에 그대로 재생하지 않도록 정규화된 mod 구성이 같은 경우만 호환 처리

### Packaging

- 정식 Windows 배포 자산을 `baepo/TenRiff-1.1.8`과 `TenRiff-1.1.8.zip`으로 구성하고, 빈 `logs/`/`songs/`, 멀티플레이 안내, BMS key converter를 포함
- 공개 소스 번들을 활성 worktree의 명시 allowlist에서 `opensource-Tenriff-source/TenRiff-1.1.8-source.zip`으로 구성해 새 OD8 구현/테스트 파일까지 포함

### Verification

- 활성 checkout과 독립 공개 소스 stage에서 각각 단위 테스트 `463 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- Release `TenRiff.exe`, CLI/GUI BMS key converter를 활성 checkout과 독립 공개 소스 stage에서 모두 링크
- 바이너리 ZIP `18 entries`와 소스 ZIP `327 entries`를 실제 압축 해제해 stage 파일별 SHA-256과 대조하고, 빈 `logs/`/`songs/`, 금지 산출물·개인 로컬 경로·secret 형태 문자열 부재를 확인

## [1.1.7] - 2026-07-22

### Changed

- 네이티브 노트와 이미지가 없거나 일부만 있는 스킨 fallback을 캐시된 단일 패스 재질 그라데이션으로 개선하고, 롱노트 body의 대비와 tail cap을 명확히 하면서 기존 import 스킨 비트맵은 그대로 보존
- 0%/100% 위치에서도 판정선 core가 잘리지 않게 하고, 키 입력 pulse·판정/콤보·상태별 게이지를 cyan/white 중심 계층과 캐시된 segmented grid로 재정리
- player/ghost 콤보와 게이지를 공통 렌더 경로로 통합하고 콤보 문자열을 HUD revision 단위로 캐시해 두 화면의 크기 차이와 불필요한 프레임별 문자열 생성을 제거
- Song Select 선택곡 아트를 목록 배경에 은은하게 연결하고 선택 카드의 cyan rail/top sheen, 미리보기 HUD band, 긴 필터 값용 compact 표기를 추가해 선택 상태와 정보 대비를 강화

### Packaging

- 정식 Windows 배포 자산을 `baepo/TenRiff-1.1.7`과 `TenRiff-1.1.7.zip`으로 구성하고, 빈 `logs/`/`songs/`, 멀티플레이 안내, BMS key converter를 포함
- 공개 소스 번들을 활성 Git checkout의 명시 allowlist에서 `opensource-Tenriff-source/TenRiff-1.1.7-source.zip`으로 구성

### Verification

- 0%/100% 판정선 끝점의 노트 좌표 연속성, active LN의 판정선 고정, stale-head에서 headless hold로 전환될 때 빈 프레임이 생기지 않는 조건을 렌더 회귀 테스트로 보강
- 활성 checkout과 독립 공개 소스 stage에서 각각 단위 테스트 `449 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- Release `TenRiff.exe`, CLI/GUI BMS key converter 링크 성공
- 바이너리 ZIP `18 entries`와 소스 ZIP `324 entries`를 실제 압축 해제해 stage 및 checkout 파일 해시와 대조하고, 빈 `logs/`/`songs/`, 금지 산출물·개인 로컬 경로·secret 형태 문자열 부재를 확인
- 앱 내장 캡처로 1280x720 Song Select 및 gameplay의 레이아웃·판정선·게이지를 확인

## [1.1.6] - 2026-07-22

### Added

- Mode Settings의 Random에 결정적 `Mirror`를 추가하고, 일반 키모드는 전체 반전하며 10K/16K는 두 플레이어 영역을 유지한 채 각 절반 안에서 반전

### Changed

- BMS/osu!mania 네이티브 난이도 계산에서 롱노트 Head/Tail miss-ms를 0.5배로 완화해 `300ms -> 150ms`처럼 평가하고, 기존 LV/CR 캐시를 재사용하지 않도록 song-index schema를 10으로 갱신 (실플레이 판정창은 변경 없음)

### Fixed

- 스킨 판정선 위치를 기존 55~86% 제한 대신 0~100% 전체 범위에서 1% 단위로 조절할 수 있도록 설정과 렌더러 제한을 통일
- 롱노트 머리 판정 직후 HUD 스냅샷이 이전 active-hold 상태와 새 hidden 상태를 섞지 않도록 동기화하고, 다음 HUD 갱신까지 롱노트 몸통이 끊기지 않게 렌더 전환을 보강

### Packaging

- 정식 Windows 배포 자산을 `baepo/TenRiff-1.1.6`과 `TenRiff-1.1.6.zip`으로 새로 구성하고, 빈 `logs/`/`songs/`, 멀티플레이 안내, BMS key converter를 포함
- 공개 소스 번들을 활성 Git checkout의 명시 allowlist에서 `opensource-Tenriff-source/TenRiff-1.1.6-source.zip`으로 새로 구성

### Verification

- 활성 checkout과 독립 공개 소스 스테이지에서 각각 단위 테스트 `449 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- Release `TenRiff.exe`, CLI/GUI BMS key converter 링크 성공
- 바이너리 ZIP 18 entries와 소스 ZIP 324 entries를 실제 압축 해제해 stage 해시와 대조하고, 빈 `logs/`/`songs/`, 금지 경로·개인 로컬 경로·secret 형태 문자열 부재를 확인

## [1.1.5] - 2026-07-20

> Local staging only; no GitHub tag/release was published. These changes are included in `1.1.6`.

### Added

- `.osk` 파일 선택/드롭으로 활성 프로필의 `skins`에 스킨을 설치하고, `.osz`는 `Shift+F2` 또는 드롭으로 활성 songs source에 설치한 뒤 osu chart를 활성화하고 재인덱싱하는 import 경로를 추가
- osu!mania 스킨의 지원 note/LN 이미지와 `ColumnWidth`, `ColumnSpacing`, `ColumnLineWidth`, `HitPosition` 같은 레이아웃 값을 gameplay에 반영하고, 현재 렌더링하지 않는 다른 osu! mode 자산도 archive 안의 원본 파일은 보존

### Changed

- 신규 설치와 `ghost_battle_enabled` 키가 없는 설정에서는 고스트 배틀을 기본 `OFF`로 시작하되, 사용자가 이미 저장한 명시적 `true` 선택은 그대로 유지
- Graphics 화면에서 실제 DXGI 전체 화면을 `독점 전체 화면`으로 명확히 표시하고, Discord 음성 오버레이에는 `Borderless/Windowed`와 좌하단 Voice 위젯 배치를 안내

### Security / Compatibility

- OSK/OSZ archive 전체를 먼저 검증하고 전용 staging에 압축 해제한 뒤 원자적으로 설치하며, 기존 폴더를 덮어쓰지 않고 충돌 시 새 이름을 사용
- 경로 탈출·절대/UNC 경로, 심볼릭 링크, Windows 예약 경로, 대소문자/파일·폴더 충돌, 손상 CRC, 과도한 압축 해제 크기와 비정상 ZIP 메타데이터를 거부
- 일반 ZIP64와 UTF-8/레거시 CP932 파일명을 지원하고, `.osu`의 배경·오디오·히트사운드 참조를 차트 폴더 내부로 제한
- archive의 유효 파일은 보존하지만 화면 적용 범위는 TenRiff가 지원하는 osu!mania gameplay 요소이며, 모든 osu! mode/UI의 pixel-perfect 재현을 의미하지는 않음
- Discord 항목은 기존 Game Overlay와의 실행 모드·배치 호환 안내이며, Discord SDK 기반 음성 기능이나 참가자 목록 직접 렌더링은 추가하지 않음

### Packaging

- 정식 Windows 배포 자산을 `baepo/TenRiff-1.1.5`와 `TenRiff-1.1.5.zip`으로 갱신하고, 빈 `logs/`/`songs/`, 멀티플레이 안내, BMS key converter를 포함
- 공개 소스 번들을 `opensource-Tenriff-source/TenRiff-1.1.5-source`와 같은 이름의 ZIP으로 갱신하고, build tree·사용자 데이터·내부 작업 파일을 제외한 명시 allowlist만 포함
- 바이너리 패키지의 `THIRD_PARTY_NOTICES.md`에 miniz 3.1.2 MIT 전문을 직접 포함해 정적 링크 배포에서도 라이선스 참조가 끊기지 않도록 정리

### Verification

- CMake project/cache 버전 `1.1.5`, Release `TenRiff.exe`, CLI/GUI BMS key converter 링크 성공
- 작업 checkout과 독립 공개 소스 스테이지에서 각각 단위 테스트 `444 pass / 1 optional skip / 0 fail`, CTest `1/1` 통과
- 일반 deflate/ZIP64/CP932와 무덮어쓰기, 경로 탈출, 링크·충돌, 크기 제한, CRC, staging 정리 회귀 테스트 통과
- osu 스킨 매핑·고해상도/CJK 자산·fallback·레이아웃 수치와 osu 자산 경로 제한 회귀 테스트 통과
- 수동 확인 잔여: 다양한 실사용 OSK/OSZ의 GUI 호환성, 실제 Discord 음성 참가자/발화자 overlay, 권한이 필요한 Windows symlink 경로, 모든 외부 스킨의 시각적 동일성

## [1.1.4] - 2026-07-20

### Changed
- `1.1.3 Multiplayer Preview r5`까지의 직접 IP 멀티플레이와 RawInput/Polling 수명주기 수정을 정식 stable 라인에 통합
- Rate는 곡 재생 속도와 차트 스케줄만 바꾸고, 같은 Hi-Speed의 시각 스크롤 속도는 유지하도록 정리

### Fixed
- 같은 레인에서 노트 하나를 놓친 뒤 넓은 `BAD` 창의 이전 노트가 다음 정확 입력을 계속 가로채던 판정 고정을 수정하고, 이전 노트는 `BAD` 미스로 정리하되 바로 다음 노트가 `GOOD` 이상으로 명확할 때 그 입력을 다음 노트에 배정
- Windows를 며칠 이상 재부팅하지 않은 상태에서도 `ClockSync`가 큰 QPC 절대값 때문에 회귀 정밀도를 잃지 않도록 centered anchor 회귀로 변경하고, 지속적인 오디오 시계 불연속 뒤 자동 재기준화
- gameplay backlog stale 여부를 QPC 이벤트 나이로 판별하고 fresh 입력은 최신 playback anchor와 대조해, 키 불빛은 반응하지만 이후 입력이 비채점 catch-up으로 고정되던 경로를 복구
- 낮은 Rate에서 HUD가 `Hi-Speed / Rate`를 적용해 노트 스크롤이 오히려 빨라지던 문제를 수정

### Packaging
- 정식 Windows 배포 자산을 `TenRiff-1.1.4.zip`으로 갱신하고, 멀티플레이 안내와 BMS key converter를 stable 패키지에 포함

### Verification
- 기본 판정창과 210ms 동일 레인 스트림으로 `BAD` 연쇄를 재현해 수정 전 실패를 확인하고 수정 후 정상 복구를 검증
- multi-day QPC, 지속 clock discontinuity, fresh/stale input anchor, Rate/Hi-Speed 독립 회귀 테스트를 추가
- Release `bms_parser_tests`/CTest 통과, Release 실행 파일 링크 성공, 문제 곡 사용자 실플레이 재검증 통과

## [1.1.3 Multiplayer Preview r5] - 2026-07-19

### Fixed
- RawInput 등록을 process-global 단일 소유권으로 관리해 메뉴와 gameplay 입력 인스턴스가 전환 중 서로의 등록을 해제하지 않도록 수정
- RawInput 대상 창 교체·종료와 숨은 message window 종료를 100ms 주기로 감지하고, 키 입력을 기다리지 않은 채 같은 입력 스레드에서 Polling으로 즉시 전환
- 한 번 확인된 Polling fallback을 profile 설정 파일은 덮어쓰지 않고 현재 앱 실행과 다음 gameplay까지 유지해, 사망·메뉴 복귀 뒤 키를 움직여야 입력이 살아나던 상태를 제거
- `Input Settings`와 `Profile Setup`에서 Polling/RawInput을 직접 선택하고 RawInput 재시도를 명시적으로 수행할 수 있도록 backend 설정 흐름을 정리
- 소유권 충돌, 대상 창 손실, message pump 종료, fallback 고정 및 profile 설정 동작을 회귀 테스트로 추가

### Packaging
- 입력 backend 수명주기 수정이 포함된 `TenRiff-1.1.3-multiplayer-preview-r5.zip`을 새 GitHub prerelease 자산으로 게시하고, 기존 stable 1.1.3 및 preview r4 자산은 유지

## [1.1.3 Multiplayer Preview r4] - 2026-07-19

### Added
- 동일 차트 파일을 가진 두 PC가 IP와 TCP 포트로 직접 연결하는 1:1 멀티플레이를 추가하고, 로비/Ready/호스트 시작, 양쪽 로딩 barrier, 실시간 상대 HUD, 최종 결과 및 재대전 흐름을 지원
- 멀티 선곡 권한을 호스트로 제한하고, 참가자는 호스트의 차트 HASH+파일 크기로 현재 곡 폴더와 프로필에 기록된 최근 로드 폴더의 기존 인덱스에서 동일 차트를 자동 선택
- Options에 `Profile Setup` 재진입 항목을 추가해 현재 프로필의 언어·오디오·입력 백엔드·그래픽·키맵을 첫 실행과 같은 한 화면에서 다시 설정
- 양쪽 플레이 종료 후 내 결과와 상대 결과, 승패 및 signed 점수차를 함께 보여주는 대전 결과 화면을 추가

### Changed
- 멀티플레이 게이지를 세션 전용 단방향 시프트로 변경해 `Normal`이 `33%` 이하가 되면 `Easy 100%`로 한 번 전환되고, 이후 되돌아가지 않으며 `Easy 0%`에서 해당 플레이어가 Game Over되도록 조정
- 플레이 중 로컬 기준 점수차를 `LOSS(-10,000) <- 0 -> WIN(+10,000)` 끝단으로 표시하되, 끝단 도달은 표시 포화일 뿐 경기를 종료하지 않도록 분리
- 한쪽이 먼저 Game Over되면 다른 플레이어는 계속 진행하고, 사망한 쪽은 상대의 aggregate 점수·콤보·게이지·상태를 보며 결과를 기다리도록 변경; protocol이 전송하지 않는 정확한 lane input/note state는 관전 화면에 표시하지 않음

### Fixed
- 메뉴↔게임 입력 전환에서 이전 RawInput 인스턴스의 늦은 해제가 새 등록까지 제거하지 않도록 소유권을 분리하고, 메뉴 health fallback을 복구해 입력 스레드만 살아 있는 무응답 상태를 방지
- gameplay RawInput 경로에 같은 `InputThread`/`KeyStateTracker`를 쓰는 bound-key polling shadow를 항상 유지하고, polling press가 먼저 도착한 뒤 늦은 Raw press가 오는 순서도 소유권 이전으로 중복 없이 처리해 빠른 잭 입력의 release/재입력 누락을 방지
- RawInput message pump가 실행 중 예기치 않게 끝나면 queue와 pressed-state를 초기화하지 않고 같은 producer thread에서 Polling으로 전환해 플레이 중 노트 키 입력이 통째로 멎는 경로를 복구
- 멀티 로비에서 Options 진입 시 Ready를 먼저 해제하고, 멀티 Result의 마우스 Back이 round 상태 정리를 우회해 이후 싱글 Result를 P2P로 오인하던 문제를 수정
- 경기 종료 HUD와 shutdown 경로가 FinalScore를 중복 전송해 상대 round reset과 충돌할 수 있던 race를 제거
- protocol v2의 round nonce + 전용 `RoundReset`으로 한쪽이 Result를 먼저 닫았을 때 다음 Ready/선곡이 상대 FinalScore를 지우거나 연결을 끊던 경쟁 조건을 막고, 양쪽 Result 종료 뒤에만 재대전을 해제
- 모든 경기 패킷을 round nonce로 묶고 ordered `RoundCancel`/ACK barrier를 추가해, Launch와 빠른 Ready 해제가 교차하거나 지연 Loaded/Chart가 도착해도 연결을 유지한 채 로비 상태를 양쪽 Ready=false로 동기화
- heartbeat RTT의 절반만큼 참가자 Begin 지연을 보정해 직접 IP 환경의 시작 시점 오차를 줄임
- 싱글 진입/실행 시 남은 멀티 세션·선곡 상태를 정리하고 gameplay 호출 목적을 명시적으로 구분해, 멀티 화면을 거친 뒤 싱글 플레이가 대전 경로로 오인되어 메뉴로 복귀하던 문제를 수정

### Packaging
- 개인정보성 MP3 태그를 제거한 `TenRiff-1.1.3-multiplayer-preview-r4.zip`을 별도 GitHub prerelease 자산으로 게시하고, 기존 stable 1.1.3 자산은 유지

## [1.1.3] - 2026-05-11

### Added
- `EX-Hard` 게이지를 추가해 Hard보다 낮은 회복과 더 큰 `BAD`/`POOR` 손실을 가진 도전용 clear status(`EX-HARD CLEAR`)를 지원

### Fixed
- LN/hold 판정의 `0.5` weight가 게이지 증감에도 적용되도록 수정하고, Easy 25% 이하 `BAD` 완화가 weighted hold `BAD`에도 같이 적용되도록 정리
- 너무 늦은 입력으로 직전 노트를 `BAD` 처리한 경우 같은 입력이 다음 노트를 즉시 판정하지 않도록 mask 기준을 입력 시각으로 변경
- gameplay RawInput 경로에서 thread 내부 polling shadow를 끄고, polling release가 stale RawInput source를 정리하도록 해 키가 눌린 상태로 stuck 되는 입력 누락 가능성을 줄임

## [1.1.2] - 2026-03-31

### Changed
- gameplay live 입력 백엔드는 저장된 RawInput 설정을 우선 사용하되 bound-key polling shadow를 함께 유지하고, RawInput 초기화/시작 실패 시 Polling으로 재시도해 입력 인식이 끊기지 않도록 정리
- 메뉴 입력은 기존 foreground process/root-window 경계를 유지하면서 RawInput 시작 실패 시 Polling fallback으로 재시도하고, 저장된 `input.backend` / `input.rawinput` 값은 runtime fallback 결과로 영구 rewrite하지 않도록 유지
- `1.1.1` 문서에서 하이브리드 gameplay 입력으로 설명하던 부분을 RawInput 우선 + polling shadow/fallback 동작 기준으로 다시 맞춤
- `docs/baseline-1.1.2*`를 새 기준선 문서로 승격하고, `1.1.2` 라인을 현재 공개 기준의 `final stable` 버전으로 명명

### Packaging
- 프로젝트 메타데이터, 문서, `build-dist` 기준 Windows 배포 스테이징, 공개 오픈소스 소스 스테이징을 `1.1.2` 라인으로 갱신

## [1.1.1] - 2026-03-31

### Changed
- gameplay 입력 경로를 하이브리드 입력 캡처로 재정리해 `RawInput` 사용 시에도 polling shadow가 계속 같이 동작하고, 두 source는 `KeyStateTracker`에서 dedupe된 뒤 하나의 gameplay queue로 합류하도록 수정
- `InputThread`에 runtime gate policy를 추가해 메뉴는 기존처럼 foreground process 기준 입력 게이트를 유지하고, gameplay 세션은 foreground 여부와 무관하게 입력을 계속 수집하도록 분리
- gameplay/menu의 restart형 `RawInput -> Polling` 자동 재시작 및 profile 입력 설정 영구 rewrite 경로를 제거하고, runtime backend 상태는 실제 소비된 입력 source 기준으로만 갱신하도록 정리

### Packaging
- 프로젝트 메타데이터, 문서, `build-dist` 기준 Windows 배포 스테이징, 공개 오픈소스 소스 스테이징을 `1.1.1` 라인으로 갱신

## [1.1.0] - 2026-03-31

### Changed
- 포커싱/입력 게이트 경로를 `0.999` 방식으로 되돌려, `InputThread`는 다시 현재 foreground process 기준으로 입력 허용을 판정하고 `MenuWindow` UI hit-test/fullscreen foreground 판단도 root-window foreground 비교를 사용하도록 정리
- `1.0.9`에서 도입했던 gameplay playback-head 기준 live-input timing 보정은 유지하고, shared activation state(`WM_ACTIVATEAPP`/`WM_SETFOCUS`) 기반 메뉴-입력 동기화 경로만 제거

### Packaged
- 프로젝트 메타데이터, 문서, `build-dist` 기준 Windows 배포 스테이징, 공개 오픈소스 소스 스테이징을 `1.1.0` 라인으로 갱신

## [1.0.9] - 2026-03-29

### Changed
- gameplay live input timing이 더 이상 미래 write cursor 기준으로 밀리지 않도록, `GameSession`이 `ClockSync`와 startup fallback을 실제 device playback head(`playback_sample`) 기준으로 매핑하도록 수정
- 장치 padding이 큰 노트북/공유 WASAPI 환경에서 지속적으로 늦게 판정되던 경로를 줄이기 위해 gameplay 시작 시 오디오 timing 진단 로그(`exclusive/shared`, `sample_rate`, `buffer_frames`, `write_ahead`)를 1회 남기도록 보강
- 입력 backend fallback 이후 gameplay 입력 baseline 재동기화도 마지막 committed sample이 아니라 현재 playback head를 우선 사용하도록 정리

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.9` 라인으로 갱신
- 공개 소스 패키징 스크립트가 이제 `external/llama.cpp/`를 기본 제외하여 로컬 LLM/tooling checkout이 `TenRiff-1.0.9-source*`에 섞여 나가지 않도록 고정

## [1.0.8] - 2026-03-28

### Changed
- Windows 입력 경로를 `0.999` 기준으로 되돌려 RawInput 기본 동작, foreground 기반 입력 게이트, `ClockSync` 직접 샘플 매핑, `BAD` 창 기준 backlog 압축 흐름을 다시 기본선으로 복구
- `1.0.7`에서 추가됐던 menu/gameplay 자동 Polling fallback 및 Polling 강제 영구 저장을 제거하고, profile/config seed도 다시 `rawinput=true` 기본선으로 복귀
- 입력 설정 화면에서 실제로 더 이상 쓰지 않던 `Judgement Hz` 조절 행을 제거해 현재 런타임 동작과 UI를 맞춤
- 두 키보드 source 집계 입력 처리와 한글 경로 osu asset 해석 보강은 유지한 채, 입력 핵심 경로만 `0.999` 논리로 정리
- 게임 도중 abort 후 다음 판에서 menu pressed-state와 gameplay 시작 baseline이 엉키며 입력이 흔들리던 세션 경계 회귀를 고쳐, 메뉴 스레드 재시작과 gameplay 시작 직전에 입력 상태를 다시 비우고 재동기화

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.8` 라인으로 갱신

## [1.0.7] - 2026-03-28

### Changed
- Windows `1.0.7` 테스트 릴리스 라인에서는 RawInput 불안정 원인 분리를 위해 입력 백엔드를 설정/기존 프로필 값과 무관하게 Polling으로 고정하고, 저장 시에도 `polling` + `rawinput=false`만 남도록 정규화
- 기본 전역 config, 기본 profile config, Linux launcher seed config, config schema 문서를 Polling 고정 동작에 맞게 정리
- 최근 한글 경로 보강에 이어 osu!mania skin/hitsound asset 경로와 입력 backend persistence 경계를 함께 정리해 테스트 릴리스의 재현성 우선 정책을 강화

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.7` 라인으로 갱신

## [1.0.6] - 2026-03-28

### Changed
- 같은 키를 두 키보드가 동시에 눌러도 마지막 소스가 해제될 때까지 논리적 `Pressed` 상태를 유지하도록 입력 상태 추적을 `keycode + source` 집계 방식으로 재구성
- RawInput source token을 저비트 `device_id`가 아닌 전체 폭 토큰으로 확장하고, polling aggregate source를 별도로 예약해 다중 키보드/RawInput+polling 혼합 상황의 상태 충돌을 줄임
- 메뉴/게임플레이/리플레이에서 자동 polling fallback이 발생했을 때 `configured/effective backend`, fallback origin, reason, timestamp를 상태/UI/로그에 남기고 profile backend 저장은 그대로 유지하도록 정리
- 메뉴 polling 대상 키를 현재 화면의 실제 메뉴 키로, 게임플레이 polling 대상 키를 실제 lane/control 바인딩으로 제한해 불필요한 입력 변화가 상태 추적에 섞이지 않도록 정리
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.6` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.6` 라인으로 갱신

## [1.0.5] - 2026-03-27

### Changed
- 메뉴 입력 게이트를 프로세스 foreground PID 기준 대신 앱 활성화 상태 기준으로 정리하고, `Quick Setup`/`Title`/`Song Select`/`Options`/`Keymap`에서도 `RawInput` 이상 시 자동 polling fallback이 동작하도록 보강
- Keymap 편집 모드를 선택한 차트 lane count 기준으로 맞추고, 리바인드 성공 즉시 저장되도록 바꾸면서 숨겨진 메뉴/키맵 단축키를 정리
- `Autoplay`와 `Practice (No Fail)` 보조 모드를 추가하고, assist 결과 표기/리플레이 메타데이터/기록 우선순위에서 일반 플레이와 구분되도록 정리
- 게임 시작 직전 입력 상태를 다시 baseline으로 동기화하고 startup timing anchor를 실제 입력 샘플 환산에 연결해, 카운트다운 이후 첫 입력이 늦거나 씹히는 문제를 완화
- 레이아웃 민감 OEM 키(`Semicolon`/`Bracket` 계열 등)의 polling 정규화를 고정 매핑으로 안정화하고, 기본 `vsync=false` 그래픽 프리셋의 gameplay cap을 `1050`에서 `300`으로 내려 첫 실행부터 과한 렌더 스케줄링이 입력을 흔들지 않도록 조정
- `vsync=false` gameplay render pacing에 `min(configured target, max(300, monitor_hz * 2))` safety clamp와 high-FPS adaptive wait tail을 추가해, stale/custom `1050Hz` 프로필에서도 render thread가 과하게 yield/spin 하며 입력 타이밍을 흔드는 경로를 줄임
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.5` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.5` 라인으로 갱신

## [1.0.3] - 2026-03-24

### Changed
- 한글 IME가 켜진 Windows 환경에서도 `RawInput`이 `VK_PROCESSKEY` 같은 값으로 깨질 때 scan code 기준으로 실제 키를 복구해 lane/control 입력이 끊기지 않도록 보강
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.3` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.3` 라인으로 갱신

## [1.0.21] - 2026-03-24

### Changed
- `Audio > Background Sound` 토글을 추가해 메뉴 BGM과 차트 배경음을 on/off 할 수 있도록 하고, hit keysound는 별도로 유지되도록 분리
- 게임 시작 직전에 눌린 키가 이후 입력 상태를 오염시키지 않도록, countdown 종료 직전 현재 눌림 상태를 non-scoring baseline으로 재동기화
- `ClockSync`가 아직 fit되지 않은 게임 시작 직후에는 입력을 `current_playback_sample_`에 뭉개지 않고, 오디오 콜백 시점의 startup anchor 기준으로 샘플 위치를 선형 환산하도록 바꿔 첫 몇 노트 입력이 늦게 몰리는 문제를 완화
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.21` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.21` 라인으로 갱신

## [1.0.1] - 2026-03-24

### Changed
- rate 변경 시 차트/오디오가 이미 playback timeline으로 압축된 상태를 기준으로, 판정 윈도우를 더 이상 `1 / rate`로 이중 축소하지 않도록 수정
- 인게임 HUD 상단에 곡 진행 프로그레스 바와 `경과 / 전체 / 남은 시간` 표기를 추가
- Song Select에 `ARTIST` 정렬을 추가하고, 검색 입력을 별도 Browse 화면이 아니라 Song Select 왼쪽 `SEARCH` 항목에서 바로 받도록 조정
- Song Select에 osu! 스타일의 `GROUP` 토글을 추가해 `Artist / Level / Folder / None` 기준으로 현재 정렬 안에서 묶어 볼 수 있게 조정
- Song Select 왼쪽 레일 항목이 많아졌을 때 버튼 높이/간격/텍스트를 자동으로 압축해 footer 영역을 침범하지 않도록 UI 레이아웃을 조정
- 해상도와 `windowed / borderless / fullscreen` 전환을 반복할 때 swap-chain 전환 실패가 곧바로 `Present` fatal로 이어지지 않도록 렌더러 재시도/복구 경로를 보강
- fullscreen 상태에서 `720p` 같은 낮은 해상도로 전환할 때 flip-model swap-chain이 `SetFullscreenState(TRUE)` 이후 다시 `ResizeBuffers` 되도록 순서를 고쳐, `Failed to present the menu frame` fatal을 줄이도록 보강
- 인게임 입력이 RawInput/큐 입력 정체 상황에서도 끊기지 않도록, 게임 세션 안에서 매핑된 lane/control 키를 큐 입력과 `GetAsyncKeyState` fallback polling으로 병합
- Windows 비미국권/중국권 키보드 레이아웃에서 `;`, `[`, `\` 같은 OEM 키가 깨지지 않도록 keymap/rawinput/polling 경로를 스캔코드 기반 정규화로 통일
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.1` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.1` 라인으로 갱신

## [1.0.01] - 2026-03-23

### Changed
- 오디오 콜백 내부 판정 루프를 입력 `polling_hz`(`1000/2000/4000/8000Hz`) 기준의 서브스텝으로 세분화하고, 미래로 매핑된 입력은 현재 버퍼 끝으로 당기지 않고 후속 틱까지 유지하도록 정리
- 입력 폴링과 내부 판정 서브스텝 빈도를 분리해 `input.polling_hz` 기본값은 `1000Hz`, `input.judgement_hz` 기본값은 `4000Hz`로 운용하도록 조정
- 입력 시각을 write cursor가 아니라 실제 playback head 기준으로 오디오 clock sync에 맞추도록 수정해, recent future-queue 변경 이후 생긴 체감 입력 지연/먹통 회귀를 완화
- 인게임 timing indicator를 최근 `100노트` 롤링 히스토리로 확장해, 최신 기록은 선명하게 남고 오래된 기록은 점차 흐려지며 밀려나도록 조정
- 렌더러 fatal 에러가 launcher 없이 `TenRiff.exe`만 직접 실행돼도 `logs/run.log`를 직접 남기도록 보강하고, Windows 배포판에 `launch_win.bat`와 빈 `logs/` 폴더를 다시 포함
- 판정 hot path를 직접 비교할 수 있는 synthetic `gameplay_judgement_benchmark` 타깃을 추가
- 루트 README와 문서 맵에 `vibe coding` 작품 성격과 감사 크레딧을 명시
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `1.0.01` 배포 라인 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.01` 라인으로 갱신

## [1.0.0] - 2026-03-22

### Changed
- 플레이가 끝나기 전에 중도 종료한 세션은 더 이상 `CLEAR`로 기록/export되지 않도록 결과 상태 계산을 수정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서, 기준선 문서를 `1.0.0` 기준으로 승격

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `1.0.0` 라인으로 갱신

## [0.9989] - 2026-03-21

### Changed
- `Options > Skins`에 `Target Gap`, `Lane Width`, `Lane Spacing`를 추가해 key mode별 개별 lane 폭과 lane 사이 간격을 직접 조절할 수 있도록 확장
- skin preview, gameplay, ghost field가 모두 같은 per-lane/per-gap 레이아웃 계산을 사용하도록 정리
- 판정 흐름을 LR2 기준으로 다시 분리해 note-consuming 실패는 `BAD`, 너무 이른 non-consuming 입력은 `POOR`로 처리하고, `POOR`를 결과/리플레이/UI에 다시 노출
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9989` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9989` 라인으로 갱신

## [0.998] - 2026-03-21

### Changed
- `Options > Skins`에 `16K Center Gap` 옵션을 추가해 16키 필드/미리보기에서 좌우 8레인 사이를 14K2S 스타일로 벌릴 수 있도록 정리
- Keymap 화면의 캡처/안내 문구를 분리된 하단 footer 영역으로 옮겨, `Enter`로 키 바인딩 대기를 시작해도 lane 리스트가 재배치되지 않도록 수정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.998` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.998` 라인으로 갱신

## [0.997] - 2026-03-21

### Changed
- 저장된 최고 replay가 있으면 일반 플레이에서 자동 ghost 비교를 켜고, 실플레이 왼쪽 / ghost 오른쪽 split HUD로 score, combo, gauge, judgement feedback을 함께 볼 수 있도록 추가
- Song Select에 Favorites / named Collections / local clear lamp 표시를 추가하고, 관련 필터와 상태 저장을 `config.ui`에 연결
- Options에 `Calibration Wizard` 화면을 추가해 입력 오프셋과 표시 오프셋을 즉시 저장/리셋할 수 있도록 정리
- 루트 README 3종과 `README_SOURCE_PACKAGE.md`에 공개 오픈소스 소스 패키지 전용 CMake 빌드 절차와 제외 항목(`tools/`, `10k-calc/`, runtime data dirs) 안내를 추가
- `docs/current-state*`, `docs/developer-extension-guide*`, `README_SOURCE_PACKAGE.md`에 공개 소스 패키지 갱신 시 staged source bundle 자체에서 standalone configure/build/test를 확인해야 한다는 유지보수 규칙을 추가
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.997` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.997` 라인으로 갱신

## [0.995] - 2026-03-21

### Changed
- 오토 게이지 시프트를 제거하고, 선택한 `Hard / Normal / Easy` 게이지가 곡 시작부터 종료 또는 실패까지 그대로 유지되도록 정리
- 세 게이지 모두 `100%`에서 시작하고 `0%` 도달 시 즉시 게임오버가 나도록 통일
- 리절트 화면에서 `Left`로 즉시 재시작, `F1`로 replay 재생, `F9`로 전역 스크린샷 저장을 지원하도록 입력 동작을 재배치
- 키 설정 화면에서 Save를 한 번만 눌러도 즉시 저장되고 저장 완료 메시지가 뜨도록 UX를 정리
- Graphics Settings에 `Language` row를 추가하고 메뉴 UI의 `English / 한국어` 전환을 즉시 반영하도록 추가
- 향후 다국어 확장을 쉽게 따라갈 수 있도록 `docs/localization*.md` 현지화 참고 문서를 추가
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.995` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.995` 라인으로 갱신

## [0.994] - 2026-03-20

### Changed
- 게이지 상한을 `Hard 100 / Normal 50 / Easy 40`으로 재정의
- `Normal BAD/PR`을 `-6.25`, `Easy BAD/PR`을 `-4.1`로 조정하고 다른 회복 수치는 유지
- 다운시프트 후 게이지가 다음 타입의 상한값으로 시작하도록 정리
- 직전 `Hard/Normal/Easy = -10/-6/-4` 기본 테이블을 쓰던 기존 프로필도 새 손실값으로 올라오도록 마이그레이션 경로를 추가
- 프로젝트 메타데이터와 핵심 현재 상태 문서를 `0.994` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.994` 라인으로 갱신

## [0.9.92] - 2026-03-20

### Changed
- 노말 게이지 시작 시점을 `50%`가 아니라 `100%`로 올리고, 이후 `33%` 이하에서 Easy로만 다운시프트되도록 조정
- 이지 게이지 시작 시점도 `100%`로 올려서, Easy 시작 모드에서는 `0%` 도달 시 바로 게임오버가 나도록 정리
- BAD 손실을 `Hard -4 / Normal -2 / Easy -2`로 완화
- Easy 게이지만 회복 테이블을 따로 조정해 `PG 0.032 / GR 0.0016 / GD 0.00064`를 사용하고, Hard/Normal 회복은 기존 `PG 0.01 / GR 0.05 / GD 1/65`를 유지
- 직전 기본 회복/손실 테이블(`Hard/Normal/Easy BAD = -8/-6/-4`, `Easy PG 0.135`)을 쓰던 기존 프로필도 새 값으로 올라오도록 마이그레이션 경로를 추가
- 게이지 시작값 변경을 반영하도록 관련 gauge/gameplay 회귀 테스트를 갱신
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.92` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.92` 라인으로 갱신

## [0.9.91] - 2026-03-20

### Changed
- BAD 게이지 기본 손실을 `Hard -8 / Normal -6 / Easy -4`로 재조정
- Easy 게이지가 `25%` 이하일 때 BAD/PR 손실에 추가 `0.90x` 완화를 적용
- 이전 `-14.24896 / -8.90560 / -6.27845` BAD 기본값을 쓰던 프로필도 새 값으로 올라오도록 마이그레이션 경로를 추가
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.91` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.91` 라인으로 갱신

## [0.9.19] - 2026-03-20

### Changed
- gameplay 시작 전 `3 / 2 / 1` 카운트다운 숫자를 DirectWrite text metrics 기준으로 다시 중앙 배치해 정가운데 정렬이 틀어지지 않도록 수정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.19` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.19` 라인으로 갱신

## [0.9.18] - 2026-03-20

### Changed
- 타이틀 메뉴 메인 로고 텍스트 정렬을 고정 좌표 추정 대신 DirectWrite text metrics 기준으로 재배치해 위치가 틀어지지 않도록 수정
- 기본 `BAD/PR` 게이지 손실을 이전 `0.9.17` 기본값 대비 2.2배로 강화하고, 이전 기본값을 쓰던 프로필이 새 손실값으로 마이그레이션되도록 조정
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.18` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.18` 라인으로 갱신

## [0.9.17] - 2026-03-20

### Changed
- 타이틀 메뉴의 메인 브랜딩을 중앙 정렬 `TenRiff` 락업으로 조정하고, 기존 버튼/가이드 패널 레이아웃은 유지
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.17` 기준으로 갱신

### Packaged
- `build-dist` 기준 Windows 배포 스테이징과 공개 오픈소스 소스 스테이징을 `0.9.17` 라인으로 갱신

## [0.9.16] - 2026-03-20

### Changed
- `src/render/MenuWindow.cpp`에서 분리 과정 중 누락됐던 창 초기화, imported-skin cache, gameplay static cache, mouse hit-test 구현을 복구해 `build-dist` 릴리스 링크 실패를 수정
- `build-dist` 기준 `tenriff`와 `bms_parser_tests` 빌드, `bms_parser_tests.exe` 실행까지 다시 통과

## [0.9.15] - 2026-03-20

### Changed
- `tests/unit/test_10k_calc_consistency.cpp`와 `tests/smoke/bms_10k_compare_smoke.cpp`를 조정해 로컬 `10k-calc` Python reference가 없을 때 optional check를 skip 하도록 변경
- 오픈소스 소스패키지가 `10k-calc` 체크아웃 없이도 앱 빌드와 핵심 테스트 실행을 진행할 수 있게 정리
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.15` 기준으로 갱신

## [0.9.14] - 2026-03-20

### Changed
- `MenuApp`의 Song Select render 조립과 filter/sort state rebuild 경계를 `src/app/MenuAppSongSelectRender.cpp`, `src/app/MenuAppSongSelectState.cpp`, `src/app/MenuAppSongSelectUtils.cpp`로 분리
- `MenuAppTail.inl`에서 Song Select 전용 대형 구현 블록을 제거해 tail 조각이 gameplay/result 쪽에 더 집중되도록 정리
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.14` 기준으로 갱신

## [0.9.13] - 2026-03-20

### Changed
- 유지보수용으로 `MenuApp` 분리 파일과 Song Select 경계에 짧은 설명 주석을 추가해 캐시/정렬/legacy 동기화 규칙을 더 쉽게 추적할 수 있게 함
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.13` 기준으로 갱신

## [0.9.12] - 2026-03-20

### Changed
- `MenuApp`의 Song Select 하위 유지보수 경계를 더 잘라 `src/app/MenuAppRecords.cpp`와 `src/app/MenuAppSongSelect.cpp`로 분리
- record/replay/best-result/background-preview 경로와 일부 song-list helper를 `MenuAppTail.inl`에서 분리해 tail 조각의 책임을 축소
- 프로젝트 메타데이터와 핵심 현재 상태/로드맵 문서를 `0.9.12` 기준으로 갱신

## [0.9.11] - 2026-03-20

### Changed
- `MenuApp`의 keymap 화면 입력/렌더/캡처/저장 로직을 `src/app/MenuAppKeymap.cpp`로 분리해 메인 파일과 tail 조각의 결합도를 낮춤
- 프로젝트 메타데이터와 현재 상태/로드맵 문서를 `0.9.11` 기준으로 갱신

## [0.9.10] - 2026-03-20

### Added
- 루트 README와 `docs/` 문서들의 영문/중문 번역본을 추가해 다국어 문서 진입점을 정리
- `mode`/mod 확장과 유지보수 절차를 설명하는 개발자용 문서를 추가

### Changed
- 문서 인덱스와 현재 상태 문서를 `0.9.10` 기준으로 갱신

## [0.9.9] - 2026-03-20

### Changed
- `MenuWindow.cpp`, `MenuApp.cpp`, `GameSession.cpp` 대형 구현 파일을 더 작은 구현 조각으로 분리하는 유지보수 리팩터를 진행
- 동작 변경 없이 대형 렌더/메뉴/게임플레이 구현을 분리해 후속 `1.0.0` 안정화 작업의 충돌 범위를 줄임

## [0.9.8] - 2026-03-20

### Changed
- `skin.lr2_resolution_mode` config schema를 추가하고 `auto | sd | hd | fhd` 저장/정규화/저장 테스트를 맞춤
- `docs/config.md`와 `docs/current-state.md`를 새 LR2 해상도 override 설정에 맞게 갱신
- 프로젝트 메타데이터를 `0.9.8` 기준으로 승격

## [0.9.7] - 2026-03-19

### Added
- LR2 플레이 스킨 포팅 경로를 추가해 기본 활성 분기 기준으로 노트/LN 이미지와 레인 배치를 가져올 수 있게 함
- osu!mania 쪽 per-note hitsound 해석, imported skin 크기 비율 반영, 흰선 토글, LN tail taper 같은 1.0 안정화 작업을 반영

### Changed
- `mode.key_mode` 변환이 osu!mania에도 적용되도록 정리하고, judge/easy-hard persistence, gauge 기본값, 스킨 설정 동작을 보정
- `MenuApp` 대형 파일 분리 리팩터를 진행해 skin/settings 화면 로직을 별도 translation unit으로 이동

### Packaged
- Windows 배포 스테이징과 공개 소스 스테이징을 `0.9.7` 기준으로 새로 생성
- 공개 소스 zip과 Windows 배포 zip을 `0.9.7` 기준으로 다시 생성

## [0.9.4] - 2026-03-18

### Added
- `mode.key_mode`에 `none` 옵션을 추가해 차트의 원래 키 수와 패턴 레이아웃을 그대로 따르는 native 경로를 노출
- `tools/build_with_retry.ps1`와 패키징 재시도 로직으로 Windows Defender/안티바이러스가 `TenRiff.exe`를 잠그는 동안에도 빌드/배포 재시도를 자동화

### Changed
- Mode Settings에서 BMS-only 상태여도 `Key Mode`를 `none` 포함 전체 런타임 키모드로 바꿀 수 있게 하고, 설정/마이그레이션 기본값도 `10k` 강제 대신 `none` 기반으로 정리
- 결과 화면 복귀 직후 메뉴 BGM 장치 충돌이 나던 경로를 줄이기 위해 gameplay 세션 종료 시 WASAPI/input 자원을 실제 `shutdown()`까지 수행하도록 조정
- 메뉴 BGM 재생 실패가 날 때 같은 파일을 프레임마다 다시 열어 경고를 도배하던 동작을 짧은 재시도 쿨다운으로 완화

### Packaged
- Windows 배포 스테이징과 공개 소스 스테이징을 `0.9.4` 기준으로 새로 생성
- 공개 소스 zip과 Windows 배포 zip을 `0.9.4` 기준으로 다시 생성

## [0.9.3] - 2026-03-18

### Added
- BMS 실차트 기준 `key_mode` 조합 스모크(`bms_mode_smoke`)와 `N2NC` 비교 스모크를 통해 리팩터 이후 차트 변환/모드 조합 회귀를 더 넓게 검증
- 비정상 종료 시 `logs/crash-*.log`를 남기는 Windows 크래시 로거를 추가해 추후 사용자 로그 수집/분석 경로를 마련

### Changed
- `mode.key_mode`의 `N2NC` 포팅 품질을 원본 `krrcream-Toolkit` 흐름에 더 가깝게 맞춰 note delta와 변환 shape 차이를 크게 줄임
- BMS에서 `key_mode`를 `SR/FR/full_short/full_long/judge` 등 다른 모드와 함께 써도 정렬/overlap이 깨지지 않도록 후처리와 회귀 테스트를 보강
- CMake, README, 현재 상태 문서, 패키지 스코프를 `0.9.3` 기준으로 정렬하고 공개 소스 번들에 `README.en.md`, `README.zh-CN.md`를 포함하도록 패키징 스크립트를 갱신

### Packaged
- Windows 배포 스테이징과 공개 소스 스테이징을 `0.9.3` 기준으로 새로 생성
- 공개 소스 zip과 Windows 배포 zip을 `0.9.3` 기준으로 다시 생성

## [0.9.2] - 2026-03-18

### Added
- 영문 README(`README.en.md`)와 중국어 README(`README.zh-CN.md`)를 추가하고 루트 README에 언어 링크를 연결

### Changed
- CMake 프로젝트 버전과 루트/문서 메타데이터를 `0.9.2` 기준으로 정렬
- `mode.key_mode` 변환과 메뉴 BGM 수정 이후 현재 코드 상태를 반영하도록 현재 상태/README 문구를 갱신
- `MenuApp::publish_snapshot()`가 Song Select 렌더 데이터를 직접 조립하던 중복 블록 대신 기존 `populate_song_select_render_data(...)` 헬퍼를 다시 사용하도록 정리

## [0.9.1] - 2026-03-17

### Changed
- `ModeManager` 기반 모드 정규화/차트 변환/점수 배율 계산 리팩터가 들어간 현재 런타임 상태로 공개 릴리스 메타데이터를 `0.9.1` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.9.1-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.9.1` 기준으로 다시 생성

## [0.9.0] - 2026-03-17

### Added
- BMS는 `SUBTITLE`/`DIFFICULTY`, osu!mania는 `Version`/`TitleUnicode`/`ArtistUnicode`를 활용해 차분명과 표시 메타데이터를 더 정확히 보존
- Title/Song Select 메뉴 화면에서 `Mainmusic/` 배경 음악을 재생하고, 공개 배포판도 해당 런타임 자산을 함께 포함

### Changed
- osu skin import가 현재 활성 스킨 소스를 강제로 `osu`로 덮어쓰지 않도록 조정해 native/osu 토글 고정을 해소
- 공개/오픈소스 릴리스 메타데이터를 `0.9.0` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.9.0-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.9.0` 기준으로 다시 생성

## [0.8.8] - 2026-03-17

### Changed
- 기본 `GOOD` 판정 범위를 `75ms`로 상향하고, 이전 기본값 `55ms`를 쓰는 프로필은 런타임 migration으로 자동 승격
- Linux preview launcher의 기본 judge preset도 현재 기본값(`GOOD 75 / BAD 340 / hold 80/200`)으로 정렬
- 공개/오픈소스 릴리스 메타데이터를 `0.8.8` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.8.8-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.8.8` 기준으로 다시 생성

## [0.8.6] - 2026-03-17

### Added
- Skin Settings에 key-mode별 `Divider Width` 조절 추가
- osu!mania skin의 `ColumnLineWidth`를 읽어 gameplay lane divider 폭에 반영

### Changed
- gameplay static playfield와 skin preview가 lane divider 폭 변경을 캐시 키로 추적하도록 보강
- 공개/오픈소스 릴리스 메타데이터를 `0.8.6` 기준으로 정렬

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.8.6-source`로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.8.6` 기준으로 다시 생성

## [0.8.5] - 2026-03-17

### Changed
- 공개/오픈소스 릴리스 메타데이터를 `0.8.5` 기준으로 정렬
- 루트 README, 현재 상태 문서, CMake 프로젝트 버전이 현재 공개 라인을 직접 가리키도록 갱신
- 공개 소스 번들 scope를 정리해 내부 작업용 `AGENTS.md`는 더 이상 오픈소스 패키지에 포함하지 않음

### Packaged
- 공개 소스 번들 스테이징 경로를 `opensource-Tenriff-source/TenRiff-0.8.5-source`로 새로 갱신
- Windows 배포 스테이징과 공개 소스 zip을 `0.8.5` 기준으로 다시 생성

## [0.8.0] - 2026-03-16

### Added
- 대형 라이브러리용 song indexing 프로파일 `mode.song_index_profile` 추가: 기본 `safe`, 선택 `fast`

### Changed
- song indexing을 전후 2-pass enumerate + small batch 처리로 재구성해 후보 파일 전체 적재를 제거
- 인덱싱용 BMS 파서는 `WAV/BMP`, 대부분의 unknown header, 비필수 measure command를 건너뛰는 저메모리 경로를 사용
- 대형 scan에서는 더 보수적인 worker/batch budget과 주기적 heap/working-set trim을 적용해 RAM high-water를 크게 낮춤
- Mode Settings에서 `Indexing` row로 `Safe/Fast` 프로파일을 직접 선택 가능
- 배포판과 공개 소스 번들 릴리스 라인을 `0.8.0`으로 승격

### Verified
- 46k-chart Windows benchmark library full-index 실측에서 `46,636` candidate 기준 safe profile peak 메모리가 약 `working set 453MB / private 524MB` 수준으로 완주 확인
- 같은 라이브러리 1024-chart sample 기준 fast profile이 safe profile 대비 약 `2.05x` 빠른 metadata throughput 확인

## [0.7.9] - 2026-03-16

### Changed
- 대용량/외부 BMS 폴더를 불러올 때 song source 내부에 `.tenriff/song_index.json`을 쓰지 않도록 song index cache를 profile-local 경로(`profiles/<name>/.tenriff/song-index/<source-hash>.json`)로 이동
- 기존 song source 내부 legacy cache가 있으면 읽기만 하고 새 profile-local cache로 안전하게 마이그레이션
- 배포판과 공개 소스 번들 릴리스 라인을 `0.7.9`로 승격

## [0.7.8] - 2026-03-16

### Added
- gameplay 초보자 온보딩용 `docs/gameplay-guide.md` 문서 추가

### Changed
- 간접미스(auto-miss) 판정 시점을 별도 `judge.indirect_miss` 설정으로 분리하고 기본값을 `500ms`로 완화
- 배포판과 공개 소스 번들 릴리스 라인을 `0.7.8`로 승격
- 외부 BMS 폴더 안에 song index cache를 쓰지 않도록 캐시 저장 위치를 profile-local 경로로 이동

## [0.7.7] - 2026-03-16

### Added
- 로컬 `10k-calc` Python 원본과 C++ 포트를 직접 대조하는 consistency doctest 추가
- 랜덤 실차트 샘플로 Python `10k-calc`와 TenRiff 결과를 비교하는 `bms_10k_compare_smoke` 추가
- `stb_vorbis` 기반 OGG 내부 디코더 추가
- 배포/소스 번들용 `THIRD_PARTY_NOTICES.md` 정리

### Changed
- C++ 난이도 계산을 축약판이 아니라 로컬 `10k-calc/new_calc.py` 전체 흐름 기준으로 이식
- BMS 난이도 계산 경로가 `5+1 SP`, `7+1 SP`, `14+2 DP` 같은 scratch layout의 canonical lane order를 따르도록 보정
- gameplay note motion이 HUD poll 시각이 아니라 오디오 callback 기준 시각으로 extrapolation 되도록 변경
- gameplay HUD revision을 motion/text로 분리해서 note motion 중 문자열 캐시 churn을 줄임

### Fixed
- 일부 실차트에서 발생하던 `10k-calc` 대비 난이도 오차 완화
- OGG/WAV 재생 시 불필요하게 `ffmpeg`에 의존하던 경로 제거
- gameplay note motion 미세 끊김 원인 중 하나였던 timestamp 기준점 오차 수정

## [0.7.6] - 2026-03-15

### Added
- BMS compact/SP/DP layout 지원 확대: `4K`, `6K`, `8K`, `5+1 SP`, `7+1 SP`, `PMS 9K`, `14+2 DP`
- Song Select cache-first 로드, source 브라우징, recent source 복원, drag-and-drop 폴더 인덱싱
- 결과 화면/로컬 best record 표시 강화

### Changed
- gameplay 렌더 경로 성능 최적화: fixed-size HUD transport, static playfield command-list cache, note bitmap cache
- render pacing과 performance overlay 개선
- WASAPI / chart audio playback 안정화와 sample-rate 선택 로직 개선

### Fixed
- shared-mode WASAPI stutter/slow playback 문제 수정
- 입력 backlog/미래 이벤트 처리 때문에 체감 입력이 밀리던 문제 완화
- 일부 gameplay HUD/overlay 경로의 불필요한 복사와 할당 부담 감소

## [0.7.5] - 2026-03-14

### Added
- Windows GUI 메뉴/곡 선택/옵션/결과/플레이 루프의 첫 배포 가능한 패키지 라인 정리
- replay/result JSON export와 local records 기반 결과 누적
- 초기 공개 소스 번들 라인 정리

### Changed
- 프로젝트 메타데이터와 패키지 구조를 버전별 스테이징 방식으로 정리
- 문서 엔트리포인트를 README + `docs/README.md` 기준으로 재정리
