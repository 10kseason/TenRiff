# TenRiff Skin Editor

[한국어](#한국어) · [English](#english) · [日本語](#日本語)

## 한국어

`Launch-Skin-Editor.cmd` 또는 `index.html`을 더블클릭하세요. 설치, 서버, 인터넷,
Node.js 없이 최신 Edge / Chrome / Firefox에서 실행되는 오프라인 편집기입니다.
모든 편집 화면은 한국어·영어·일본어를 지원하며, 오른쪽 위에서 언어를 고릅니다.

### 시작과 적용

1. **스킨 폴더 열기**로 `skin.json`이 있는 폴더 하나를 고릅니다. 이미지도 함께
   읽으므로 배경 미리보기와 ZIP 내보내기를 사용할 수 있습니다. **JSON 열기**는
   매니페스트만 읽습니다. **새 스킨**은 기본 메뉴의 외형을 유지하는 Native 스킨으로 시작합니다.
2. 왼쪽 탭에서 스킨 정보, 메뉴 배치, 색상 팔레트, 배경·이미지, Native 세부 설정,
   기존 플레이 스타일을 편집합니다. 플레이 스타일 편집은 기존 manifest 기능이며
   게임 코드나 판정·오디오 설정을 바꾸지 않습니다.
3. **다른 이름으로 저장**은 새 `skin.json`을 다운로드합니다. 원본에 자동으로 쓰지
   않습니다. **스킨 ZIP 내보내기**는 manifest와 읽어 온 이미지 모두를 한 스킨 폴더로
   묶습니다. 기존 스킨을 보존하려면 ZIP을 새 폴더에 풀어 주세요.
4. 게임의 `Options > Skins > Import Skin`으로 폴더를 가져오고 `F5 / Reload Skin`으로
   실제 화면을 확인합니다. 에디터 미리보기는 배치와 색상을 확인하는 참고 화면입니다.

### 편집 도구

- **메뉴 배치:** 1920×1080 기준 `[left, top, right, bottom]` 절대 좌표입니다.
  화면/영역을 고른 뒤 캔버스에서 드래그로 이동하고, 선택 영역 우하단 손잡이로 크기를
  바꿉니다. 숫자 입력과 8px 맞춤도 지원합니다. 기본값 사용은 JSON에서 해당 override를 제거합니다.
  Native 타이틀은 우측 버튼·좌측 가이드 배치를, 기존 스킨은 중앙 버튼 배치를 각각 사용합니다.
- **Native 세부 설정:** 카탈로그의 수치, 색상, 사각형, 이미지, 모션, 글꼴 슬롯을
  검색해 편집합니다. `lobby.renderer = "native"`에서 적용됩니다. `native.rects`는
  `[X 이동, Y 이동, 너비 증감, 높이 증감]` 상대 보정값입니다. 모두 0이면 원래 사각형을
  유지합니다. 기존 `layout`의 절대 좌표와 의미가 다릅니다. 글꼴은 해당 PC에 설치된
  글꼴 이름을 씁니다. 일부 색상과 큰 배치만 미리보기에 반영되며 모든 세부 슬롯은 게임에서 확인해야 합니다.
  게임은 모션 속도 0~4, 강도 0~2, 등장 시간 0.05~2초, 켬 값 0~1,
  글자 크기 8~180 등 실제 동작 범위로 수치를 제한합니다.
- **플레이 스타일:** 공통값 또는 `1k`~`16k`, `7+1`별 덮어쓰기를 선택합니다.
  노트 높이 비율은 `0.5`~`4.0` (50%~400%), 기본값은 `1.0` (100%)입니다.
  레인별 배열은 JSON 형식으로 입력합니다. 이미지 배열 예: `["a.png", "b.png"]`.
  이미지 경로 패턴 `{lane}`, `{index}`, `{index:02}`를 보존합니다.
- **JSON 편집:** 적용 전까지 별도 초안입니다. 적용하거나 되돌리기 전에는 다른 도구로
  문서를 수정하지 않습니다. 알 수 없는 키는 그대로 보존하고 경고로 표시합니다.
- **검증:** 필수 메타데이터, 타입, 숫자 범위, 좌표 순서, 이미지 경로, Native 슬롯 이름을
  검사합니다. 오류가 있으면 저장하지 않으며, 알 수 없는 키의 경고만으로 저장을 막지는 않습니다.

`Ctrl+S`: 저장 · `Ctrl+O`: 열기 · `Ctrl+Z`: 실행 취소 · `Ctrl+Shift+Z / Ctrl+Y`: 다시 실행.
입력칸 안에서는 브라우저의 텍스트 실행 취소를 사용합니다. 캔버스에 포커스가 있을 때 방향키로
영역을 1px, Shift+방향키로 10px 이동합니다. 실행 취소 기록은 최대 100회입니다.

### 파일과 한계

이미지는 PNG/JPG/JPEG/BMP만 지원합니다. 모든 경로는 스킨 폴더 내부의 상대 경로여야 합니다.
JSON은 8MB, 이미지 한 개는 64MB, ZIP 전체는 512MB까지입니다. ZIP에 참조 이미지가 빠져 있으면
내보내기를 막고 목록을 표시합니다. 미지정 슬롯의 기본 이미지 자동 감지를 위해 폴더에서 읽은
이미지는 참조 여부와 상관없이 모두 ZIP에 포함합니다. JSON 이외의 문서나 글꼴 파일은 복사하지 않습니다.

저장 완료 표시는 브라우저에 새 다운로드를 전달했다는 뜻입니다. 실제 저장 폴더나 브라우저의
다운로드 취소 여부는 편집기가 읽을 수 없습니다. 다운로드 후 파일을 확인하세요.
페이지를 닫으면 편집 내용과 실행 취소 기록은 사라집니다. 자동 저장은 하지 않습니다.
언어 선택만 브라우저 로컬 저장소에 보관하며 이미지나 JSON은 외부로 전송하지 않습니다.

## English

Double-click `Launch-Skin-Editor.cmd` or `index.html`. This offline editor runs in a
current Edge, Chrome, or Firefox browser without installation, a server, Node.js,
or internet access. The entire UI supports Korean, English, and Japanese.

### Open, edit, and apply

1. Choose **Open skin folder** and select one folder containing `skin.json`. This
   also loads images for previews and ZIP export. **Open JSON** loads only the
   manifest. **New skin** starts with the native menu presentation.
2. Use the tabs for metadata, layout, palette, backgrounds, native details, and
   existing play-style options. Play-style editing changes supported manifest
   settings; it does not change gameplay code, judgement, or audio settings.
3. **Save as** downloads a new `skin.json`; it never writes directly to the
   original file. **Export skin ZIP** bundles the manifest and all loaded images.
   Extract the ZIP to a new folder to preserve the previous skin.
4. Import that folder through `Options > Skins > Import Skin`, then press
   `F5 / Reload Skin` in TenRiff. The browser preview is a layout/color reference,
   not a pixel-perfect game renderer.

### Controls

- **Menu layout:** absolute `[left, top, right, bottom]` in a 1920×1080 coordinate
  space. Drag regions to move them; drag the bottom-right handle to resize.
  Numeric entry and 8 px snapping are supported. **Use default** removes the override.
  Native title previews use the right-hand buttons and left-hand guide; legacy
  skins retain the centered button stack.
- **Native details:** search the catalog of metrics, colors, rectangles, images,
  motion, and fonts. Requires `lobby.renderer = "native"`. `native.rects` uses
  relative `[X offset, Y offset, width change, height change]`; zero preserves
  the original rectangle. Existing `layout` fields use absolute coordinates.
  Fonts are installed family names. Preview shows only selected colors and major
  layout; verify detailed slots in the game.
  Runtime bounds include motion speed 0–4, intensity 0–2, entry time 0.05–2 seconds,
  enabled 0–1, and font size 8–180.
- **Play style:** select common fields or a `1k`–`16k` / `7+1` override.
  Note height ratios range from `0.5` to `4.0` (50%–400%), default `1.0` (100%). Enter arrays
  as JSON, such as `["a.png", "b.png"]`. Lane patterns `{lane}`, `{index}`, and
  `{index:02}` are preserved.
- **JSON editor:** changes remain a draft until applied. Apply or discard the
  draft before editing with other controls. Unknown keys survive round trips
  and are reported as warnings.
- Validation checks metadata, types, ranges, rectangle ordering, relative paths,
  and native slot names. Errors block saving; unknown-key warnings do not.

Shortcuts: `Ctrl+S` save, `Ctrl+O` open, `Ctrl+Z` undo, `Ctrl+Shift+Z / Ctrl+Y` redo.
Text fields keep browser text undo. With the canvas focused, arrow keys move a
region by 1 px, or 10 px with Shift. History retains up to 100 edits.

### Files and limits

Supported images: PNG/JPG/JPEG/BMP. Paths must stay inside the skin folder.
Limits: 8 MB JSON, 64 MB per image, 512 MB per ZIP. Missing referenced images block
ZIP export. All loaded images are included, even if unreferenced, to preserve
standard-filename auto-detection. Other documents and font files are not copied.

The saved indicator means the browser received a new download; the editor cannot
verify the chosen download folder or detect a cancelled download. Check the file
after downloading. Closing the page loses edits and undo history; there is no
autosave. Only the UI language is retained in browser local storage. No images
or manifests are uploaded.

## 日本語

`Launch-Skin-Editor.cmd` または `index.html` をダブルクリックしてください。
インストール、サーバー、Node.js、インターネット接続は不要です。現在の Edge / Chrome /
Firefox で動作します。画面全体を韓国語・英語・日本語に切り替えられます。

### 開く・編集する・適用する

1. **スキンフォルダーを開く**で `skin.json` のあるフォルダーを 1 個選択します。
   プレビューと ZIP 同梱用の画像も読み込みます。**JSON を開く**では設定だけを読み込みます。
   **新規スキン**は標準メニューの外観を維持する Native スキンから始まります。
2. 左のタブで情報、配置、色、背景、Native 詳細、既存のプレイスタイル設定を編集します。
   プレイスタイルは対応済みの manifest 設定を編集し、ゲームコード、判定、音声設定は変更しません。
3. **名前を付けて保存**は新しい `skin.json` をダウンロードします。元ファイルには直接
   書き込みません。**スキン ZIP を書き出す**は設定と読み込んだ画像を同梱します。
   元のスキンを残すため、ZIP は新しいフォルダーに展開してください。
4. TenRiff の `Options > Skins > Import Skin` で取り込み、`F5 / Reload Skin` で確認します。
   ブラウザーのプレビューは配置と色の参考表示です。実際の描画はゲームで確認してください。

### 編集操作

- **メニュー配置:** 1920×1080 基準の絶対座標 `[左、上、右、下]` です。領域をドラッグで
  移動し、右下のハンドルでサイズを変えます。数値入力と 8px スナップも使えます。
  **既定値に戻す**は JSON から指定値を削除します。
  Native タイトルは右側のボタンと左側のガイド、従来スキンは中央のボタン配置を表示します。
- **Native 詳細設定:** 数値、色、矩形、画像、モーション、フォントのスロットを検索できます。
  `lobby.renderer = "native"` で有効になります。`native.rects` は
  `[X 移動、Y 移動、幅の増減、高さの増減]` の相対補正です。0 は元の値を維持します。
  既存の `layout` は絶対座標です。フォントは PC にインストール済みの名前を指定します。
  プレビューは一部の色と大きな配置のみ反映するので、詳細はゲームで確認してください。
  ゲームはモーション速度 0〜4、強度 0〜2、登場時間 0.05〜2 秒、有効値 0〜1、
  文字サイズ 8〜180 などの実行時範囲に制限します。
- **プレイスタイル:** 共通値か `1k`〜`16k` / `7+1` の指定値を選びます。
  高さの比率は `0.5`〜`4.0` (50%〜400%)、既定値は `1.0` (100%) です。配列は
  `["a.png", "b.png"]` のような JSON で入力します。`{lane}`、`{index}`、`{index:02}` を保持します。
- **JSON 編集:** 適用前は別の下書きです。他の操作で変更する前に適用または破棄してください。
  不明なキーも保存時に保持し、警告として表示します。
- メタデータ、型、数値範囲、座標順序、画像パス、Native キーを検証します。
  エラーがあると保存を止めますが、不明キーの警告だけでは保存を止めません。

`Ctrl+S`: 保存、`Ctrl+O`: 開く、`Ctrl+Z`: 元に戻す、`Ctrl+Shift+Z / Ctrl+Y`: やり直す。
入力欄内ではブラウザーの文字編集履歴を使います。キャンバスにフォーカスがある場合は
矢印キーで 1px、Shift と併用すると 10px 移動します。履歴は 100 回まで保持します。

### ファイルと制限

PNG/JPG/JPEG/BMP の画像に対応します。パスはスキンフォルダー内の相対パスのみです。
上限は JSON 8MB、画像 1 枚 64MB、ZIP 全体 512MB です。参照画像が不足していると
ZIP を作成しません。標準ファイル名での自動検出を維持するため、読み込んだ画像は
未参照でもすべて同梱します。その他の文書やフォントファイルはコピーしません。

保存表示はブラウザーへダウンロードを渡したことを示します。保存先やダウンロードの
キャンセルは検知できないため、保存後にファイルを確認してください。ページを閉じると
編集内容と履歴は失われます。自動保存はありません。言語のみローカルストレージに保存し、
画像や JSON を外部に送信しません。

## Maintainer checks

The runtime files are `index.html`, `style.css`, `catalog.js`, `native-catalog.js`,
`i18n.js`, `core.js`, `gameplay.js`, `zip.js`, and `app.js`. Ship them together with the launcher
and this README. There are no npm packages or build steps.

After changing the source schema or native preset:

```powershell
python tools/skin_editor/sync_catalog.py
node tools/skin_editor/test_core.cjs
```

The direct Node command avoids worker-process restrictions in Windows sandboxes.
`node --test tools/skin_editor/test_core.cjs` also works where child processes are allowed.
The native renderer owns `native-catalog.json` and its corresponding script;
regenerate them with the renderer catalog generator after native slot changes.

Tests cover real bundled manifests, unknown-key preservation, undo/redo, Unicode,
prototype-shaped JSON keys, schema boundaries, relative assets, key modes,
all native catalog defaults, trilingual labels, ZIP CRC/UTF-8 headers, and script syntax.
Browser and native game checks remain separate from these document-level tests.

## Luma Keys gameplay editing

The instrument tab edits native gameplay metrics, colors, motion, HUD rectangle
adjustments, fonts, and the five layered vector sprites. Select common settings or
a 4K–16K mode; each native category inherits unmodified common slots. Sprite arrays
replace as a whole, and an empty array intentionally hides that sprite.

The new default includes its vector artwork in `skin.json`, so it can be exported
without loading separate image files. PNG replacements still require loading the
referenced files. Use the client build that supports `gameplay.renderer: native`.
Older clients ignore the new presentation extension.

Sprite layers use a 128×256 coordinate space for keys and 128×32 for notes.
Mix 0 uses the lane color and mix 1 uses the specified layer color. The canvas uses
the same layers and default palettes as the client, with an approximate HUD and
synthetic chart. It is a visual editor, not an audio/input latency test.
