# TenRiff 1.5.1 릴리스 증거와 운영 경계

`1.5.1`은 계정 기반 글로벌 채팅과 제출 시점 BMS 등록을 `1.5.1 fixed stable baseline`으로 고정하고, 별도 `TenRiff Server v1.1.0`과 함께 배포하는 릴리스다. 자체 호스팅 서버와 프로젝트가 직접 운영하는 중앙 공식 서버는 같은 의미가 아니다.

## 1.5.1 릴리스 게이트

- 클라이언트 Release 전체 CTest와 단위 테스트, Windows ASan CI
- 서버 Windows/Linux 테스트, client protocol 교차 테스트, Docker image 내부 CTest
- 로그인 전 main/private server 선택, F10 로그인·회원가입, 공백 보존 비밀번호 `Ctrl+V`, Windows DPAPI session 보관
- PBKDF2-HMAC-SHA256 600,000회 password hash와 hash된 bearer token
- F8 전역 global chat, `/np`, URL 경고/승인 흐름과 입력 충돌 방지
- 로그인 계정이 선택한 main/private server의 인증된 멀티 방 검색과 로그아웃 LAN 검색 fallback
- 새 BMS result 제출 시 catalog 등록, 관리자 SHA-256 제외 목록, challenge-bound replay 재검증
- 필터 상태에서도 online record 제출 경로 유지와 실패 시 local record 보존
- `27301/TCP` game, loopback-only `27302/TCP` API, `27303/TCP+UDP` HTTPS gateway 공개 매핑
- 실행/source archive 경로 안전성, standalone converter·Songs·profile·DB·secret 제외, SHA-256 manifest
- 공개 Git tag와 GitHub release asset이 같은 commit/version을 가리키는지 확인

## 운영자가 완료해야 하는 외부 게이트

- 원격 tester에게 계정·채팅·랭킹을 제공하려면 실제 domain과 신뢰 가능한 TLS certificate를 구성한다.
- `27302` origin API는 loopback에만 유지하고 외부에는 `27303` HTTPS gateway만 공개한다.
- password pepper, receipt HMAC key, backup encryption key 같은 secret은 repository나 Compose 평문 기본값에 넣지 않는다.
- 개인정보 최소 수집 고지, chat/replay/audit log 보존 및 삭제 기간, 신고·차단·이의 제기 절차를 정한다.
- 공개 중앙 서버로 표현하기 전 load/soak, reconnect, abuse/rate-limit, backup/restore, rollback 훈련을 통과한다.

위 운영 게이트를 완료하지 않은 자체 호스팅 서버는 `TenRiff official`이 아니라 운영자가 관리하는 private/community server로 표시한다.
