# 옵션 아이콘 색상

1.8.2 수정. 옵션 아이콘을 회색에서 선명한 기능별 색으로 바꿨습니다.
미선택 아이콘도 불투명하게 그려 색을 유지합니다. 카드 배경, 값 글자와
곡 선택 로비의 올블랙 설정은 그대로 사용합니다. 카드 모서리는 7px에서
12px로, 테두리는 기본 1px/선택 1.35px에서 1.5px/2.25px로 바꿨습니다.
선택 테두리는 해당 아이콘 색을 따릅니다. 수치는 1920×1080 논리 좌표 기준이며
실제 화면 크기에 따라 함께 확대·축소합니다.

| 항목 | 색 | 스킨 색상 키 |
|---|---|---|
| 키 수 | 노랑 `#FFD600` | `options.icon.key_mode` |
| 키 설정 | 시안 `#00C8FF` | `options.icon.keymap` |
| 스킨 | 보라 `#A64DFF` | `options.icon.skin` |
| 그래픽 | 파랑 `#3C6FFF` | `options.icon.graphics` |
| 오디오 | 빨강 `#FF3B30` | `options.icon.audio` |
| 입력 | 초록 `#00D66F` | `options.icon.input` |
| 타이밍 조정 | 주황 `#FF7900` | `options.icon.latency` |
| 프로필 | 분홍 `#FF36B3` | `options.icon.profile` |
| MOD | 라임 `#A9E600` | `options.icon.mode` |
| 키 테스트 | 청록 `#00D7BD` | `options.icon.key_test` |

기본 벡터는 위 `native.colors` 키를 사용합니다. 스킨 편집기 카탈로그도 같은
렌더러 기본값에서 생성합니다. 다른 화면의 벡터 색상은 바꾸지 않습니다.
TenRiff Studio PNG는 이 색상으로 내보내므로 이미지를 사용하는 화면에도 같은
색으로 표시됩니다. 사용자가 공급한 PNG는 원래 픽셀 색을 유지하며 벡터 색상
슬롯으로 다시 칠하지 않습니다. PNG 색을 바꾸려면 파일을 교체합니다.

```text
python skins/TenRiff_Studio/generate.py --check
python tools/native_skin_catalog.py --check
node tools/skin_editor/test_core.cjs
```

네이티브 옵션 화면과 Studio PNG 화면을 실제 D3D11/D2D 미리보기로 확인합니다.
웹·오프라인 에디터의 옵션 미리보기도 같은 벡터와 기본값을 사용합니다. Native 세부 설정에서 카드를 클릭하면 선택 테두리를 바꿔 볼 수 있습니다.
[1.8.2 검증 범위](release-1.8.2-gate.md)를 참고하세요.

## FAST/SLOW 막대

이전에는 현재 FAST/SLOW 피드백이 없어도 판정 기록이 남아 있으면 막대를
표시했습니다. 현재 FAST/SLOW 글자가 표시되는 동안에만 막대를 함께 그립니다.
PG, 밀리초 표시가 0으로 반올림되는 판정, 피드백 종료, 표시 옵션 끄기에서는
막대도 숨깁니다. 플레이와 고스트 모두 같은 표시 경로를 사용합니다.
판정 기록이나 점수 계산은 바꾸지 않습니다.
