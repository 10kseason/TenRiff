#pragma once

#include <string>
#include <string_view>
#include "app/menu/MenuScreen.h"
#include "ui/Localization.h"

namespace tenriff::app::menu::settings {

struct SettingsHelpText { std::string_view en, ko, ja; };

// Indexed by stable setting IDs, never by translated labels or display order.
// Keep these tables in sync with the controller enums (coverage is tested).
inline std::string setting_help(Screen screen, int id, ui::Language language) {
    static constexpr SettingsHelpText audio[] = {
        {"Basic favors stable playback; High reduces buffering. Use Basic if audio crackles.", "기본은 안정적인 재생, 고성능은 작은 버퍼를 우선합니다. 소리가 끊기면 기본을 사용하세요.", "基本は安定性、高性能は小さいバッファを優先します。音が途切れる場合は基本を選びます。"},
        {"Follow plays BMS key sounds when you hit notes. Autoplay plays them with the chart; Off silences them.", "연동은 노트를 누를 때 키음을 냅니다. 자동재생은 차트에 맞춰 키음을 재생하며, 끔은 키음을 제거합니다.", "連動はノート入力時、自動再生は譜面に合わせてキー音を鳴らします。オフではキー音を消します。"},
        {"Toggle menu, result and song-preview music. Gameplay chart BGM keeps playing.", "메뉴·결과·곡 미리듣기 음악을 켜거나 끕니다. 플레이 중 차트 BGM은 계속 재생됩니다.", "メニュー・結果・試聴の音楽を切り替えます。プレイ中のBGMは継続します。"},
        {"Adjust the volume of all game audio together. Start low before raising it.", "게임 전체 음량을 조절합니다. 낮은 값에서 조금씩 올려 맞추세요.", "ゲーム全体の音量です。小さい値から調整してください。"},
        {"Adjust background music without changing key-sound volume.", "키음 크기를 유지하면서 배경 음악의 음량만 조절합니다.", "キー音は変えず、背景音楽の音量を調整します。"},
        {"Adjust note key sounds. Raise this relative to BGM if key sounds are hard to hear.", "노트 키음의 음량을 조절합니다. 키음이 잘 들리지 않으면 BGM보다 높게 맞추세요.", "ノートのキー音量です。聞こえにくい場合はBGMより高く設定します。"},
        {"Shift chart BGM and autoplay key sounds. Positive delays audio; Follow key sounds and judgement timing stay unchanged.", "차트 BGM과 자동재생 키음의 시점을 옮깁니다. 양수는 소리를 늦추며, 직접 누르는 연동 키음과 판정은 그대로입니다.", "BGMと自動再生キー音の時刻を調整します。正の値で遅くなり、連動キー音と判定は変わりません。"},
        {"Save audio changes and return to the previous screen.", "변경한 오디오 설정을 저장하고 이전 화면으로 돌아갑니다.", "音声設定を保存して前の画面に戻ります。"},
        {"Reduce volume differences between songs. This can change perceived loudness.", "곡 사이의 음량 차이를 줄입니다. 켜면 체감 음량이 달라질 수 있습니다.", "曲ごとの音量差を抑えます。聴感上の音量が変わる場合があります。"},
        {"WASAPI uses Windows audio. ASIO requires a compatible installed driver and device.", "WASAPI는 Windows 오디오를 사용합니다. ASIO는 호환 장치와 설치된 전용 드라이버가 필요합니다.", "WASAPIはWindows音声、ASIOは対応機器と専用ドライバーを使用します。"},
        {"Choose the ASIO device driver. Press F5 to refresh the installed driver list.", "사용할 ASIO 장치 드라이버를 고릅니다. F5를 누르면 설치된 드라이버 목록을 다시 읽습니다.", "ASIOドライバーを選びます。F5で一覧を更新します。"},
        {"Choose the audio sample rate supported by your device. Unsupported ASIO rates may fail to start.", "장치가 지원하는 오디오 샘플 레이트를 고릅니다. ASIO가 지원하지 않는 값이면 시작에 실패할 수 있습니다.", "機器が対応するサンプルレートを選びます。非対応のASIO設定では開始に失敗する場合があります。"},
        {"Smaller buffers reduce delay but can cause crackling. Increase the value if playback breaks up.", "버퍼가 작으면 지연이 줄지만 소리가 끊길 수 있습니다. 끊김이 생기면 값을 올리세요.", "小さいバッファは遅延を減らしますが、音切れが出たら大きくしてください。"},
        {"Mute game audio while another window is active. Audio returns when you focus TenRiff.", "다른 창을 사용하는 동안 게임 소리를 끕니다. TenRiff로 돌아오면 다시 재생됩니다.", "他のウィンドウを使用中は消音し、TenRiffに戻ると解除します。"},
        {"Choose title-screen music: default, random BMS, last played, or none.", "타이틀 음악을 기본 음악, 랜덤 BMS, 마지막 플레이한 곡 또는 없음으로 고릅니다.", "タイトル音楽を標準、ランダムBMS、最後に遊んだ曲、なしから選びます。"},
        {"After the last judgement, listen to the remaining audio or skip it after the result delay. Notes and scoring stay unchanged.", "마지막 판정 뒤 남은 음악을 끝까지 듣거나 결과 대기 시간 후 스킵합니다. 노트와 점수는 그대로입니다.", "最後の判定後、残りの音楽を最後まで聴くか結果待機時間後にスキップします。"},
    };
    static constexpr SettingsHelpText graphics[] = {
        {"Choose windowed, borderless, or fullscreen display.", "창 모드, 테두리 없는 창, 전체 화면 중 표시 방식을 고릅니다.", "ウィンドウ、ボーダーレス、全画面を選びます。"},
        {"Set the output resolution. Higher resolutions show more detail and require more GPU work.", "화면 해상도를 고릅니다. 높일수록 선명해지지만 GPU 작업량도 늘어납니다.", "表示解像度を選びます。高解像度ほどGPU負荷が増えます。"},
        {"Set the frame-rate limit when VSync is off. Menus remain capped at 300 FPS.", "수직동기화가 꺼졌을 때 프레임 상한을 정합니다. 메뉴는 최대 300 FPS로 동작합니다.", "垂直同期オフ時のFPS上限です。メニューは最大300 FPSです。"},
        {"Match presentation to the monitor to reduce tearing. This may add display latency.", "모니터 주기에 맞춰 화면 찢어짐을 줄입니다. 표시 지연이 늘어날 수 있습니다.", "モニターに同期して画面のずれを抑えます。表示遅延が増える場合があります。"},
        {"Show FPS and frame-time statistics. Use the graph to check stutters.", "FPS와 프레임 시간 통계를 표시합니다. 그래프로 순간 끊김을 확인할 수 있습니다.", "FPSとフレーム時間を表示します。グラフで瞬間的な停止を確認できます。"},
        {"Enable chart background images and video. Turn this off to reduce visual distraction and load.", "차트 배경 이미지와 영상을 표시합니다. 끄면 시각적 방해와 재생 부하를 줄일 수 있습니다.", "譜面の背景画像・動画を表示します。オフで視覚的な妨げと負荷を減らせます。"},
        {"Allow BGA behind the note lanes. Turn it off if the chart becomes difficult to read.", "노트 레인 뒤에도 BGA를 표시합니다. 노트가 잘 보이지 않으면 끄세요.", "ノートの後ろにもBGAを表示します。見づらい場合はオフにしてください。"},
        {"Choose how background images are enlarged. ONNX uses a local model and adds GPU processing.", "배경 확대 방식을 고릅니다. ONNX는 로컬 모델을 사용하며 GPU 작업이 추가됩니다.", "背景の拡大方式です。ONNXはローカルモデルを使用し、GPU負荷が増えます。"},
        {"Select the local ONNX model used for background upscaling.", "배경 업스케일에 사용할 로컬 ONNX 모델을 선택합니다.", "背景拡大に使うローカルONNXモデルを選びます。"},
        {"Prefer the lower-power DirectX adapter for ONNX background upscaling. This does not select the main renderer's GPU.", "ONNX 배경 업스케일에 전력 소모가 적은 DirectX 어댑터를 우선합니다. 게임 전체의 렌더링 GPU를 바꾸는 설정은 아닙니다.", "ONNX背景拡大で低消費電力のDirectX機器を優先します。メイン描画のGPUは変更しません。"},
        {"Save graphics settings and return.", "그래픽 설정을 저장하고 돌아갑니다.", "画面設定を保存して戻ります。"},
    };
    static constexpr SettingsHelpText input[] = {
        {"RawInput receives Windows key events. Polling checks key state repeatedly; use it if RawInput is incompatible.", "RawInput은 Windows 키 이벤트를 받습니다. Polling은 키 상태를 반복 확인하며, RawInput 호환 문제가 있을 때 사용할 수 있습니다.", "RawInputはWindowsのキーイベントを受信します。互換性に問題がある場合はPollingを試してください。"},
        {"Set the key polling frequency. Higher rates cost more CPU. During play this also controls RawInput's bound-key backup checks.", "키 상태 확인 빈도입니다. 높일수록 CPU를 더 사용합니다. 플레이 중에는 RawInput의 노트 키 보조 감시 주기에도 적용됩니다.", "キー確認頻度です。高い値ほどCPUを使います。プレイ中のRawInput補助監視にも適用されます。"},
        {"Suppress rapid duplicate key transitions. Extra debounce can delay fast repeated presses.", "짧은 간격의 중복 키 변화를 억제합니다. 과도하게 설정하면 빠른 연타 입력이 지연될 수 있습니다.", "短時間の重複入力を抑えます。大きすぎる値は素早い連打を遅らせる場合があります。"},
        {"Save input settings and apply backend changes on return.", "입력 설정을 저장하고 돌아가며, 변경한 입력 방식을 적용합니다.", "入力設定を保存し、入力方式の変更を適用して戻ります。"},
    };
    static constexpr SettingsHelpText calibration[] = {
        {"Choose the number of milliseconds changed by one adjustment.", "좌우 키를 한 번 누를 때 바뀌는 밀리초 단위를 고릅니다.", "左右を一度押したときの調整幅を選びます。"},
        {"Shift the input timestamp used for judgement. Change in small steps while checking EARLY/LATE feedback.", "판정에 사용하는 입력 시점을 보정합니다. EARLY/LATE 표시를 확인하며 조금씩 조절하세요.", "判定に使う入力時刻を補正します。EARLY/LATEを確認しながら少しずつ調整します。"},
        {"Positive values draw notes further ahead in time. This changes the display only, not judgement or audio.", "양수는 노트를 시간상 앞서 표시합니다. 화면만 바뀌며 판정과 오디오 시점은 바꾸지 않습니다.", "正の値でノート表示を時間的に先へ進めます。判定と音声は変わりません。"},
        audio[6],
        {"Reset input, visual, and sound offsets to 0 ms.", "입력·화면·소리 보정값을 모두 0ms로 되돌립니다.", "入力・表示・音声の補正値をすべて0 msに戻します。"},
        {"Save timing offsets and return.", "타이밍 보정값을 저장하고 돌아갑니다.", "タイミング補正値を保存して戻ります。"},
    };
    static constexpr SettingsHelpText mode[] = {
        {"Safe builds hashes, previews and difficulty. Fast indexes basic song metadata only; tables switch it back to Safe.", "안전은 해시·미리보기·난이도를 준비합니다. 빠름은 기본 곡 정보만 읽으며, 난이도표를 선택하면 안전으로 전환됩니다.", "安全はハッシュ・プレビュー・難易度も作成します。高速は基本情報のみで、難易度表選択時は安全に戻ります。"},
        {"Calculate native LV/CR during Safe indexing. Fast indexing always skips this calculation.", "안전 인덱싱 중 자체 LV/CR을 계산합니다. 빠름 모드에서는 켜도 계산을 건너뜁니다.", "安全スキャンで独自LV/CRを計算します。高速では常に省略します。"},
        {"Compare your play with the best compatible saved replay in a split view.", "호환되는 최고 기록 리플레이를 불러와 분할 화면에서 현재 플레이와 비교합니다.", "対応する最高記録のリプレイと分割画面で比較します。"},
        {"Let the game hit notes automatically. The result is marked ASSIST.", "게임이 노트를 자동으로 처리합니다. 결과에는 ASSIST가 표시됩니다.", "ノートを自動で演奏します。結果はASSIST扱いになります。"},
        {"Continue after gauge failure. Judgements and the final result are still recorded.", "게이지가 소진되어도 끝까지 연습합니다. 판정과 최종 결과는 계속 기록합니다.", "ゲージが尽きても最後まで練習します。判定と結果は記録されます。"},
        {"End on the first OD8 MISS. Empty-key POOR does not count. This disables No Fail.", "첫 OD8 MISS에서 플레이를 종료합니다. 빈 키 POOR는 세지 않으며, 켜면 실패 없는 연습 모드가 꺼집니다.", "最初のOD8 MISSで終了します。空打ちPOORは対象外で、No Failとは併用できません。"},
        {"Show the live accuracy gap or score gap against the target pace. Normal gauge failure and scoring remain active; otherwise eligible records still count.", "플레이 중 목표 정확도 또는 진행도에 맞춘 목표 점수와의 차이를 표시합니다. 일반 게이지 실패·채점은 유지하며, 다른 기록 조건을 충족하면 최고 기록과 랭킹에 반영됩니다.", "目標精度・進行度に応じた目標スコアとの差を表示します。通常ゲージと採点を維持し、他の条件を満たす記録は対象になります。"},
        {"Set the target for the selected Pacemaker mode. Enable Pacemaker first.", "선택한 페이스메이커 방식의 목표 정확도 또는 점수를 정합니다. 먼저 페이스메이커를 켜세요.", "ペースメーカーの目標精度・スコアです。先にペースメーカーを有効にします。"},
        {"Original keeps the chart's lanes. A numbered mode converts the pattern to that key count.", "원본은 차트의 키 수와 배치를 유지합니다. 숫자 키 모드는 패턴을 해당 키 수로 변환합니다.", "原本は譜面のキー数を保持します。数値を選ぶとそのキー数へ変換します。"},
        {"Choose the pattern conversion method. Compare the preview and pick the layout that feels comfortable.", "키 수를 바꿀 때 패턴을 배치하는 방식을 고릅니다. 변환 결과를 확인해 편한 방식을 선택하세요.", "キー数変換時の配置方式を選びます。変換結果を確認して選択してください。"},
        {"Choose the nK2 conversion preset. This setting is unavailable with Krrcream.", "nK2 변환 프리셋을 고릅니다. Krrcream을 선택하면 이 항목은 사용할 수 없습니다.", "nK2変換プリセットです。Krrcream使用時は変更できません。"},
        {"Choose the starting gauge. Gauge shifting follows the selected play mode's rules.", "시작할 게이지 종류를 고릅니다. 이후 게이지 전환은 선택한 플레이 모드의 규칙을 따릅니다.", "開始時のゲージを選びます。以降のシフトはプレイモードの規則に従います。"},
        {"Rearrange note lanes. Mirror reverses lanes; random modes change the pattern using a seed.", "노트 레인 배치를 바꿉니다. 미러는 좌우를 뒤집고, 랜덤 계열은 시드를 이용해 배치를 바꿉니다.", "ノート配置を変更します。ミラーは左右反転、ランダム系はシードで配置を変えます。"},
        {"Set a repeatable pattern seed. Modes marked Auto choose a fresh seed each play.", "같은 배치를 재현할 시드를 정합니다. 자동으로 표시되는 모드는 플레이마다 새 시드를 사용합니다.", "配置を再現するシードです。自動と表示されるモードは毎回新しくなります。"},
        {"Open gameplay modifiers. Their effects and score multipliers are shown in the next screen.", "플레이 모드를 엽니다. 다음 화면에서 효과와 점수 배율을 확인할 수 있습니다.", "プレイMODを開きます。次の画面で効果とスコア倍率を確認できます。"},
        {"Change song speed and timing together. 1.00x is original speed; this also affects score eligibility.", "곡 재생 속도와 노트 타이밍을 함께 바꿉니다. 1.00x가 원래 속도이며 기록 조건에도 영향을 줍니다.", "曲速とノート時刻を一緒に変更します。1.00xが原速で、記録条件にも影響します。"},
        {"Change note scrolling speed without changing song speed or judgement timing.", "곡 속도와 판정 시점은 유지하고, 노트가 화면을 이동하는 속도만 바꿉니다.", "曲速と判定時刻を変えず、ノートのスクロール速度だけを変えます。"},
        {"Save mode settings and return.", "모드 설정을 저장하고 돌아갑니다.", "モード設定を保存して戻ります。"},
    };
    static constexpr SettingsHelpText skin[] = {
        {"Choose the key layout to edit and preview. Each layout keeps its own lane settings.", "편집하고 미리 볼 키 배치를 고릅니다. 레이아웃마다 레인 설정을 따로 유지합니다.", "編集・プレビューするキー配置です。配置ごとにレーン設定を保持します。"},
        {"Choose 7+1 in Key Mode above to move the scratch lane left or right. Other layouts keep this option disabled.", "위의 키 모드를 7+1로 선택하면 스크래치를 왼쪽·오른쪽으로 옮길 수 있습니다. 다른 키 배치에서는 비활성화됩니다.", "上のキーモードで7+1を選ぶとスクラッチを左右に移せます。他の配置では無効です。"},
        {"Choose Native, TenRiff skin.json, or an imported LR2 skin.", "기본 Native, TenRiff skin.json, 가져온 LR2 중 사용할 스킨 방식을 고릅니다.", "Native、TenRiff skin.json、取り込んだLR2スキンを選びます。"},
        {"Choose an installed skin for the selected source and layout.", "선택한 스킨 방식과 키 배치에 사용할 설치된 스킨을 고릅니다.", "選択した方式とキー配置で使うスキンを選びます。"},
        {"Set the LR2 skin's reference resolution so its images keep the intended proportions.", "LR2 스킨의 기준 해상도를 골라 이미지 비율을 맞춥니다.", "LR2スキンの基準解像度を指定し、画像の比率を合わせます。"},
        {"Choose a skin folder or supported skin file to import.", "가져올 스킨 폴더 또는 지원하는 스킨 파일을 선택합니다.", "取り込むスキンのフォルダーや対応ファイルを選びます。"},
        {"Create a new editable skin template in the skin folder.", "스킨 폴더에 편집 가능한 새 스킨 템플릿을 만듭니다.", "編集できる新しいスキンの雛形を作成します。"},
        {"Open the active TenRiff skin folder. First use Create New Skin, or import a skin.json folder and select Skin Source: TenRiff plus its installed skin.", "현재 TenRiff 스킨 폴더를 엽니다. 먼저 새 스킨 만들기를 실행하거나 skin.json 폴더를 가져온 뒤, 스킨 소스를 TenRiff로 바꾸고 가져온 스킨을 선택하세요.", "TenRiffスキンのフォルダーを開きます。新規作成するかskin.jsonフォルダーを取り込み、ソースをTenRiffにして対象スキンを選んでください。"},
        {"Reload an active imported TenRiff skin after editing (F5). Create or import a skin first, then select Skin Source: TenRiff and the installed skin.", "사용 중인 TenRiff 스킨 파일을 다시 읽습니다 (F5). 새 스킨을 만들거나 가져온 뒤 스킨 소스 TenRiff와 해당 스킨을 선택하면 사용할 수 있습니다.", "使用中のTenRiffスキンを再読み込みします（F5）。作成・取り込み後、ソースをTenRiffにして対象スキンを選択してください。"},
        {"Select the lane whose color and width you want to change.", "색상과 폭을 바꿀 레인을 선택합니다.", "色と幅を変更するレーンを選びます。"},
        {"Select the gap between lanes to adjust.", "간격을 조절할 두 레인 사이의 위치를 선택합니다.", "幅を調整するレーン間の位置を選びます。"},
        {"Set the selected lane's note color. Single Color can override this palette.", "선택한 레인의 노트 색상을 바꿉니다. 단색 모드를 켜면 해당 색이 우선할 수 있습니다.", "選択レーンのノート色です。単色モードの設定が優先される場合があります。"},
        {"Use one shared note color across lanes, or turn this off to use lane colors.", "모든 레인에 같은 노트 색을 쓰거나, 꺼서 레인별 색을 사용합니다.", "全レーンを共通色にするか、オフでレーンごとの色を使います。"},
        {"Choose the note-head shape for the native note style.", "기본 노트 스타일에서 노트 머리의 모양을 고릅니다.", "標準ノートの頭の形を選びます。"},
        {"Show or hide the note outline to separate notes from the background.", "노트 테두리를 켜거나 꺼서 배경과의 구분을 조절합니다.", "ノートの輪郭線を切り替えます。"},
        {"Keep the source image's aspect ratio or fit it to the note area.", "노트 이미지의 원래 비율을 유지하거나 노트 영역에 맞춥니다.", "画像の元の比率を保つか、ノート領域に合わせます。"},
        {"Show or hide the vertical lines between lanes.", "레인 사이의 세로 구분선을 표시하거나 숨깁니다.", "レーン間の縦線を表示・非表示にします。"},
        {"Show or hide the line where notes are judged.", "노트를 판정하는 위치의 선을 표시하거나 숨깁니다.", "ノートの判定位置の線を表示・非表示にします。"},
        {"Show or hide the gear boundary around the playfield.", "플레이 영역 주변의 기어 경계 표시를 켜거나 끕니다.", "プレイ領域のギア境界を表示・非表示にします。"},
        {"Show the end cap of long notes so release timing is easier to see.", "롱노트의 끝부분을 표시해 손을 떼는 시점을 구분하기 쉽게 합니다.", "ロングノートの終端を表示し、離す時刻を見やすくします。"},
        {"Taper the long-note tail instead of keeping a constant width.", "롱노트 끝을 같은 폭으로 유지할지, 가늘어지게 표시할지 고릅니다.", "ロングノートの終端を細くするか、同じ幅に保つかを選びます。"},
        {"Apply a preset combination of skin visual settings.", "스킨의 시각 설정 조합을 프리셋으로 한 번에 적용합니다.", "スキンの表示設定をプリセットでまとめて適用します。"},
        {"Adjust how opaque the lane background is. Higher values hide more of the BGA.", "레인 배경의 불투명도를 조절합니다. 높일수록 뒤쪽 BGA가 덜 보입니다.", "レーン背景の不透明度です。高いほど背後のBGAを隠します。"},
        {"Adjust note visibility. Lower opacity makes notes more transparent.", "노트의 불투명도를 조절합니다. 낮출수록 노트가 투명해집니다.", "ノートの不透明度です。低いほど透明になります。"},
        {"Adjust the strength of note outlines.", "노트 테두리가 얼마나 진하게 보일지 조절합니다.", "ノート輪郭の濃さを調整します。"},
        {"Adjust long-note body opacity independently of its head.", "롱노트 머리와 별도로 몸통의 불투명도를 조절합니다.", "ロングノートの胴体の不透明度を頭とは別に調整します。"},
        {"Adjust the glow at the judgement line when a lane is active.", "레인이 활성화될 때 판정선에 나타나는 빛의 강도를 조절합니다.", "入力時の判定線の発光を調整します。"},
        {"Choose the effect displayed when a note is hit.", "노트를 처리할 때 나타나는 키 폭발 효과의 모양을 고릅니다.", "ノートを叩いた際のエフェクトを選びます。"},
        {"Toggle the visual pulse on pressed keys.", "키를 누를 때 나타나는 맥동 효과를 켜거나 끕니다.", "キー入力時のパルス表示を切り替えます。"},
        {"Show a background panel behind the key area.", "키 표시 영역 뒤에 배경 패널을 표시합니다.", "キー表示の後ろに背景パネルを表示します。"},
        {"Adjust the key background's opacity.", "키 배경 패널의 불투명도를 조절합니다.", "キー背景の不透明度を調整します。"},
        {"Move key labels above or below the key area.", "키 이름 표시를 키 영역 위나 아래로 옮깁니다.", "キー名をキー領域の上か下に配置します。"},
        {"Move the judgement line vertically. The preview shows the new hit position.", "판정선의 세로 위치를 옮깁니다. 미리보기에서 입력 위치를 확인하세요.", "判定線を上下に移動します。プレビューで位置を確認できます。"},
        {"Adjust the width of the selected lane.", "선택한 레인의 폭을 조절합니다.", "選択レーンの幅を調整します。"},
        {"Adjust note width for this key layout.", "현재 키 배치의 노트 폭을 조절합니다.", "このキー配置のノート幅を調整します。"},
        {"Adjust the selected gap between lanes.", "선택한 두 레인 사이의 간격을 조절합니다.", "選択したレーン間の幅を調整します。"},
        {"Adjust the thickness of lane divider lines.", "레인 구분선의 두께를 조절합니다.", "レーン区切り線の太さを調整します。"},
        {"Adjust the space between the two halves of the 16K layout.", "16K 배치에서 양쪽 키 묶음 사이의 중앙 간격을 조절합니다.", "16K配置の左右の間隔を調整します。"},
        {"Adjust long-note body width independently of note heads.", "노트 머리와 별도로 롱노트 몸통의 폭을 조절합니다.", "ロングノート胴体の幅を頭とは別に調整します。"},
        {"Set note height from 50% to 400%. Larger notes can overlap in dense patterns.", "노트 높이를 50~400%로 조절합니다. 높이면 밀집 구간에서 노트가 겹칠 수 있습니다.", "ノート高さを50～400%で調整します。大きいと密集部分で重なる場合があります。"},
        {"Move the combo display up or down.", "콤보 표시의 세로 위치를 옮깁니다.", "コンボ表示を上下に移動します。"},
        {"Use a black playfield behind the lanes to improve note contrast.", "레인 뒤의 플레이 영역을 검정으로 표시해 노트와의 대비를 높입니다.", "レーン背後を黒にしてノートのコントラストを高めます。"},
        {"Choose the font used by the skin UI.", "스킨 UI에 사용할 글꼴을 고릅니다.", "スキンUIで使うフォントを選びます。"},
        {"Positive values advance note visuals. Judgement and audio timing remain unchanged.", "양수는 노트 화면을 시간상 앞당깁니다. 판정과 소리 시점은 그대로 유지됩니다.", "正の値でノート表示を先へ進めます。判定と音声は変わりません。"},
        {"Adjust the small visual gap between neighboring notes.", "이웃한 노트 사이의 작은 표시 간격을 조절합니다.", "隣接するノート間の表示間隔を調整します。"},
        {"Show or hide the mouse cursor during gameplay.", "플레이 중 마우스 커서를 표시하거나 숨깁니다.", "プレイ中のマウスカーソルを表示・非表示にします。"},
        {"Show FAST/SLOW text independently of the timing bar.", "타이밍 막대와 별도로 FAST/SLOW 글자를 켜거나 끕니다.", "タイミングバーとは別にFAST/SLOW文字を切り替えます。"},
        {"Save skin adjustments and return.", "스킨 조절값을 저장하고 돌아갑니다.", "スキン設定を保存して戻ります。"},
        {"Move judgement text vertically.", "판정 글자의 세로 위치를 옮깁니다.", "判定文字を上下に移動します。"},
        {"Move judgement text horizontally.", "판정 글자의 가로 위치를 옮깁니다.", "判定文字を左右に移動します。"},
        {"Move the combo display horizontally.", "콤보 표시의 가로 위치를 옮깁니다.", "コンボ表示を左右に移動します。"},
        {"Save the skin settings and assets as a portable .trskin file.", "스킨 설정과 에셋을 다른 PC로 옮길 수 있는 .trskin 파일로 저장합니다.", "設定と素材を移動可能な.trskinファイルに保存します。"},
        {"Load skin settings and assets from a .trskin file.", ".trskin 파일에서 스킨 설정과 에셋을 불러옵니다.", ".trskinファイルから設定と素材を読み込みます。"},
        {"Open the bundled offline skin editor in your browser.", "동봉된 오프라인 스킨 에디터를 브라우저에서 엽니다.", "同梱のオフラインスキンエディターをブラウザーで開きます。"},
        {"Adjust the combo text size.", "콤보 글자 크기를 조절합니다.", "コンボ文字の大きさを調整します。"},
        {"Adjust the judgement text size.", "판정 글자 크기를 조절합니다.", "判定文字の大きさを調整します。"},
        {"Adjust how bright the key background becomes on a hit.", "키를 누를 때 키 배경이 얼마나 밝아지는지 조절합니다.", "入力時のキー背景の明るさを調整します。"},
        {"Adjust the vertical size of the key background panel.", "키 배경 패널의 세로 높이를 조절합니다.", "キー背景パネルの高さを調整します。"},
        {"Show the timing bar independently of FAST/SLOW text.", "FAST/SLOW 글자와 별도로 타이밍 막대를 켜거나 끕니다.", "FAST/SLOW文字とは別にバーを切り替えます."},
        {"Move FAST/SLOW text on the X axis in base pixels.", "FAST/SLOW 글자의 가로 위치를 기준 픽셀 단위로 옮깁니다.", "FAST/SLOW文字を左右に移動します。"},
        {"Move FAST/SLOW text on the Y axis in base pixels.", "FAST/SLOW 글자의 세로 위치를 기준 픽셀 단위로 옮깁니다.", "FAST/SLOW文字を上下に移動します。"},
        {"Move FAST/SLOW bar on the X axis in base pixels.", "FAST/SLOW 막대의 가로 위치를 기준 픽셀 단위로 옮깁니다.", "FAST/SLOWバーを左右に移動します。"},
        {"Move FAST/SLOW bar on the Y axis in base pixels.", "FAST/SLOW 막대의 세로 위치를 기준 픽셀 단위로 옮깁니다.", "FAST/SLOWバーを上下に移動します。"},
        {"Black fog at the top makes notes fade in. 0% is off; higher values extend the fog toward the judgement line.", "상단의 검은 안개 아래에서 노트가 서서히 나타납니다. 0%는 끔이며 높일수록 안개 범위가 판정선 쪽으로 넓어집니다.", "上部の黒い霧からノートが現れます。0%はオフ、値を上げると範囲が広がります。"},
        {"Black fog above the judgement line makes notes fade out. 0% is off; the judgement line, keys and HUD stay visible.", "판정선 위의 검은 안개로 노트가 서서히 사라집니다. 0%는 끔이며 판정선·키·HUD는 유지됩니다.", "判定ライン上の黒い霧でノートが消えます。0%はオフ。ライン・キー・HUDは表示されます。"},
    };
    static constexpr SettingsHelpText profile[] = {
        {"Choose the interface language. The change is applied and saved immediately.", "화면 언어를 선택합니다. 변경 즉시 적용하고 저장합니다.", "表示言語を選びます。すぐに適用・保存されます。"},
        {"Increase menu text size if labels are difficult to read.", "글자가 작으면 메뉴 글자 크기를 키우세요.", "文字が小さい場合はメニュー文字を大きくします。"},
        {"Press Enter or F2 to choose a songs folder. You can also drop a folder onto the window.", "Enter 또는 F2로 곡 폴더를 선택합니다. 창에 폴더를 끌어 놓아도 됩니다.", "EnterかF2で曲フォルダーを選びます。ウィンドウへのドロップも使えます。"},
        mode[11], mode[15], calibration[2], audio[1], input[0],
        {"Press Enter to edit the name shown in records and multiplayer.", "Enter를 눌러 기록과 멀티플레이에 표시할 닉네임을 입력합니다.", "Enterで記録やマルチプレイに表示する名前を編集します。"},
        {"Choose a local PNG or JPG image for your profile.", "프로필에 사용할 로컬 PNG 또는 JPG 이미지를 선택합니다.", "プロフィール用のローカルPNG・JPG画像を選びます。"},
        {"Remove the profile image selection. The original image file is kept.", "프로필 사진 지정을 해제합니다. 원본 이미지 파일은 삭제하지 않습니다.", "プロフィール画像の指定を解除します。元の画像ファイルは残ります。"},
        {"Save key bindings, audio/input settings and the active skin to one .trprofile file for the next version. Accounts and library/history paths are excluded.", "키 배치·오디오·입력 설정과 현재 스킨을 .trprofile 파일 하나로 저장해 다음 버전으로 옮깁니다. 계정과 곡 목록·기록 경로는 포함하지 않습니다.", "キー配置・音声・入力設定と現在のスキンを.trprofileに保存し、次版へ移せます。アカウントやライブラリ・履歴のパスは含みません。"},
        {"Apply a .trprofile settings file. Existing config/keymap files are backed up under settings-backups before import.", ".trprofile 설정 파일을 적용합니다. 적용 전 현재 config/keymap 파일을 settings-backups 폴더에 자동 백업합니다.", ".trprofileを適用します。適用前に現在のconfig/keymapをsettings-backupsへ自動保存します。"},
        {"Finish setup and return. Your changes have already been saved.", "설정을 마치고 돌아갑니다. 변경한 값은 이미 저장되어 있습니다.", "設定を終えて戻ります。変更は保存済みです。"},
        {"Go to the title screen. You can reopen these settings from Options.", "타이틀 화면으로 이동합니다. 옵션에서 다시 설정할 수 있습니다.", "タイトルへ移動します。オプションから再度設定できます。"},
    };
    static constexpr SettingsHelpText mods[] = {
        {"Widen or narrow judgement windows. Easy lowers the score multiplier; the current multiplier is shown below.", "판정 시간 범위를 넓히거나 줄입니다. Easy는 점수 배율을 낮추며, 현재 배율은 아래에 표시됩니다.", "判定幅を広げる・狭める設定です。Easyではスコア倍率が下がります。"},
        {"DP Flip swaps the left and right sides of a double-play layout.", "DP Flip은 더블 플레이 배치의 왼쪽과 오른쪽을 맞바꿉니다.", "DP Flipはダブルプレイの左右を入れ替えます。"},
        {"Add notes at the selected percentage to practice denser patterns.", "선택한 비율만큼 노트를 추가해 더 밀집된 패턴을 연습합니다.", "選んだ割合でノートを追加し、密度の高い配置を練習します。"},
        {"Choose Full LN, LN Mix, or Full Tap. Full Tap converts long notes and uses a 0.50 score multiplier.", "전체 LN, LN Mix, 전체 단노트 중 고릅니다. 전체 단노트는 롱노트를 바꾸며 점수 배율은 0.50입니다.", "Full LN、LN Mix、Full Tapを選びます。Full TapはLNを単ノートにし、倍率は0.50です。"},
        {"No LN Release removes long-note release judgement; continue holding notes through their ends.", "No LN Release는 롱노트 끝의 떼기 판정을 없앱니다. 노트 끝까지 누르고 유지하세요.", "No LN ReleaseはLN終端の離し判定をなくします。終端まで押し続けてください。"},
        {"Auto Scratch plays only BMS scratch lanes. Practice assistance: score multiplier 0%, no official best or ranking. Keyboard lanes remain manual.", "BMS 스크래치 레인만 자동으로 처리합니다. 연습용 보조 기능이며 배율 0%, 최고 기록·랭킹에서 제외됩니다. 일반 키 레인은 직접 입력합니다.", "BMSスクラッチのみ自動演奏します。練習補助のため倍率0%、公式ベスト・ランキング対象外です。鍵盤は手動です。"},
        {"Hide scratch columns only while Auto Scratch is enabled; other key widths remain unchanged.", "오토스크래치를 켰을 때 스크래치 레인을 숨깁니다. 나머지 키의 폭은 유지합니다.", "Auto Scratch中のみスクラッチ列を隠します。他の鍵盤の幅は維持します。"},
        mode[17],
    };
    static constexpr SettingsHelpText hub[] = {
        mode[8],
        {"Assign a primary and optional secondary key for each lane. Enter opens key settings.", "레인마다 기본 키와 보조 키를 지정합니다. Enter로 키 설정을 여세요.", "各レーンのメイン・補助キーを割り当てます。Enterで設定を開きます。"},
        {"Choose a skin and adjust notes, lane spacing, and hit effects with a live preview.", "스킨을 고르고 노트·레인 간격·입력 효과를 미리 보며 조절합니다.", "スキンを選び、ノート・レーン間隔・入力効果をプレビューで調整します。"},
        {"Set display mode, resolution, VSync and background images.", "화면 모드, 해상도, 수직동기화, 배경 표시를 설정합니다.", "画面モード、解像度、垂直同期、背景表示を設定します。"},
        {"Set BMS key sounds, music volume and your audio device. Reduce the buffer only if playback is stable.", "BMS 키음, 음악 음량, 오디오 장치를 설정합니다. 버퍼는 소리가 안정적으로 재생되는 범위에서 줄이세요.", "BMSキー音、音量、音声機器を設定します。バッファは音切れしない範囲で小さくしてください。"},
        input[0],
        {"Adjust input, visual and sound offsets separately. Start at 0 ms and change one setting at a time.", "입력·화면·소리 시점을 따로 보정합니다. 0ms에서 시작해 한 항목씩 조절하세요.", "入力・表示・音声を個別に補正します。0 msから一つずつ調整してください。"},
        {"Set language, text size, nickname, profile image and songs folder.", "언어, 글자 크기, 닉네임, 프로필 사진, 곡 폴더를 설정합니다.", "言語、文字サイズ、名前、画像、曲フォルダーを設定します。"},
        {"Change judgement windows and note patterns. Check the score multiplier when enabling a modifier.", "판정 범위와 노트 패턴을 바꿉니다. 모드를 켤 때 점수 배율도 확인하세요.", "判定幅とノート配置を変えます。MODを有効にするときはスコア倍率も確認してください。"},
        {"Hold several keys to check simultaneous input. Left tests fewer keys; Right tests more (4K to 16K).",
         "여러 키를 함께 눌러 동시 입력을 확인합니다. 테스트에서 ←는 키 수 줄이기, →는 늘리기입니다 (4K~16K).",
         "複数キーの同時入力を確認します。テスト中は左でキー数を減らし、右で増やせます（4K～16K）。"},
    };
    const SettingsHelpText* entries = nullptr;
    std::size_t count = 0;
    const auto use = [&](const auto& values) { entries = values; count = sizeof(values) / sizeof(values[0]); };
    switch (screen) {
        case Screen::SettingsAudio: use(audio); break;
        case Screen::SettingsGraphics: use(graphics); break;
        case Screen::SettingsInput: use(input); break;
        case Screen::SettingsCalibration: use(calibration); break;
        case Screen::SettingsSkins: use(skin); break;
        case Screen::ModeSelect: use(mode); break;
        case Screen::ModeMods: use(mods); break;
        case Screen::QuickSetup: use(profile); break;
        case Screen::OptionsHub: use(hub); break;
        default: break;
    }
    if (id < 0 || static_cast<std::size_t>(id) >= count) return {};
    const auto& help = entries[id];
    return std::string(language == ui::Language::Korean ? help.ko :
                       language == ui::Language::Japanese ? help.ja : help.en);
}

template<class GenericData>
inline void apply_settings_help(Screen screen, GenericData& data, ui::Language language) {
    const auto text = [language](std::string_view en, std::string_view ko, std::string_view ja) {
        return std::string(language == ui::Language::Korean ? ko : language == ui::Language::Japanese ? ja : en);
    };
    data.selected_help.clear();
    data.selected_help_heading.clear();
    if (screen == Screen::OptionsHub) {
        for (std::size_t i = 0; i < data.card_descriptions.size(); ++i)
            data.card_descriptions[i] = setting_help(screen, static_cast<int>(i), language);
        return;
    }
    if (screen == Screen::KeymapTest) {
        data.selected_help_heading = std::to_string(data.rows.empty() ? 0 : data.rows.size() - 1) + "K";
        data.selected_help = setting_help(Screen::OptionsHub, 9, language) + "\n\n" +
            text("The displayed mapping is used for this test only. Esc returns.", "표시된 배치로만 테스트하며 설정은 저장하지 않습니다. Esc로 돌아갑니다.", "表示中の配置でテストするだけで設定は保存しません。Escで戻ります。");
        return;
    }
    for (std::size_t index = 0; index < data.rows.size(); ++index) {
        const auto& row = data.rows[index];
        if (!row.selected) continue;
        auto help = setting_help(screen, row.row_index, language);
        if (screen == Screen::Keymap || screen == Screen::KeymapConfirm) {
            if (index == 0) help = text("Left/Right chooses the layout to edit (4K-16K). Each mode keeps its own mapping.", "좌우로 편집할 배치를 고릅니다 (4K~16K). 모드마다 키 배치를 따로 저장합니다.", "左右で編集する配置を選びます（4K～16K）。配置ごとに保存します。");
            else if (index + 3 < data.rows.size()) help = text("Left/Right selects primary or secondary. Enter starts key capture; press the new key to save. Delete clears a secondary binding.", "좌우로 기본/보조 키를 선택합니다. Enter 후 새 키를 누르면 저장됩니다. Delete는 보조 키 지정을 해제합니다.", "左右でメイン・補助を選びます。Enter後に新しいキーを押すと保存されます。Deleteで補助キーを解除します。");
            else if (index + 3 == data.rows.size()) help = text("Reset this layout's primary and secondary keys to defaults and save immediately.", "현재 배치의 기본·보조 키를 초기값으로 되돌리고 즉시 저장합니다.", "この配置のメイン・補助キーを初期値に戻し、すぐに保存します。");
            else if (index + 2 == data.rows.size()) help = setting_help(Screen::OptionsHub, 9, language);
            else help = text("Return to the previous screen. Key assignments were saved when entered.", "이전 화면으로 돌아갑니다. 입력한 키 배치는 이미 저장되어 있습니다.", "前の画面に戻ります。キー割り当ては保存済みです。");
        }
        if (screen == Screen::OnnxUpscalerConfirm) help = row.row_index == 0
            ? text("Enable local ONNX background upscaling. It adds processing work; disable it if frame times worsen.", "로컬 ONNX 배경 업스케일을 켭니다. 추가 연산이 생기므로 화면이 끊기면 꺼 주세요.", "ローカルONNX背景拡大を有効にします。負荷が増えるため、処理落ちする場合は無効にしてください。")
            : text("Keep the current background rendering without ONNX upscaling.", "ONNX 업스케일을 켜지 않고 현재 배경 표시를 유지합니다.", "ONNX拡大を使わず、現在の背景表示を維持します。");
        if (help.empty()) continue;
        data.selected_help_heading = row.label;
        data.selected_help = help + "\n\n" + (row.adjustable
            ? text("Left / Right: adjust this value.", "← / →: 이 값을 조절합니다.", "左 / 右：値を調整します。")
            : row.activatable ? text("Enter: select this action.", "Enter: 이 항목을 실행합니다.", "Enter：実行します。")
                              : text("Unavailable in the current mode.", "현재 모드에서는 바꿀 수 없습니다.", "現在のモードでは変更できません。"));
        break;
    }
}

} // namespace tenriff::app::menu::settings
