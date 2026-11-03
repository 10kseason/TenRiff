# TenRiff 1.9.9.9 — 選曲・記録フィードバック・リザルト背景動画

2026-11-03

- 提供された `continue_result_bga_ko_1080p60.mp4` を標準リザルト背景に使用します。1920×1080・60fps・60秒の無音ループで、復元済みの配置・指標・書体を維持します。背景の文字を低い不透明度で合成し、ユーザー背景を優先、モーション軽減時は停止、画面離脱/最小化でデコーダーを解放します。
- 選曲は10行と既存EZ/NM/HD/EX CLEARバッジを使用し、行の点数を置き換えます。未プレイは空欄、フォルダー曲数と右側MY BESTは維持。BPM/キー数/Lv/ノート数/判定難易度と実際のNPS MIN/AVG/MAX、12秒1080p30背景を追加し、詳細精度はロビーのみ非表示にします。
- NPSは原譜面のTAP/LN開始を空白の1秒区間も含めて集計し、AVGは算術平均、グラフのみ最大96区間のピークです。キャンセル可能な非同期計算で、試聴OFFでも待機中に反映します。固定分岐のため実際のランダム分岐と異なる場合があります。
- メイン/クリア/失敗BGMとボタン/オプション/スライダー/開始/ドロップ/読込完了音を選択できます(MP3/OGG/WAV/FLAC)。ファイル選択のスレッド初期化衝突を修正。OptionsはMain Menu 3、Clearは旧Main Menu 5、Main Menu 4は除外します。
- 点数が1点以上向上すると1.2秒の効果とNEW RECORDを表示し、公式BESTはクリア優先保存を維持します。クリアのみの向上は点数更新扱いにしません。ランクアップ/FULL COMBO効果とゲージ降格時の連続履歴を含みます。
- 任意の移動可能HUDは互換BESTリプレイの同時刻点数と比較します。S/S+/S++/S+++は95/97/98/99%精度との差(%p)で、互換BEST不在は`--`、初期OFFです。有効な一時停止ローカルBESTの除外と比較値のみ変化時の更新を修正します。
- フォルダー曲数/左右移動、10ロゴ、入力バックエンド表示の非表示、長い記録タイトル、30フレーム・1秒トランジションを含みます。リザルトはHTML変更前に復元した配置を維持します。

[Release/downloads](https://github.com/10kseason/TenRiff/releases/tag/1.9.9.9) · [Validation](release-1.9.9.9-gate.md)

Extract into a new folder and run `launch_win.bat`. Keep existing installations, profiles and records. Existing scoring/judgement and R13/R14 replay/online contracts remain compatible. No website or database deployment. Fixed baseline: **1.9.6**. Physical input/listening, arbitrary skins/charts, long play and other PCs remain unverified.
