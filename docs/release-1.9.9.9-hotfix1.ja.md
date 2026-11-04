# TenRiff 1.9.9.9-hotfix1 — OGG セキュリティ修正・共通 BGA・スキンエディター

- stb_vorbis の comment サイズ・件数、乗算・アラインメント、codebook 制限、部分割り当て失敗時の解放を修正しました。probe と decode は同じ修正済みパーサーを使用します。[upstream SHA と依存関係](../external/stb_vorbis.TENRIFF.md)を記録し、ライセンスを保持します。
- オプション > グラフィックスで **共通 BGA 動画**を選び、**共通 BGA** をオンにすると、全曲の BGA とオーバーレイを同じ無音ループ動画で置き換えます。BGA 表示オフが優先です。初期値は OFF・空のパスです。[使い方](common-bga-20261104.ja.md)。
- オフラインスキンエディターのバージョン、グラフィックス 14 項目、選曲・リザルト例と韓国語・英語・日本語の説明を更新しました。共通 BGA はゲームのプロフィール設定であり、skin.json v1 には保存しません。ブラウザー表示は例なので、ゲームで確認してください。

Media Foundation fallback を保持します。この PC には Opus-in-OGG ハンドラーがなく、実際の Opus 再生は未検証です。通常の mono/stereo Vorbis、複数 sample rate、65 秒音源の PCM は修正前後で一致しました。極端に大きい codebook は upstream の上限で拒否します。

Windows ZIP を新しいフォルダーに展開して `launch_win.bat` を実行してください。`tools/skin_editor/Launch-Skin-Editor.cmd` でオフラインエディターを開きます。既存の環境・プロフィール・記録を保持してください。固定基準は **1.9.6** のままです。判定・リプレイ・オンライン記録・音声タイミングの変更や Web/DB 配信はありません。

[Verification / 검증](release-1.9.9.9-hotfix1-gate.md)
