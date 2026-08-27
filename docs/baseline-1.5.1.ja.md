# TenRiff 1.5.1 Fixed Stable Baseline

この文書は `1.5.1` release で TenRiff の stable contract を固定する。後続作業は明示的な互換性変更と migration なしに以下を破壊しない。旧 baseline 文書は履歴として残すが、現在の判断ではこの文書、実コード、`docs/current-state.ja.md` を優先する。

## Release Identity

- 固定 baseline line: `1.5.1`
- release 名: `fixed stable baseline`
- 基準 platform: Windows GUI
- product surface: BMS family 専用（`.bms/.bme/.bml/.pms`）
- 対応 self-hosted server: `TenRiff Server v1.1.0`
- 正式 asset: runtime ZIP、public source ZIP、SHA-256 manifest

## Stable Contract

- 基本 flow は `Title -> Song Select -> Gameplay -> Result`。online service が失敗しても local record は保持する。
- BMS parsing/timing、long note、landmine、keysound、replay v3、deterministic replay verification は baseline behavior である。
- gameplay input timing は audio playback head を基準にする。RawInput を優先し、bound-key polling shadow と Polling fallback を維持するが、保存 backend 設定は書き換えない。
- NK3 は P64 と host beam safety solver を authoritative path とし、generalized MLP は非10K source を10Kへ変換する場合だけ使う。
- Windows package は `Mainmusic/`、bundled skin、NK3 runtime/model を含み、Songs と standalone BMS key-converter executable は含めない。

## Account And Global Chat

- `F10` は account 画面を開き、login/register 前に main server または user 指定 private API server を選択する。
- password は平文保存しない。server は unique salt と PBKDF2-HMAC-SHA256 600,000回を使い、bearer token も hash で保存する。
- client の保存 session は Windows DPAPI で保護する。ID/password recovery は baseline 必須機能ではない。
- password 欄は `Ctrl+V` に対応し、前後の空白を保持したまま常に mask 表示する。
- `F8` は menu、gameplay、result のどこでも同じ server-wide global chat overlay を開き、chat input を gameplay/menu command に流さない。
- `/np` は現在の曲名と artist を送る。chat URL は animation 付き警告と明示承認後にだけ開く。
- login 中の multiplayer room 検索は選択した main/private server の認証済み一覧を使う。logout 中は LAN 検索を維持する。

## Ranked BMS

- bulk song indexing は local search 専用で、ranked catalog を事前登録しない。
- 新しい BMS は実際の result submission 時に登録候補となり、challenge-bound replay の再現に成功した record だけ leaderboard に入る。
- osu 派生、autoplay/assist、不完全、再現不能な submission は fail-closed で拒否する。
- administrator は BMS SHA-256 exclusion list で登録と leaderboard 表示を停止できる。
- client score claim は信頼せず、exact chart bytes と external verifier の結果を使う。

## Network And Security

- game server: `27301/TCP`
- origin API: `127.0.0.1:27302/TCP`。LAN/WAN へ直接公開しない。
- public HTTPS gateway: `27303/TCP`。HTTP/3 利用時は `27303/UDP` も使う。
- `27304~27305` は予約。default host mapping は `27300`、`80`、`443` を公開しない。
- remote account/chat/ranking は信頼できる HTTPS certificate を必須とし、HTTP は loopback development のみ許可する。

## Compatibility Rule

- 後続変更は `1.5.1 fixed stable baseline` への additive change を基本とする。
- replay、score、chart identity、account storage、API、port の変更には versioned migration と cross-compatibility test を追加する。
- Git tag `1.5.1` を不変 baseline identifier とし、公開後の tag と asset は差し替えない。

## Companion Docs

- `docs/current-state.ja.md`
- `docs/config.ja.md`
- `docs/gameplay-guide.ja.md`
- `docs/multiplayer.md`
- `docs/release-1.5.1-gate.md`
