# TenRiff Docs Map

이 폴더는 설계 문서와 현재 상태 문서를 같이 담고 있습니다. 빠르게 맥락을 잡아야 할 때는 아래 순서로 읽는 것이 가장 효율적입니다.

## Recommended Reading Order
1. `docs/current-state.md`
   - 현재 제품 상태, 핵심 서브시스템, 배포 규칙, 검증된 명령, 남은 수동 검증 항목
2. `AGENTS.md`
   - 최근 완료 항목과 작업 규칙의 압축 요약
3. `.codex-memory/tenriff/state/project_state.md`
   - 다음 에이전트를 위한 durable state
4. `.codex-memory/tenriff/state/open_loops.md`
   - 아직 직접 확인이 남은 항목
5. `docs/config.md`
   - 실제 설정/프로필/키맵 구조

## Which Docs Are Source Of Truth
- `docs/current-state.md`
  - 현재 구현 상태의 요약 문서
- `docs/config.md`
  - 실제 `config/config.json`, `profiles/<name>/config.json`, `keymap.json` 기준
- `AGENTS.md`
  - 최근 완료 작업과 working agreements

## Historical / Design Docs
- `docs/menu.md`
  - 메뉴/상태머신/저지연 방향성 설계 문서
- `docs/core-loop.md`
  - 플레이 루프 초기 설계 및 데이터 흐름 설명
- `docs/latency.md`, `docs/modes.md`, `docs/gap-analysis.md`, `docs/roadmap.md`
  - 기능별 설계/분석/중장기 방향 문서

## Practical Rule
- 현재 동작을 확인할 때는 `docs/current-state.md`를 우선 봅니다.
- 오래된 설계와 현재 코드가 다를 수 있으므로, 충돌하면 현재 코드는 `docs/current-state.md`, `AGENTS.md`, `.codex-memory` 순으로 해석합니다.
