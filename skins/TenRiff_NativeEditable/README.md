# TenRiff Native Editable

## 한국어

현대적인 기본 메뉴와 Luma Keys 디지털 건반을 시작점으로 삼는 편집용 스킨입니다.
메뉴는 어두운 네이비 바탕, 시안·보라 강조색과 짧은 등장·선택 애니메이션을 사용합니다.
인게임은 깊이감 있는 건반, 짧은 신호 펄스, 밝은 노트와 어두운 중심의 롱노트 레일로
4~16키를 지원합니다. 메뉴 텍스트와 밀집 패턴의 대비를 우선합니다.

1. 동봉된 `tools/skin_editor/index.html`을 브라우저로 엽니다.
2. 이 폴더의 `skin.json`을 열고 Native 메뉴와 인게임 항목을 편집합니다.
3. **다른 이름으로 저장**한 사본을 별도 폴더에 둡니다. 이미지를 바꿨다면 참조한
   PNG/JPG/BMP 파일도 그 폴더 안에 상대 경로 그대로 둡니다.
4. 게임의 `Options > Skins > Import Skin`으로 사본을 가져옵니다.
5. 가져온 폴더를 추가로 수정한 뒤에는 `F5 / Reload Skin`으로 반영합니다.

`native.rects`는 `[x 이동, y 이동, 폭 증감, 높이 증감]`입니다. 기존 `layout`의
절대 사각형 좌표와 구분해야 합니다. 비워 둔 항목은 현재 기본값을 따릅니다.
인게임은 `gameplay.renderer: native`로 활성화하며, `gameplay.native`에 Luma Keys의
도형·색·크기·움직임 기본값을 담습니다. 건반 도형은 128×256, 노트 도형은 128×32
기준입니다. 도형 배열을 지우면 기본값으로 돌아가고, `[]`로 지정하면 숨깁니다.
모든 키 수에서 공통 도형을 사용하고 `gameplay.modes`에서 모드별로 조정할 수 있습니다.

지원 슬롯은 동봉 에디터의 카탈로그, 포맷은 `docs/skin-format.md`를 참고하세요.
SVG 원본은 `assets/native-menu/`와 `assets/native-gameplay/luma-keys/`에 제공됩니다.
배포본에는 `tools/skin_editor/vector-assets/`와 `vector-gameplay/luma-keys/`로 포함됩니다.
외부 교체 이미지는 PNG/JPG/BMP를 사용합니다. 판정·오디오·입력은 스킨에 포함되지 않습니다.

## English

An editable starting point for the modern default menu and the Luma Keys digital
keyboard. The menu uses dark navy, cyan and violet accents, short entrance and focus
animations, and readable text. Gameplay uses sculpted keys, compact signal pulses,
bright notes and dark-centred hold rails for every key count from 4 through 16.

Open the bundled `tools/skin_editor/index.html`, load this `skin.json`, and edit the
Native menu and gameplay fields. Save a separate copy into a new skin folder, include any referenced
PNG/JPG/BMP files using relative paths, then use **Options > Skins > Import Skin**.
Further changes to the imported folder take effect with **F5 / Reload Skin**.

`native.rects` contains additive `[dx, dy, dwidth, dheight]` adjustments. The older
`layout` section still uses absolute rectangles. Empty overrides retain the current
defaults. Supported slots are listed in the editor's catalogs.

`gameplay.renderer: native` selects the native keyboard renderer. Its complete Luma
Keys visual defaults are in `gameplay.native`. Key layers use a 128×256 canvas; note
layers use 128×32. Omitted sprite arrays use built-in art; an explicit `[]` hides that
sprite. Use `gameplay.modes` for key-count-specific changes. Skin values do not change
judgement timing, audio or input handling.

## 日本語

標準のモダンなメニューと Luma Keys のデジタル鍵盤を編集するためのスキンです。
メニューは濃紺の背景、シアンと紫のアクセント、短い登場・選択アニメーションを使います。
ゲームプレイは奥行きのある鍵盤、小さな発光パルス、明るいノートと中央が暗い
ロングノートで、4～16キーの全キー数に対応します。

同梱の `tools/skin_editor/index.html` を開き、この `skin.json` を読み込みます。
Native メニューとゲームプレイの項目を編集し、別のフォルダーに名前を付けて保存してください。
画像を指定した場合は PNG/JPG/BMP も相対パスどおりに配置し、ゲームの
**Options > Skins > Import Skin** から取り込みます。取り込み後の変更は
**F5 / Reload Skin** で反映されます。

`native.rects` は `[横移動, 縦移動, 幅の増減, 高さの増減]` です。
従来の `layout` は絶対座標を使います。空の項目は標準値を使います。
対応する項目はエディターのカタログに記載されています。

`gameplay.renderer: native` で標準鍵盤の描画を選択します。Luma Keys の図形・色・
大きさ・動きの初期値は `gameplay.native` に含まれます。鍵盤の図形は128×256、
ノートは128×32の座標です。配列を省略すると標準図形を使い、`[]` は非表示を意味します。
キー数ごとの調整には `gameplay.modes` を使います。判定・音声・入力処理は変わりません。

## Asset provenance

This manifest, the native menu artwork and the Luma Keys vector layers were created for TenRiff. No
third-party skin or commercial rhythm-game artwork is included. This preset uses
the executable's cached native vector renderer. Editable gameplay layers are embedded
in the manifest; image files are not duplicated. Sources use the repository's MIT license.
