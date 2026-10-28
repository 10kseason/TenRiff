> **履歴文書 — 1.9.9.5以前のローカル候補:** 下記の動作・未公開状態・検査件数は当時の候補の記録です。候補検証は新リリース検証ではありません。現在の範囲と配色は[1.9.9.5](release-1.9.9.5.ja.md)、新しい検証状態は[公開リリース検証表](release-1.9.9.5-gate.md)を参照してください。

# 画面共有バッファ点検・BGA停止復帰（ローカル候補）

1.9.9.3と前候補のシンプルLN・EX6色・ALL COMBO変更を継承します。公開版と一般基準1.9.6は維持します。

HUDは最新スナップショット一つを保持し、ノート表示は最新オーディオ基準点から計算します。Present中はHUD/判定ロックを保持せず、描画期限の遅れも蓄積しません。flip-discard・バッファ2個・最大1フレーム待ちは以前から存在し、FPS/VSync/Presentフラグは変更しません。

WASAPIは固定デバイスバッファの空き部分だけを書き込み、無制限のPCM待ち行列はありません。通常のloopbackは別のキャプチャバッファへコピーします。ただしDiscordのフックやCPU/GPU/ドライバ競合は未検証です。反復する音声供給不足はデバイス時間と提出サンプル時間を離す可能性がありますが未測定です。判定・入力・音声時計・バッファサイズ・スコア・リプレイは変更しません。

MF BGAは途中の大きな時間ジャンプ後も一要求240フレームずつ過去を読みました。3秒超の差では既存の1秒preroll seekで復帰します。通常再生・既知のsparse gap・EOFを優先し、不正確/失敗したseekや読み取り上限を超える長いGOPでは順次処理を維持して同じkeyframeへの無限巻戻しを防ぎます。seek直後のEOFでは最後の有効画像を保持します。

65秒の合成MP4/AVIで20/40/60秒要求が返す最初のフレームは、修正前8.23/16.23/24.23秒、修正後20/40/60秒です。長いGOPは複数要求を必要とします。MFが非対応の動画は既存FFmpeg経路で別途確認が必要です。10分・120停止の合成試験をFPS60/144/1500、rate0.75/1/1.5、offset−200/0/+200msで実施します。実時間10分試験やDiscord視聴側同期の証明ではありません。

`bga_video_smoke <65秒以上のMF動画> --stall-recovery`、正確に65秒の動画には`--stall-eof`、65秒/30fps/30秒keyframe間隔には`--long-gop`を使用します。MF専用検査はFFmpeg代替を成功とみなしません。既存`--timeline`/`--ffmpeg`は維持します。同じ曲・設定で共有前後とDiscord DebugのScreen Share方式を比較してください。既存オーバーレイだけではDiscordキューや実ゲームの音声underrunは確定できず、実ゲームのdevice/QPC・GetBuffer/ReleaseBufferトレースは追加していません。


## References

- [Discord capture methods](https://support.discord.com/hc/en-us/articles/9410427556375--Windows-Capturing-Application-Window-for-Screen-Share-and-Go-Live)
- [Discord streaming pipeline](https://discord.com/blog/how-it-all-goes-live-an-overview-of-discords-streaming-technology)
- [Windows loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
- [MF approximate seeking](https://learn.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-setcurrentposition)
