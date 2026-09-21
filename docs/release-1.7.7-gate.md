# TenRiff 1.7.7 Release Gate

## Scope

1.7.7 is a Windows x64 client release and public source refresh. The source bundle contains client code and verifier/build material only. It does not contain ranking-server code or deployment configuration, user profiles, logs, replay/result exports, connection files, upload tokens, or private model checkpoints. `Dockerfile.verifier` is retained as a verifier-only build recipe and does not deploy the ranking service.

## Requested changes

- BMS Editor `P` starts real gameplay at the current cursor section. The edited BMS is written to a temporary file beside the source chart so relative keysounds remain valid.
- Practice playback enables no-fail for that session only. Practice runs skip ranked and Sites submission.
- `T` cycles silent placement, note movement, and note removal tools.
- `O` and `Ctrl+O` remain offline previews; Song Select preview ownership cannot stop editor preview audio.

## Verification

| Check | Result | Evidence |
| --- | --- | --- |
| Release `tenriff` build | PASS | `build-release/Release/TenRiff.exe` |
| BMS parser/editor/preview tests | PASS for the requested cases | `bms_parser_tests.exe` output |
| 10K2S layout regression | PASS | `parser ignores empty 14 plus 2 template channels when detecting 10K2S` |
| Practice chart trim regression | PASS | `practice chart trim starts notes and audio at the editor cursor` |
| Full test executable | PASS | `bms_parser_tests.exe` completed with no failures; Unicode profile-path replay/result export regression included |
| Credential-shaped literal scan | PASS | no `tr_` upload key, bearer token, API key, or private key literal found |
| Private artifact inventory | PASS | no `profiles/`, `logs/`, `.dpapi`, `.pem`, `.key`, or build output in source bundle |

The replay/result export regression was fixed in 1.7.7 by decoding the UTF-8 profile directory with `std::filesystem::u8path`; the test now exercises a Korean/Japanese temporary path and verifies both files are written.

## Privacy boundary

The public bundle may contain client-side Sites/official-ranking protocol code and documented public endpoint names, but it contains no account connection data or server-side implementation. The source-package audit must be repeated against the final archive before any public upload.
