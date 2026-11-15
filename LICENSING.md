# TenRiff licensing / 라이선스 안내

Policy date: 2026-11-16. The binding English terms are in [LICENSE](LICENSE).
This document explains the scope; it is not an additional restriction.

## 한국어 안내

TenRiff는 구성요소별 라이선스를 사용합니다. 새 자체 엔진·게임 코드의 재사용은
사전 서면 허락을 받도록 하고, 스킨 제작과 외부 오픈소스 이용은 계속 허용합니다.
제한 대상 코드의 조건은 **TenRiff Source-Available License 1.0**이며, Apache나
MIT 같은 오픈소스 라이선스가 아닙니다. 공개 저장소라는 사실과 자유로운 코드
재사용 허락은 별개입니다.

### 기존 MIT 권리는 그대로입니다

공개된 기준 커밋은
[`c6f6a3a817f63f59dab13dba8a87290288cd232d`](https://github.com/10kseason/TenRiff/tree/c6f6a3a817f63f59dab13dba8a87290288cd232d)
(2.0.2)입니다. 이 커밋과 기존 MIT 배포분의 MIT 대상 부분은 계속 MIT로
이용·수정·재배포할 수 있으며, 외부 코드의 개별 조건도 그대로입니다.
**지금부터 새로 받는 사람도 합법적인 기존 MIT 사본을 MIT로 이용할 수 있습니다.**
기존 코드를 새 버전에 그대로 포함하거나 이름을 바꿔도 그 권리가 사라지지 않습니다.

이번 정책은 이후 이 조건으로 처음 배포하는 새 자체 저작물에 적용됩니다.
기존 코드와 새 코드가 섞인 파일에서는 새 제한 대상 부분에만 적용됩니다.
이번 변경은 정책 도입이며 기존 게임 코드를 새로 구현한 것이 아닙니다.
다른 곳에서 이미 MIT 등으로 공개한 코드를 가져오는 경우에도 기존 권리를 유지합니다.

### 구성요소별 적용

| 범위 | 적용 조건 |
| --- | --- |
| 새 자체 엔진·게임 코드, 테스트, 개발 도구, 빌드 스크립트 | 아래 예외를 제외하고 [LICENSE](LICENSE). 허용된 실행·열람·기여 활동 외의 재사용·수정·재배포에는 사전 서면 허락 필요 |
| `docs/`, `examples/`, `skins/`, `assets/`, `Mainmusic/`, `config/`, `models/`, `tools/skin_editor/`, `launch_skin_editor.bat`의 자체 저작물 | [MIT](LICENSE-MIT). 별도 고지가 있으면 해당 조건 우선 |
| `src/gameplay/KeyModeConverter.cpp`, `src/gameplay/KeyModeConverter.h` | 기존 Apache 2.0 유지. 게임의 다른 코드까지 Apache로 허용하는 것은 아님 |
| `external/`, `third_party/`, `nk2e/`, `nk2eplus/`와 다른 외부 유래 부분 | [서드파티 고지](THIRD_PARTY_NOTICES.md) 및 원래 MIT·Apache·BSD·LGPL 등 유지 |
| 기존 MIT 코드 및 외부 기여분 | 기존 고지와 권리 유지. 저장소 소유권은 타인의 저작권을 양도받았다는 뜻이 아님 |
| 사용자가 별도로 만든 차트·스킨·설정·호환 도구 | 작성자가 권리와 조건을 정함. TenRiff에서 사용한다는 이유만으로 이 제한 라이선스가 적용되지 않음 |

폴더 위치만으로 외부 저작물의 조건이 바뀌지는 않습니다. 코드나 자산을 다른
위치로 복사할 때도 원래 고지와 출처를 함께 유지해야 합니다. 특히 음악·차트·폰트·
모델·타 게임의 자산에 대해 TenRiff가 없는 권리를 대신 허락하지 않습니다.

### 허락 없이 가능한 것

- 공식 게임 실행과 변경하지 않은 멀티플레이 서버 운영.
- 소스 열람·개인 학습·평가와 변경하지 않은 소스의 개인용 빌드·실행.
- 게임 방송·영상·스크린샷·리뷰. 수익화도 가능하지만 곡 등 외부 콘텐츠의 권리는 별도.
- 고지와 필요한 외부 소스 제공 자료를 보존한 공식 바이너리 패키지의 무료 원형 공유.
- 본인 소유 또는 이용 허가를 받은 자료로 차트·스킨을 만들고 배포하기.
- MIT 문서·예제·스킨·에디터 및 외부 구성요소를 각각의 조건에 따라 재사용하기.
- TenRiff에 패치를 제출하기 위한 수정과 리뷰용 브랜치·PR 공개.
- GitHub 약관이 허용하는 공개 저장소 열람·포크 및 법률상 허용되는 이용.

### 새 제한 대상 코드에서 사전 허락이 필요한 것

- 다른 게임·프로그램에 코드를 복사하거나 포팅·통합하기.
- 기여 제출 범위를 넘어 수정판을 만들거나 별도 프로젝트로 배포하기.
- 허용된 경우 이외의 소스·바이너리 재배포, 판매, 재라이선스.

무료·비상업 프로젝트라도 위 재사용에는 허락이 필요합니다. 출처 표시는 허락을
대신하지 않습니다. 다만 이 요구는 기존 MIT나 외부 라이선스로 이미 허용된
이용을 제한하지 않습니다.

허락 요청은 [저장소 이슈](https://github.com/10kseason/TenRiff/issues)에
`License permission request` 제목으로 버전·파일·사용 목적·수정·배포 범위를
적어 주세요. 개인정보나 비밀은 공개하지 마세요. 관련 권리자 또는 권한을 받은
대리인의 명시적인 서면 승인이 필요하며, 답변 없음이나 이슈 종료는 승인이 아닙니다.

### 배포·기여 유지 규칙

- 배포본에는 `LICENSE`, `LICENSE-MIT`, `LICENSING.md`, `THIRD_PARTY_NOTICES.md`와
  해당 외부 구성요소의 원래 고지를 함께 포함합니다.
- FFmpeg는 LGPL 구성요소입니다. 바이너리를 배포할 때 실제 빌드에 대응하는 소스와
  필요한 빌드 자료 등을 LGPL에 맞게 제공해야 합니다. 비공개 저장소 전환이나
  `-BinaryOnly` 패키징은 이 의무를 없애지 않습니다. 현재 패키징 스크립트는 이
  옵션에서 소스 ZIP을 만들지 않으므로, 배포자는 별도의 준수 경로를 확보해야 합니다.
- `third_party/ffmpeg-7.1/source-packet/`은 기존 FFmpeg 소스·빌드 자료 위치입니다.
  외부 코드를 자체 제한 라이선스로 덮거나 LGPL상의 권리를 금지하지 않습니다.
- 새 기여자는 해당 부분에 적용되는 조건으로 제출하고 타인의 코드·자료는 출처와
  라이선스를 밝혀야 합니다. 기존 기여자의 권리는 유지되며 저작권 양도를 요구하지 않습니다.
  새 제한 대상 기여에는 공식 TenRiff에 통합·수정·배포할 수 있는 허락이 포함됩니다.
- 과거 릴리스 문서의 MIT 표기는 당시 배포 이력입니다. 과거 태그·ZIP을 소급 변경하지 않습니다.

## English summary

New original engine/game code and development code first released under this
policy require prior written permission for reuse, modification and redistribution
outside the express permissions in [LICENSE](LICENSE). Normal gameplay, unmodified
private builds, gameplay media, independent skins/charts and upstream contribution
patches are permitted. This is a source-available policy, not an open-source license.

The MIT-covered portions of the public baseline
`c6f6a3a817f63f59dab13dba8a87290288cd232d` (2.0.2), earlier MIT releases, and material
already permissively licensed elsewhere keep their original rights for all users,
including people obtaining those copies later. The policy is prospective; unchanged
old code does not become restricted, even inside newer releases or renamed files.

Project-owned documentation, examples, skins, assets, menu music, configuration,
models and the skin editor in the exact paths listed in LICENSE section 1(c)
remain MIT unless separately licensed. Third-party components retain their original
terms. These exceptions apply to embedded/copied portions as well as whole files.
Keep all notices and satisfy applicable LGPL source-delivery obligations when
distributing FFmpeg. Ownership of this repository is not ownership of every contribution.

Permission requests: <https://github.com/10kseason/TenRiff/issues>, subject
`License permission request`. State the version/files, intended use, modifications
and distribution. Only relevant rightsholders or their authorized representatives
can grant additional rights; silence is not consent.

## Maintainer verification

The policy entry point is `LICENSE`; scope/examples live here, historical and
permissive grants in `LICENSE-MIT`, and external notices in `THIRD_PARTY_NOTICES.md`.
`tools/package_release.ps1` copies all four into binary packages and includes them
in source packages. Keep all four root README language summaries consistent.
For this policy-only change, verify the diff, unchanged external license files,
the original MIT text, documentation links, and the packaging script's syntax and
license-file inventory. No engine behavior or binary release is changed.

Reference guidance: [GitHub license changes](https://opensource.guide/legal/#what-if-i-want-to-change-the-license-of-my-project),
[Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0),
[GitHub Terms](https://docs.github.com/en/site-policy/github-terms/github-terms-of-service#d-user-generated-content),
[FFmpeg licensing](https://ffmpeg.org/legal.html).
This custom policy has not received independent legal review; it makes no claim
to have audited ownership of every asset or contribution.
