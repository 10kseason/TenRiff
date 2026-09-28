#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "render/MenuWindow.h"
#include "timing/HighResClock.h"
#include "app/PeerBattleRuntimeRules.h"
#include "app/TenRiffSkin.h"
#include "config/Config.h"

#include <windows.h>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <sstream>
#include <vector>

// Synthetic data only. This preview never initializes MenuApp or writes records.
int main(int argc, char** argv) {
    using namespace tenriff::render;
    MenuWindowConfig config;
    config.title = "TenRiff UI Preview - synthetic data";
    config.display_mode = "windowed";
    config.vsync = true;
    bool result = false;
    bool options_grid = false;
    bool table_editor = false;
    bool gameplay = false;
    int players = 0;
    int preview_fps = 144;
    int preview_keys = 10;
    int menu_keys = 7;
    double preview_note_height = 1.8;
    double fixture_seconds = -1.0;
    bool fixture_idle = false;
    int fixed_grade = -1;
    bool moved_labels = false;
    bool empty = false;
    bool failed = false;
    bool reveal = false;
    bool settings = false;
    bool skin_settings = false;
    bool profile_settings = false;
    bool title = false;
    bool focus_options = false;
    bool sites_account = false;
    bool sites_connected = false;
    bool capture_once = false;
    bool capture_requested = false;
    int rendered_frames = 0;
    int frame_limit = 0;
    int avatar_refresh_frame = 0;
    bool cycle_selection = false;
    std::vector<int> capture_frames;
    std::string skin_folder;
    std::string avatar_path;
    MenuRenderData data;
    data.ui_language = tenriff::ui::Language::Korean;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sites-account") sites_account = true;
        else if (arg == "--japanese") data.ui_language = tenriff::ui::Language::Japanese;
        else if (arg == "--reduced-motion") config.reduce_menu_motion = true;
        else if (arg == "--cycle-selection") cycle_selection = true;
        else if (arg == "--frames" && i + 1 < argc) frame_limit = std::stoi(argv[++i]);
        else if (arg == "--skin" && i + 1 < argc) skin_folder = argv[++i];
        else if (arg == "--font-size" && i + 1 < argc)
            data.ui_text_scale = tenriff::config::menu_text_scale(argv[++i]);
        else if (arg == "--avatar" && i + 1 < argc) avatar_path = argv[++i];
        else if (arg == "--note-height" && i + 1 < argc)
            preview_note_height = std::clamp(std::stod(argv[++i]),
                tenriff::config::kNoteHeightScaleMin, tenriff::config::kNoteHeightScaleMax);
        else if (arg == "--avatar-refresh-frame" && i + 1 < argc) avatar_refresh_frame = std::stoi(argv[++i]);
        else if (arg == "--profile") profile_settings = true;
        else if (arg == "--capture-frames" && i + 1 < argc) {
            std::istringstream stream(argv[++i]);
            std::string value;
            while (std::getline(stream, value, ',')) capture_frames.push_back(std::stoi(value));
        }
        else if (arg == "--sites-connected") { sites_account = true; sites_connected = true; }
        else if (arg == "--capture") capture_once = true;
        else if (arg == "--table-editor") table_editor = true;
        else if (arg == "--options") options_grid = true;
        else if (arg == "--gameplay") gameplay = true;
        else if (arg == "--keys" && i + 1 < argc) {
            preview_keys = std::stoi(argv[++i]);
            if (preview_keys < 4 || preview_keys > 16) return 2;
            menu_keys = preview_keys;
        }
        else if (arg == "--fixture-time" && i + 1 < argc) fixture_seconds = std::stod(argv[++i]);
        else if (arg == "--idle-keys") fixture_idle = true;
        else if (arg == "--three-players") players = 3;
        else if (arg == "--eight-players") players = 8;
        else if (arg == "--60fps") preview_fps = 60;
        else if (arg == "--perfect") fixed_grade = 0;
        else if (arg == "--great") fixed_grade = 1;
        else if (arg == "--good") fixed_grade = 2;
        else if (arg == "--moved-labels") moved_labels = true;
        else if (arg == "--small") { config.width = 960; config.height = 540; }
        else if (arg == "--result") result = true;
        else if (arg == "--title") title = true;
        else if (arg == "--focus-options") focus_options = true;
        else if (arg == "--settings") settings = true;
        else if (arg == "--skin-settings") { settings = true; skin_settings = true; }
        else if (arg == "--empty") empty = true;
        else if (arg == "--failed") failed = true;
        else if (arg == "--reveal") reveal = true;
        else if (arg == "--english") data.ui_language = tenriff::ui::Language::English;
        else if (arg == "--performance") data.performance.visible = true;
        else if (arg == "--1080p") { config.width = 1920; config.height = 1080; }
        else if (arg == "--sources") data.song_select.showing_sources = true;
        else if (arg == "--records") data.song_select.showing_records = true;
        else {
            std::cerr << "Unknown preview argument: " << arg << '\n';
            return 2;
        }
    }
    // Match the client localization path. Keep chart metadata, player names,
    // asset paths and renderer status tokens unchanged: those are fixture data.
    const auto loc = [&](std::string_view english, std::string_view korean) {
        return tenriff::ui::text(data.ui_language, english, korean);
    };
    const std::string preview_backend = loc("Input backend: ", "입력 백엔드: ") + "Preview";
    data.account_overlay.visible = sites_account;
    data.account_overlay.sites_mode = sites_account;
    data.account_overlay.sites_connected = sites_connected;
    data.account_overlay.sites_url = "https://tenriff-leaderboard.lastestarcorp.chatgpt.site";
    data.account_overlay.sites_status = sites_connected
        ? loc("Connection imported and protected. You can play now.",
              "연결 정보를 암호화해 저장했습니다. 이제 플레이하면 됩니다.")
        : loc("Open the website, sign in, and copy your connection information.",
              "웹사이트에서 로그인한 뒤 연결 정보를 복사해 주세요.");
    data.kind = result ? MenuScreenKind::ResultScreen : MenuScreenKind::SongSelect;
    if (settings) {
        data.kind = MenuScreenKind::GenericList;
        data.generic.heading = skin_settings ? loc("Skin Settings", "스킨 설정")
                                             : loc("Mode Settings", "모드 설정");
        const std::array<std::string, 20> labels{
            loc("Key Mode", "키 모드"), loc("Master Volume", "마스터 볼륨"),
            loc("Gauge Shift", "게이지 전환"), loc("Random", "랜덤"),
            loc("Note Shape", "노트 모양"), loc("Note Height", "노트 높이"),
            loc("Lane Width", "레인 폭"), loc("Judge Line", "판정선"),
            loc("Visual Latency", "비주얼 레이턴시"), loc("Combo X", "콤보 X"),
            loc("Combo Y", "콤보 Y"), loc("Judgement X", "판정 X"),
            loc("Judgement Y", "판정 Y"), loc("Note Gap", "노트 간격"),
            loc("Lane Spacing", "레인 간격"), loc("Divider Width", "구분선 두께"),
            loc("Visual Opacity", "표시 불투명도"), loc("Outline Alpha", "외곽선 알파"),
            loc("Hit Burst Style", "키 폭발 모양"), loc("UI Font", "UI 글꼴")};
        const std::array<std::string, 20> values{
            "10K", "75%", loc("Normal", "노말"), loc("Off", "꺼짐"),
            loc("Rect", "사각형"), std::to_string(std::lround(preview_note_height * 100.0)) + "%",
            "1.00", "82%", "+0 ms", "0", "24%",
            "0", "18%", "0", "0", "1.0", "100%", "75%", loc("Circle", "원"), "Segoe UI"};
        // Repeat real long help strings to preserve the pagination/wrapping QA
        // case, while showing the same translated prose as the shipped client.
        const std::array<std::string, 4> guides{
            loc("Image Aspect keeps imported note heads and tails from stretching to the gameplay note box.",
                "이미지 비율은 가져온 노트 머리와 꼬리를 게임플레이 노트 박스에 맞춰 늘이지 않도록 유지합니다."),
            loc("The right preview shows the native fallback lane colors and sizing per layout.",
                "오른쪽 미리보기는 레이아웃별 기본 레인 색상과 크기를 보여줍니다."),
            loc("Left / Right or the +/- buttons changes the current value.",
                "좌우 키 또는 -/+ 버튼으로 현재 값을 바꿉니다."),
            loc("Up / Down or the mouse wheel selects a row. Long skin lists have a clickable scrollbar on the right.",
                "위아래 키나 마우스 휠로 행을 고릅니다. 긴 스킨 목록은 오른쪽 스크롤바를 클릭할 수 있습니다.")};
        for (int i = 0; i < 20; ++i) {
            MenuRowData row;
            row.label = labels[static_cast<std::size_t>(i)];
            row.value = values[static_cast<std::size_t>(i)];
            row.selected = i == 0;
            row.adjustable = i != 1;
            row.slider = i == 1;
            row.slider_ratio = 0.75;
            row.increment_enabled = row.decrement_enabled = true;
            row.target_kind = MenuHitTargetKind::SettingsRow;
            row.row_index = i;
            data.generic.rows.push_back(row);
            data.generic.notes.push_back(loc("GUIDE", "안내") + " " + std::to_string(i + 1) +
                ": " + guides[static_cast<std::size_t>(i) % guides.size()]);
        }
        data.generic.footer_notes = {preview_backend,
            loc("VIEW ONLY", "보기 전용") + " / " + loc("OFFLINE", "오프라인")};
        auto& preview = data.generic.skin_preview;
        preview.visible = skin_settings;
        preview.note_height_scale = preview_note_height;
        preview.lane_count = 10;
        preview.mode_label = "10K";
        preview.selected_lane = 1;
        preview.selected_color_label = "#EDF2F7";
        for (int i = 0; i < 10; ++i) preview.lane_colors[i] = i % 2 ? 0x4B76EF : 0xEDF2F7;
    }
    if (profile_settings) {
        data.kind = MenuScreenKind::GenericList;
        data.generic.heading = loc("Profile Setup", "프로필 설정");
        data.generic.profile_preview_visible = true;
        data.generic.profile_avatar_path = avatar_path;
        const std::array<std::string, 12> labels{
            loc("Language", "언어"), loc("Menu Font Size", "메뉴 글자 크기"),
            loc("Songs Folder", "곡 폴더"), loc("Gauge", "게이지"), "Rate",
            loc("Visual Latency", "비주얼 레이턴시"), loc("Keysound", "키음"),
            loc("Input Backend", "입력 방식"), loc("Nickname", "닉네임"),
            loc("Avatar Image", "프로필 사진"), loc("Clear Avatar", "사진 지우기"),
            loc("Done", "완료")};
        const std::array<std::string, 12> values{
            data.ui_language == tenriff::ui::Language::Japanese ? "日本語" : loc("English", "한국어"),
            data.ui_text_scale > 1.2f ? loc("Extra Large", "더 크게") :
                data.ui_text_scale > 1.0f ? loc("Large", "크게") : loc("Normal", "보통"),
            "ALL SONG", "NORMAL", "1.00x", "0 ms", "Follow", "RawInput", "PLAYER",
            avatar_path.empty() ? loc("Not set", "설정 안 됨") : "preview-avatar.png",
            loc("Clear", "지우기"), loc("Back", "돌아가기")};
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            MenuRowData row;
            row.label = labels[i]; row.value = values[i]; row.row_index = i;
            row.target_kind = MenuHitTargetKind::SettingsRow;
            row.selected = i == 1; row.adjustable = i < 8;
            row.increment_enabled = row.decrement_enabled = true;
            data.generic.rows.push_back(std::move(row));
        }
        data.generic.notes = {
            loc("Language and menu font size apply immediately and are saved to this profile.",
                "언어와 메뉴 글자 크기는 즉시 적용되고 이 프로필에 저장됩니다."),
            loc("Nickname is shown in saved records and multiplayer. Avatar Image accepts local PNG/JPG files.",
                "닉네임은 저장 기록과 멀티플레이에 표시됩니다. 프로필 사진은 로컬 PNG/JPG 파일을 사용합니다.")};
    }
    if (title) {
        data.kind = MenuScreenKind::TitleMenu;
        data.title.profile = "PLAYER";
        data.title.profile_avatar_path = avatar_path;
        data.title.track = empty ? "" : "Luminous Horizon [Another]";
        data.title.buttons = {
            {empty ? loc("ADD SONGS FOLDER", "곡 폴더 추가") : loc("PLAY", "플레이"), "+", !focus_options,
             empty ? loc("Choose a folder containing your BMS charts.", "BMS 차트가 있는 폴더를 선택합니다.")
                   : loc("Open your library and choose a track.", "라이브러리에서 플레이할 곡을 고릅니다.")},
            {loc("MULTIPLAYER", "멀티플레이"), "P2P", false,
             loc("Find a room or host a session with friends.", "방을 찾거나 친구들과 새 세션을 엽니다.")},
            {loc("OPTIONS", "옵션"), "\xE2\x9A\x99", focus_options,
             loc("Adjust audio, input, graphics and skins.", "오디오, 입력, 그래픽과 스킨을 설정합니다.")},
            {loc("EXIT", "종료"), "\xE2\x8F\xBB", false, loc("Close TenRiff.", "TenRiff를 종료합니다.")},
        };
        data.title.guides = {
            loc("UP / DOWN or mouse to move", "위아래 키 또는 마우스로 이동"),
            loc("ENTER or double-click to open", "ENTER 또는 더블클릭으로 열기"),
            empty ? loc("PLAY becomes Add Songs Folder until a library is indexed", "라이브러리가 없으면 곡 폴더 추가를 표시합니다")
                  : loc("F2 selects a songs folder; -/+ adjusts Rate", "F2 곡 폴더 선택 / -/+ 배속 조절"),
            loc("F5 refreshes the current song source", "F5 현재 곡 소스 새로고침"),
            loc("F1 opens the control help overlay", "F1 조작 도움말 열기"),
            loc("ESC exits from the title menu", "ESC 타이틀에서 종료"), preview_backend};
    }
    auto& songs = data.song_select;
    songs.profile = "PLAYER";
    songs.profile_avatar_path = avatar_path;
    songs.selected_song_title = "Luminous Horizon / A very long chart title [Another]";
    songs.selected_song_artist = "TenRiff UI Preview";
    songs.selected_song_key_count = menu_keys;
    songs.selected_song_layout = std::to_string(menu_keys) + " KEYS";
    songs.selected_song_difficulty = "12";
    songs.selected_song_bpm = 180;
    songs.selected_song_note_count = 1824;
    songs.selected_song_nps_min = 3;
    songs.selected_song_nps_median = 10;
    songs.selected_song_nps_max = 24;
    songs.selected_song_chart_name = "ANOTHER";
    songs.current_gauge = loc("Normal", "노말");
    songs.current_hi_speed = "3.50";
    songs.current_visual_latency = "+0 ms";
    songs.current_random = loc("Off", "꺼짐");
    songs.difficulty_table_name = empty ? loc("Native LV", "기본 LV") : "発狂BMS難易度表";
    songs.difficulty_table_active = !empty;
    songs.difficulty_table_editing = table_editor;
    songs.difficulty_table_url_input = "https://example.com/table/header.json";
    songs.sort_summary = loc("LV ASC", "LV 오름");
    songs.group_summary = loc("NONE", "없음");
    songs.primary_hint = loc("UP/DOWN  MOVE     ENTER / dbl-click  PLAY     I  EDIT BMS     C  ADD COURSE",
                             "위아래 이동 / ENTER·더블클릭 플레이 / I BMS 편집 / C 코스 추가");
    songs.secondary_hint = loc("LEFT/RIGHT NAV     BACKSPACE BACK     F2 FOLDER     -/+ RATE     F5 REINDEX     F1 HELP",
                               "좌우 이동 / BACKSPACE 뒤로 / F2 폴더 / -/+ 배속 / F5 재검색 / F1 도움말");
    songs.empty_title = loc("NO CHARTS MATCH", "일치하는 차트 없음");
    songs.empty_message = loc("Clear the current search/filter or switch the active source to see more charts.",
                              "검색·필터를 지우거나 활성 소스를 바꿔 더 많은 차트를 확인하세요.");
    songs.result_available = !empty;
    songs.rank = "AAA";
    songs.best_score = 9452;
    songs.detail_score = 8901;
    songs.max_detail_score = 9120;
    songs.max_combo = 742;
    songs.accuracy = 97.82;
    songs.detailed_accuracy = 97.65;
    songs.selected_record_status = loc("CLEAR", "클리어");
    songs.selected_record_created_utc = "2026-09-05 09:00";
    songs.selected_source_name = loc("SONG LIBRARY", "곡 라이브러리");
    songs.selected_source_path = "songs / preview";
    const std::array<std::string, 7> navigation{
        loc("SONGS", "곡 목록"), loc("SOURCES", "소스"), loc("SEARCH", "검색"),
        loc("FILTER", "필터"), loc("RECORDS", "기록"), loc("SESSION MIX", "세션 믹스"),
        loc("OPTIONS", "옵션")};
    for (const auto& label : navigation) {
        MenuButtonData button;
        button.label = label;
        songs.left_nav.push_back(button);
    }
    const char* titles[] = {"Luminous Horizon / A very long chart title [Another]", "Blue Hour", "Afterglow",
                            "Orbit", "Midnight Transit", "Parallel Lines", "First Light"};
    if (!empty) {
        for (int i = 0; i < 7; ++i) {
            SongCardData card;
            card.title = titles[i];
            card.artist = "TenRiff UI Preview";
            card.detail = "7K  /  ANOTHER  /  180 BPM";
            card.level = 12 + i;
            card.level_label = "⑤" + loc("LEVEL", "레벨") + " " + std::to_string(12 + i);
            card.song_index = i;
            card.selected = i == 0;
            card.favorite = i == 1;
            card.lamp = "CLEAR";
            songs.songs.push_back(card);
        }
        songs.song_count = songs.list_total_count = 42;
        songs.record_count = songs.source_count = 42;
        songs.list_visible_count = 7;
    }
    if (songs.showing_sources && !empty) {
        songs.selected_source_all = true;
        songs.selected_source_name = "ALL SONG";
        songs.selected_source_path = loc("All registered song folders", "등록한 모든 곡 폴더");
        songs.selected_source_song_count = 42;
        songs.source_count = 6;
        songs.songs.front().title = "ALL SONG";
        songs.songs.front().artist = songs.selected_source_path;
        songs.songs.front().detail = loc("Combined library · duplicates removed", "통합 라이브러리 · 중복 제외");
        songs.songs.front().level = 42;
    }
    auto& score = data.result;
    score.title = songs.selected_song_title;
    score.artist = songs.selected_song_artist;
    score.profile = "PLAYER";
    score.key_count = menu_keys;
    score.level = 12;
    score.bpm = 180;
    score.rank = failed ? "C" : "AAA";
    score.status = failed ? "FAILED" : "CLEAR";
    score.cleared = !failed;
    score.score = failed ? 4230 : 9452;
    score.detail_score = 8901;
    score.max_detail_score = 9120;
    score.accuracy = songs.accuracy;
    score.detailed_accuracy = songs.detailed_accuracy;
    score.max_combo = 742;
    score.total_notes = score.judged_notes = 1824;
    score.perfect = 1620;
    score.great = 164;
    score.good = 32;
    score.bad = 8;
    score.poor = 0;
    score.mean_delta_ms = 2.4;
    score.stddev_delta_ms = 14.6;
    score.fast_count = 824;
    score.slow_count = 992;
    score.replay_available = !failed;
    score.notes = {loc("VIEW ONLY", "보기 전용") + " / " + loc("OFFLINE", "오프라인"),
                   preview_backend};
    for (int i = 0; i <= 80; ++i) {
        const float position = static_cast<float>(i) / 80.0f;
        score.gauge_points.push_back({position, failed ? 0.7f * (1.0f - position)
            : 0.65f + 0.17f * std::sin(position * 18.0f)});
    }
    if (options_grid) {
        data.kind = MenuScreenKind::GenericList;
        data.generic = {};
        data.generic.heading = loc("Options", "옵션");
        data.generic.card_grid = true;
        const std::array<std::string, 10> labels{
            loc("KEY MODE", "키 모드"), loc("KEYMAP", "키 설정"), loc("SKINS", "스킨"),
            loc("GRAPHICS", "그래픽"), loc("AUDIO", "오디오"), loc("INPUT", "입력"),
            loc("CALIBRATION", "레이턴시"), loc("PROFILE", "프로필"),
            loc("MODS", "모드 설정"), loc("KEY TEST", "키 입력 테스트")};
        const std::array<std::string, 10> values{
            "4K", loc("Configure", "설정"), "LR2", loc("Borderless", "테두리 없음"),
            loc("High", "고성능"), "RawInput", "-43.0 ms", "default",
            loc("Configure", "설정"), loc("Test", "테스트")};
        const std::array<std::string, 10> descriptions{
            loc("Choose the play key mode. The current mode is shown prominently on this first card.",
                "플레이 키 모드를 선택합니다. 현재 모드는 첫 카드에 크게 표시됩니다."),
            loc("Assign gameplay keys and test the current mapping.", "게임 키를 지정하고 현재 키 배치를 테스트합니다."),
            loc("Import and tune TenRiff or LR2 skins, notes, LN colour, and hit bursts.",
                "TenRiff·LR2 스킨과 노트, 롱노트 색, 키 폭발을 설정합니다."),
            loc("Set display mode, resolution, frame timing, and background upscaling.",
                "화면 모드, 해상도, 프레임 타이밍, 배경 업스케일을 설정합니다."),
            loc("Set keysound policy, volumes, and audio preset.", "키음 정책, 음량, 오디오 프리셋을 설정합니다."),
            loc("Set input backend, polling, judgement rate, and debounce.",
                "입력 백엔드, 폴링, 판정 주기, 디바운스를 설정합니다."),
            loc("Calibrate audio and visual timing. Visual latency changes in 1 ms steps.",
                "오디오·비주얼 타이밍을 보정합니다. 비주얼 레이턴시는 1ms씩 조절됩니다."),
            loc("Change profile name, avatar, and device setup.", "프로필 이름, 아바타, 장치 설정을 변경합니다."),
            loc("Choose gameplay modifiers and review the score multiplier.", "플레이 모드와 점수 배율을 확인합니다."),
            loc("Check simultaneous key presses with the current key mapping.", "현재 키 배치로 동시 입력을 확인합니다.")};
        for (int i = 0; i < 10; ++i) {
            MenuRowData row;
            row.label = labels[i]; row.value = values[i]; row.row_index = i;
            row.selected = i == 0; row.activatable = true; row.target_kind = MenuHitTargetKind::OptionsItem;
            data.generic.rows.push_back(row);
            data.generic.card_descriptions.push_back(descriptions[static_cast<std::size_t>(i)]);
        }
    }
    for (int i = 0; i < players; ++i) {
        MultiplayerPlayerData player;
        player.player_id = static_cast<uint8_t>(i + 1); player.rank = i + 1;
        player.name = i == 1 ? "GOMazk / 긴 플레이어 이름" : "PLAYER " + std::to_string(i + 1);
        player.local = i == 0; player.has_score = true; player.finished = result;
        player.score = 9500 - i * 640; player.combo = 123 + i; player.max_combo = 456;
        player.perfect = 620; player.great = 80; player.good = 12; player.bad = 3;
        player.gauge = 90 - i * 10;
        data.result.multiplayer_players.push_back(player);
        data.gameplay.multiplayer_players.push_back(player);
    }
    if (players > 0 && result) data.result.peer_battle = true;
    if (gameplay) {
        data.kind = MenuScreenKind::GameplayHud;
        auto& hud = data.gameplay;
        hud.note_height_scale = preview_note_height;
        hud.active = true; hud.title = loc("Gameplay", "게임플레이") + " / " + loc("LIVE PREVIEW", "미리보기");
        hud.artist = loc("VIEW ONLY", "보기 전용") + " / " + loc("OFFLINE", "오프라인");
        hud.visual_velocity = 1.0 / 48000.0;
        if (moved_labels) {
            hud.judgement_position = 0.4; hud.judgement_offset_x = -120;
            hud.combo_position = 0.5; hud.combo_offset_x = 120;
        }
        hud.lane_count = preview_keys; hud.black_playfield_enabled = true; hud.gauge = 75; hud.gauge_label = "NORMAL";
        hud.lookahead_samples = 48000; hud.past_samples = 4800; hud.duration_samples = 48000 * 600;
        hud.lane_activity_count = hud.lane_pressed_count = static_cast<std::size_t>(preview_keys);
        hud.lane_color_count = hud.key_label_count = static_cast<std::size_t>(preview_keys);
        const auto palette = tenriff::config::default_skin_lane_colors(std::to_string(preview_keys) + "k");
        const std::array<const char*,16> labels{"S","D","F","G","H","J","K","L","Q","W","E","R","U","I","O","P"};
        for (int lane = 0; lane < preview_keys; ++lane) {
            hud.lane_colors[lane] = tenriff::config::skin_color_rgb(palette[lane]);
            hud.key_labels[lane] = labels[lane];
        }
        hud.artist = "LUMA KEYS / " + std::to_string(preview_keys) + "K / SYNTHETIC PREVIEW";
        hud.has_feedback = true; hud.feedback = "PG"; hud.combo = 123;
        hud.peer_visible = players > 0; hud.peer_score_available = players > 0;
        hud.peer_score = 8860; hud.score = 9500;
        const auto lead = tenriff::app::peer_battle_score_lead(hud.score, hud.peer_score);
        hud.versus_score_difference = lead.difference; hud.versus_score_position = lead.position;
        hud.timing_history_count = 3;
        hud.timing_history_delta_ms[0] = -12; hud.timing_history_delta_ms[1] = 9; hud.timing_history_delta_ms[2] = 28;
    }
    if (!skin_folder.empty()) {
        const auto skin = tenriff::app::load_tenriff_skin_folder(skin_folder, preview_keys);
        if (!skin.found || !skin.warnings.empty()) {
            for (const auto& warning : skin.warnings) std::cerr << warning << '\n';
            return 3;
        }
        const auto resolved=std::make_shared<const tenriff::app::ImportedGameplaySkinDefinition>(skin.gameplay);
        // Exercise the same resolved manifest passed by MenuApp to gameplay and Skin Settings.
        auto apply_gameplay_skin = [&](auto& hud) {
            hud.resolved_tenriff_skin=resolved;
            hud.skin_source=skin.native_gameplay_fallback?"native":"tenriff";
            hud.skin_revision=1;
            hud.external_skin_root=skin.root_path;
            hud.external_skin_name=skin.name;
            const auto& style=skin.gameplay_style;
            auto assign=[](auto& destination,const auto& value) { if(value) destination=*value; };
            assign(hud.show_lane_dividers,style.show_lane_dividers);
            assign(hud.show_judgement_line,style.show_judgement_line);
            assign(hud.show_gear_boundary_line,style.show_gear_boundary_line);
            assign(hud.show_hold_tail,style.show_hold_tail);
            assign(hud.hold_tail_taper_enabled,style.hold_tail_taper_enabled);
            assign(hud.judgement_line_glow_enabled,style.judgement_line_glow_enabled);
            assign(hud.key_pulse_enabled,style.key_pulse_enabled);
            assign(hud.note_border_enabled,style.note_border_enabled);
            assign(hud.black_playfield_enabled,style.black_playfield_enabled);
            assign(hud.key_pulse_brightness,style.key_pulse_brightness);
            assign(hud.lane_background_opacity,style.lane_background_opacity);
            assign(hud.visual_opacity,style.visual_opacity);
            assign(hud.note_outline_opacity,style.note_outline_opacity);
            assign(hud.hold_body_opacity,style.hold_body_opacity);
            assign(hud.hit_burst_style,style.hit_burst_style);
            assign(hud.key_label_position,style.key_label_position);
            assign(hud.note_shape,style.note_shape);
            if(skin.gameplay.native_renderer && skin.gameplay.has_hit_position)
                hud.judgement_line_position=skin.gameplay.hit_position/480.0;
            if(!style.lane_colors.empty()) {
                for(int lane=0;lane<preview_keys;++lane)
                    hud.lane_colors[lane]=style.lane_colors[std::min(static_cast<std::size_t>(lane),style.lane_colors.size()-1)];
            }
        };
        apply_gameplay_skin(data.gameplay);
        apply_gameplay_skin(data.generic.skin_preview);
        if(skin.gameplay_style.show_timing_feedback) data.gameplay.show_timing_feedback=*skin.gameplay_style.show_timing_feedback;
        data.gameplay.skin_background_path=skin.gameplay_background_path;
        data.gameplay.skin_background_opacity=skin.gameplay_background_opacity;
        data.lobby_skin.enabled = true;
        data.lobby_skin.revision = 1;
        data.lobby_skin.native_menu_renderer = skin.native_menu_renderer;
        data.lobby_skin.native_menu = skin.native_menu;
        data.lobby_skin.background_path = skin.lobby_background_path;
        data.lobby_skin.logo_path = skin.lobby_logo_path;
        data.lobby_skin.theme_colors = skin.theme_colors;
        data.lobby_skin.referenced_asset_paths = skin.referenced_asset_paths;
        for (const auto& item : skin.layout_rects) {
            const auto& r = item.second;
            data.lobby_skin.layout_rects[item.first] = {r.left, r.top, r.right, r.bottom};
        }
    }
    MenuWindow window;
    window.set_config(config);
    const auto start = std::chrono::steady_clock::now();
    while (!window.should_close()) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) window.request_close();
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (score.presentation_start_ns == 0 && reveal) {
            score.presentation_start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        if (gameplay) {
            const double seconds = fixture_seconds >= 0.0 ? fixture_seconds
                : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            auto& hud = data.gameplay;
            const int hit = static_cast<int>(seconds * 2);
            const int grade = fixed_grade >= 0 ? fixed_grade : (hit / 4) % 3;
            hud.feedback = grade == 0 ? "PG" : grade == 1 ? "GR" : "G";
            hud.feedback_delta_ms = hit % 2 == 0 ? -28.0 : 28.0;
            hud.text_revision = static_cast<uint64_t>(hit + 1);
            hud.motion_revision++;
            hud.combo = 123 + hit; hud.pg = hit + 1;
            hud.current_sample = static_cast<int64_t>(seconds * 48000);
            hud.current_visual_position = seconds;
            hud.audio_sample_time_ns = hud.activity_publish_time_ns = tenriff::timing::HighResClock::now_ns();
            hud.lane_activity.fill(0.0f);
            hud.lane_pressed.fill(0);
            const int struck_lane = hit % preview_keys;
            hud.lane_activity[static_cast<std::size_t>(struck_lane)] = fixture_idle ? 0.0f
                : static_cast<float>(std::max(0.0, 1.0 - std::fmod(seconds, 0.5) * 5.0));
            hud.lane_pressed[static_cast<std::size_t>(struck_lane)] = !fixture_idle && std::fmod(seconds, 0.5) < 0.18;
            // A held key and a pending LN exercise the two distinct visual states.
            const int held_lane = (struck_lane + 2) % preview_keys;
            hud.lane_pressed[static_cast<std::size_t>(held_lane)] = !fixture_idle;
            hud.note_count = 0;
            for (int lane = 1; lane <= preview_keys; ++lane) {
                GameplayNoteData note;
                note.lane = lane;
                note.start_sample = hud.current_sample + static_cast<int64_t>((0.1 + std::fmod(lane * 0.13 + seconds * 0.5, 1.0)) * 40000);
                note.visual_position = note.start_sample / 48000.0;
                note.tail_visual_position = note.visual_position;
                if (lane % 3 == 0) {
                    note.hold = true;
                    note.tail_sample = note.start_sample + 14000;
                    note.tail_visual_position = note.tail_sample / 48000.0;
                }
                hud.notes[hud.note_count++] = note;
                if (lane == held_lane + 1 && !fixture_idle) {
                    note.hold = true; note.head_visible = false;
                    note.start_sample = hud.current_sample - 4800;
                    note.visual_position = note.start_sample / 48000.0;
                    note.tail_sample = hud.current_sample + 18000;
                    note.tail_visual_position = note.tail_sample / 48000.0;
                    hud.notes[hud.note_count++] = note;
                }
            }
        }
        ++rendered_frames;
        if (avatar_refresh_frame > 0 && rendered_frames == avatar_refresh_frame) ++data.profile_avatar_revision;
        if (cycle_selection) {
            const int selection = (rendered_frames / 45);
            for (std::size_t i = 0; i < data.title.buttons.size(); ++i)
                data.title.buttons[i].selected = static_cast<int>(i) == selection % data.title.buttons.size();
            for (std::size_t i = 0; i < data.generic.rows.size(); ++i)
                data.generic.rows[i].selected = static_cast<int>(i) == selection % data.generic.rows.size();
            for (std::size_t i = 0; i < songs.songs.size(); ++i)
                songs.songs[i].selected = static_cast<int>(i) == selection % songs.songs.size();
            if (!songs.songs.empty()) {
                songs.list_selected_index = selection % static_cast<int>(songs.songs.size());
                songs.selected_song_title = songs.songs[songs.list_selected_index].title;
            }
        }
        if (std::find(capture_frames.begin(), capture_frames.end(), rendered_frames) != capture_frames.end()) {
            window.request_screenshot();
            std::cout << "capture frame=" << rendered_frames << '\n';
        }
        if (capture_once && !capture_requested && rendered_frames >= 30) {
            window.request_screenshot();
            capture_requested = true;
        }
        window.render(data);
        if (frame_limit > 0 && rendered_frames >= frame_limit) break;
        if (capture_once && rendered_frames >= 33) break;
        while (const auto click = window.poll_click_event()) {
            std::cout << "hit kind=" << static_cast<int>(click->kind)
                      << " index=" << click->index << std::endl;
            if (click->kind == MenuHitTargetKind::SongDifficultyTable) {
                const auto action = static_cast<SongDifficultyTableAction>(click->index);
                if (action == SongDifficultyTableAction::EditUrl) songs.difficulty_table_editing = true;
                if (action == SongDifficultyTableAction::Cancel) songs.difficulty_table_editing = false;
                if (action == SongDifficultyTableAction::Reset) { songs.difficulty_table_active = false; songs.difficulty_table_name = loc("Native LV", "기본 LV"); }
                if (action == SongDifficultyTableAction::LocalFile) songs.difficulty_table_status = loc("FILE", "파일") + " / " + loc("VIEW ONLY", "보기 전용");
                if (action == SongDifficultyTableAction::Apply) songs.difficulty_table_status = loc("APPLY", "적용") + " / " + loc("OFFLINE", "오프라인");
            }
            if (options_grid && click->kind == MenuHitTargetKind::OptionsItem) {
                for (auto& row : data.generic.rows) row.selected = row.row_index == click->index;
            }
        }
        if (window.had_fatal_error()) return 1;
        if (std::chrono::steady_clock::now() - start > std::chrono::minutes(10)) break;
        std::this_thread::sleep_until(start + std::chrono::nanoseconds(
            static_cast<int64_t>((std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() * preview_fps + 1.0)) * (1000000000 / preview_fps)));
    }
    window.shutdown();
}
