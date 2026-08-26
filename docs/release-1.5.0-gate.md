# TenRiff 1.5.0 릴리스 증거와 운영 경계

`1.5.0`은 클라이언트와 별도 오픈소스 TenRiff Server를 함께 배포하는 첫 자체 호스팅
기반 릴리스다. 서버 주소는 운영자가 지정하며, TenRiff가 운영하는 중앙 공식 랭킹 서버나
공개 `online_verified` 시즌이 이미 열렸다는 뜻은 아니다.

## 1.5.0에서 완료한 게이트

- 별도 공개 `10kseason/TenRiff-Server` 저장소, MIT 라이선스, `SECURITY.md`, 운영 문서
- Windows/Linux Release 바이너리, 버전 태그, GHCR 컨테이너, Compose/Caddy TLS 구성
- SQLite 계정·세션·승인 BMS catalog·challenge·기록·감사 로그와 backup/restore 복구 시험
- 비밀번호 PBKDF2-HMAC-SHA256, 해시된 bearer token, HMAC receipt, 요청 크기·동시성·rate limit
- 승인 BMS SHA-256와 일회성 challenge를 묶은 replay 업로드, 외부 verifier 재실행, claim 불신
- `.osu` 및 osu 규칙 파생 제출의 fail-closed 거부와 서버/클라이언트 protocol 교차 테스트
- Local/Online Records, 서버 정보·검증 상태·거부 이유, 잘못된 JSON/timeout에서 로컬 기록 유지
- F8 멀티플레이 채팅과 플레이·Result 입력 충돌 방지
- 클라이언트 Release CTest와 ASan, 서버 Windows/Linux/ASan/컨테이너 CI, Trivy image scan
- 배포 archive SHA-256, archive 경로 안전성, 개인정보·비밀값·DB 포함 여부 검사

## 중앙 공식 서버를 열기 전에 남은 운영 게이트

- 실제 운영 도메인·인증서·비밀 저장소·권한 분리와 staging rollback 훈련
- 개인정보 최소 수집 고지, 로그·replay 보존/삭제 기간, 이의 제기·재검증 창구
- protocol/API fuzz, reconnect·부하·장시간 soak와 장애·지연 관측/alert
- 운영 승인 BMS 원본 보관·폐기 정책, golden replay corpus, 규칙/시즌 migration
- shadow 제출로 오탐·비용·규칙 drift를 측정한 뒤 공개 `online_verified` 승격

위 항목이 끝나기 전에는 프로젝트가 운영하는 중앙 공식 서버 주소를 기본값으로 넣거나,
자체 호스팅 서버의 기록을 TenRiff 공식 랭킹으로 표현하지 않는다.
