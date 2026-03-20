# TenRiff Docs Map

Language: Korean | [English](README.en.md) | [简体中文](README.zh-CN.md)

루트 [`README.md`](../README.md)를 먼저 읽었다면, 이 문서는 그 다음 단계의 상세 문서 인덱스입니다. 설계 문서와 현재 상태 문서를 같이 담고 있으므로, 빠르게 맥락을 잡아야 할 때는 아래 순서로 읽는 것이 가장 효율적입니다.

## Recommended Reading Order
1. `docs/current-state.md`
   - 현재 제품 상태, 핵심 서브시스템, 검증된 명령, 남은 수동 검증 항목
2. `docs/ui-audit-checklist.md`
   - renderer layout 변경 뒤 반드시 다시 돌려야 하는 UI 수동 검증 매트릭스
3. `docs/baseline-0.8.0.md`
   - 현재 작업을 어디서부터 쌓아야 하는지 정하는 `0.8.0` 기준선 문서
4. `docs/gameplay-guide.md`
   - 실제 플레이 기준의 시작 방법, 곡 선택, 조작, HUD, 판정, 결과 화면 안내
5. `docs/config.md`
   - 실제 설정/프로필/키맵 구조
6. `docs/menu.md`
   - 메뉴/상태머신/곡 선택 흐름
7. `docs/core-loop.md`
   - 플레이 루프와 데이터 흐름
8. `docs/roadmap.md`
   - 중장기 작업 방향
9. `docs/developer-extension-guide.md`
   - 새 mode/mod, UI row, runtime migration, replay/result, 테스트를 어디에 추가해야 하는지 설명하는 개발자 문서

## Which Docs Are Source Of Truth
- `docs/current-state.md`
  - 현재 구현 상태의 요약 문서
- `docs/baseline-0.8.0.md`
  - 후속 작업이 유지해야 하는 `0.8.0` 기준선 문서
- `docs/config.md`
  - 실제 `config/config.json`, `profiles/<name>/config.json`, `keymap.json` 기준

## Historical / Design Docs
- `docs/menu.md`
  - 메뉴/상태머신/저지연 방향성 설계 문서
- `docs/core-loop.md`
  - 플레이 루프 초기 설계 및 데이터 흐름 설명
- `docs/latency.md`, `docs/modes.md`, `docs/gap-analysis.md`, `docs/roadmap.md`
  - 기능별 설계/분석/중장기 방향 문서
- `docs/developer-extension-guide.md`
  - 현재 코드 기준 유지보수/확장 작업 절차 문서

## Translation Coverage
- 루트 `README.md`는 `README.en.md`, `README.zh-CN.md` 번역본을 가집니다.
- `docs/` 안의 주요 문서는 원본 옆에 `.en.md`, `.zh-CN.md` suffix 파일로 번역본을 둡니다.
- 번역 문서와 원문이 충돌하면, 현재 동작 기준은 여전히 원문 `docs/current-state.md`, `docs/config.md`, 실제 코드 순으로 판단합니다.

## Practical Rule
- 현재 동작을 확인할 때는 `docs/current-state.md`를 우선 봅니다.
- 어떤 기준선 위에서 작업을 쌓는지 정할 때는 `docs/baseline-0.8.0.md`를 같이 봅니다.
- 오래된 설계와 현재 코드가 다를 수 있으므로, 충돌하면 현재 코드는 `docs/current-state.md`, `docs/config.md` 순으로 해석합니다.
