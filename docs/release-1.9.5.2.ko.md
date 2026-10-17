# TenRiff1.9.5.2 — 노트·필드 크기 10%

기여 검토에서 작은 노트 머리보다 BMS 에디터 LN 몸통이 넓어지는 표시를 보완했습니다. 몸통은 머리 폭의80% 안에 맞추고,50~140%의 기존 몸통 좌표는 유지합니다. 에디터 머리의 기존8px 최소 표시 크기는 그대로이므로 아주 좁은 레인에서는 정확한 비례 축소보다 가독성이 우선합니다. 실제 게임의 필드 축소와는 별도입니다.

미리보기 도구의 런타임 변경 범위와 에디터 키 수·폭 전달도 수정했습니다. 실제 적용값과 노트 영역을 기록해10%·LN 화면을 검증하며, 에디터 입력 클릭 검증과 픽셀 검증을 구분합니다.

GOMazk님의 기여(PR #102)를 포함합니다. 노트·필드 크기의 조절 범위를 50–140%에서 10–140%로 넓혔습니다. 기본값100%와 저장값, 키 모드별 설정을 유지하며 메뉴·플레이·고스트·미리보기에서 같은 하한을 사용합니다. 판정·점수·입력·오디오 계산과1.9.5.1의350ms FAST/SLOW·파일 선택창 동작은 유지합니다.

Contribution: [GOMazk / PR #102](https://github.com/10kseason/TenRiff/pull/102), commit `29bc6341588390b2299d335a248617d2cde6d0ef`. The original contribution remains in Git history.

The fixed baseline remains1.9.0. No replay/schema migration, website/database deployment or bundled BGA restoration weights. Existing releases, installations and profiles are preserved.

Verification scope: [release gate](release-1.9.5.2-gate.md). Publication and downloads: [1.9.5.2](https://github.com/10kseason/TenRiff/releases/tag/1.9.5.2). Physical long play, other PCs and arbitrary imported skins remain unverified.
