# TenRiff 2.0.0 release verification

한국어: 최종 로컬 Release 검사 결과는 아래와 같습니다. 커밋 후 수행하는 소스 ZIP 독립 빌드, CI, 공개 파일 재다운로드 결과는 [2.0.0 릴리스](https://github.com/10kseason/TenRiff/releases/tag/2.0.0)에 기록합니다. 공개 전 모든 게시 게이트를 통과해야 합니다.

English: Final local Release results are below. The independent source-ZIP build, CI and public-download checks run after the commit and are recorded on the [2.0.0 release](https://github.com/10kseason/TenRiff/releases/tag/2.0.0). Every publication gate must pass before release.

日本語：最終ローカルRelease検証結果を以下に記載します。コミット後のソースZIP独立ビルド、CI、公開ファイル再取得の結果は[2.0.0リリース](https://github.com/10kseason/TenRiff/releases/tag/2.0.0)に記録します。公開前に全ゲートの合格が必要です。

简体中文：以下为最终本地Release验证结果。提交之后的源码ZIP独立构建、CI和公开文件重新下载结果记录于[2.0.0发布页](https://github.com/10kseason/TenRiff/releases/tag/2.0.0)。发布前必须通过全部关卡。

| Local check | Result |
|---|---|
| Windows Release executable/tools build | Passed |
| CTest | 4/4; 1593 cases, 0 integration skips |
| Native MenuApp/game-session smoke | 774/774; 0 failures |
| Actual hosted Worker + two C++ clients | Passed; Korean/ASCII chat both directions |
| Multiplayer / leaderboard server suites | 44/44; 87/87 |
| Offline skin editor source | 41/41 |
| Four-language Home + Song Select/Options captures | 54 geometry checks; 0 failures |
| Privacy / default / historical engine checks | Passed; repeated on final staged source and archives |

Initial failures and resolution: preview CLI parsing crossed MSVC's nesting limit; its new option was moved before the old chain. The new profile-card test initially sent an invalid default index (-1); it now tests both rejection and the real index (0). Both corrected checks passed. Rendering did not receive OS foreground focus, so geometry checks do not claim physical mouse dispatch. The native app smoke uses its documented internal dispatch and synthetic chart/profile fixtures.

Publication gates: source ZIP must exactly match the committed Git blobs, build without a Git directory, and pass CTest/native checks; runtime ZIP must match tested binaries, pass CRC/SHA-256, editor checks and privacy inventory; PR/main CI must pass; tag must point to the merged commit; draft and anonymous public downloads must match local assets. Existing releases/tags/assets must remain unchanged. Only 18 allowlisted NK3 models are packaged; no BGA upscaler weights or personal runtime data.

Scope: EX Score = 2×P-Great + Great, account-only hosted rooms, room chat, Library/Session Mix fixes and constellation UI. Home text is exactly `Tenriff BMS Rhythm Game Client`; its subtitle is removed, operational guide retained. The final4 compatibility reference and historical replay/direct-TCP semantics remain. After 2.0.0, maintenance is bug-fix-only.

Limits: client_submitted scores are not server-verified replays. Real-account internet matches, physical Windows IME timing/input, listening, long play, other PCs, arbitrary skins/codecs and performance/latency/cost remain unverified. The reported low-end laptop frame-time issue is unresolved.
