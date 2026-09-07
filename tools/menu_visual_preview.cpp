#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "render/MenuWindow.h"
#include "timing/HighResClock.h"
#include "app/PeerBattleRuntimeRules.h"

#include <windows.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

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
    int fixed_grade = -1;
    bool moved_labels = false;
    bool empty = false;
    bool failed = false;
    bool reveal = false;
    bool settings = false;
    bool skin_settings = false;
    bool title = false;
    bool focus_options = false;
    MenuRenderData data;
    data.ui_korean = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--table-editor") table_editor = true;
        else if (arg == "--options") options_grid = true;
        else if (arg == "--gameplay") gameplay = true;
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
        else if (arg == "--english") data.ui_korean = false;
        else if (arg == "--performance") data.performance.visible = true;
        else if (arg == "--1080p") { config.width = 1920; config.height = 1080; }
        else if (arg == "--sources") data.song_select.showing_sources = true;
        else if (arg == "--records") data.song_select.showing_records = true;
        else {
            std::cerr << "Unknown preview argument: " << arg << '\n';
            return 2;
        }
    }
    data.kind = result ? MenuScreenKind::ResultScreen : MenuScreenKind::SongSelect;
    if (settings) {
        data.kind = MenuScreenKind::GenericList;
        data.generic.heading = skin_settings ? "Skin Settings" : "Mode Settings";
        for (int i = 0; i < 20; ++i) {
            MenuRowData row;
            row.label = i == 0 ? "Key Mode" : i == 1 ? "Master Volume" : "Preview setting " + std::to_string(i + 1);
            row.value = i == 0 ? "10K" : i == 1 ? "75%" : "Normal";
            row.selected = i == 0;
            row.adjustable = i != 1;
            row.slider = i == 1;
            row.slider_ratio = 0.75;
            row.increment_enabled = row.decrement_enabled = true;
            row.target_kind = MenuHitTargetKind::SettingsRow;
            row.row_index = i;
            data.generic.rows.push_back(row);
            data.generic.notes.push_back("Guide " + std::to_string(i + 1) +
                ": This longer explanation wraps inside the guide panel. Changing guide pages must keep the selected setting, its value, and its keyboard focus unchanged.");
        }
        data.generic.footer_notes = {"Input backend: Preview", "Synthetic data only. No settings are saved."};
        auto& preview = data.generic.skin_preview;
        preview.visible = skin_settings;
        preview.lane_count = 10;
        preview.mode_label = "10K";
        preview.selected_lane = 1;
        preview.selected_color_label = "Ice";
        for (int i = 0; i < 10; ++i) preview.lane_colors[i] = i % 2 ? 0x4B76EF : 0xEDF2F7;
    }
    if (title) {
        data.kind = MenuScreenKind::TitleMenu;
        data.title.profile = "PLAYER";
        data.title.track = empty ? "" : "Luminous Horizon [Another]";
        data.title.buttons = {
            {empty ? "ADD SONGS FOLDER" : "PLAY", "+", !focus_options,
             empty ? "Choose a folder containing your BMS charts." : "Open your library and choose a track."},
            {"MULTIPLAYER", "P2P", false, "Find a room or host a session with friends."},
            {"OPTIONS", "\xE2\x9A\x99", focus_options, "Adjust audio, input, graphics and skins."},
            {"EXIT", "\xE2\x8F\xBB", false, "Close TenRiff."},
        };
        data.title.guides = {"UP / DOWN or mouse to move", "ENTER or double-click to open",
                            empty ? "PLAY becomes Add Songs Folder until a library is indexed"
                                  : "F2 selects a songs folder; -/+ adjusts Rate",
                            "F5 refreshes the current song source", "F1 opens the control help overlay",
                            "ESC exits from the title menu", "Input backend: Preview"};
    }
    auto& songs = data.song_select;
    songs.profile = "PLAYER";
    songs.selected_song_title = "Luminous Horizon / A very long chart title [Another]";
    songs.selected_song_artist = "TenRiff UI Preview";
    songs.selected_song_key_count = 7;
    songs.selected_song_layout = "7 KEYS";
    songs.selected_song_difficulty = "12";
    songs.selected_song_bpm = 180;
    songs.selected_song_note_count = 1824;
    songs.selected_song_nps_min = 3;
    songs.selected_song_nps_median = 10;
    songs.selected_song_nps_max = 24;
    songs.selected_song_chart_name = "ANOTHER";
    songs.current_gauge = "NORMAL";
    songs.current_hi_speed = "3.50";
    songs.current_visual_latency = "+0 ms";
    songs.current_random = "OFF";
    songs.difficulty_table_name = empty ? "기본 LV" : "発狂BMS難易度表";
    songs.difficulty_table_active = !empty;
    songs.difficulty_table_editing = table_editor;
    songs.difficulty_table_url_input = "https://example.com/table/header.json";
    songs.sort_summary = "LEVEL";
    songs.group_summary = "ALL";
    songs.primary_hint = "ENTER / Play    SPACE / Options    F1 / Help";
    songs.secondary_hint = "F5 / Refresh    - / + / Rate";
    songs.empty_title = "No matching charts";
    songs.empty_message = "Choose a song folder or adjust your filters.";
    songs.result_available = !empty;
    songs.rank = "AAA";
    songs.best_score = 9452;
    songs.detail_score = 8901;
    songs.max_detail_score = 9120;
    songs.max_combo = 742;
    songs.accuracy = 97.82;
    songs.detailed_accuracy = 97.65;
    songs.selected_record_status = "NORMAL CLEAR";
    songs.selected_record_created_utc = "2026-09-05 09:00";
    songs.selected_source_name = "Preview library";
    songs.selected_source_path = "songs / preview";
    const char* navigation[] = {"SONGS", "SOURCES", "SEARCH", "FILTER", "RECORDS", "OPTIONS", "MODES"};
    for (const char* label : navigation) {
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
            card.level_label = "⑤LEVEL " + std::to_string(12 + i);
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
    auto& score = data.result;
    score.title = songs.selected_song_title;
    score.artist = songs.selected_song_artist;
    score.profile = "PLAYER";
    score.key_count = 7;
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
    for (int i = 0; i <= 80; ++i) {
        const float position = static_cast<float>(i) / 80.0f;
        score.gauge_points.push_back({position, failed ? 0.7f * (1.0f - position)
            : 0.65f + 0.17f * std::sin(position * 18.0f)});
    }
    if (options_grid) {
        data.kind = MenuScreenKind::GenericList;
        data.generic = {};
        data.generic.heading = "옵션";
        data.generic.card_grid = true;
        const char* labels[]{"키 모드", "키 설정", "스킨", "그래픽", "오디오", "입력", "레이턴시", "프로필", "모드 설정", "키 입력 테스트"};
        const char* values[]{"4K", "설정", "LR2", "테두리 없음", "고성능", "RawInput", "-43.0 ms", "default", "설정", "테스트"};
        for (int i = 0; i < 10; ++i) {
            MenuRowData row;
            row.label = labels[i]; row.value = values[i]; row.row_index = i;
            row.selected = i == 0; row.activatable = true; row.target_kind = MenuHitTargetKind::OptionsItem;
            data.generic.rows.push_back(row);
            data.generic.card_descriptions.push_back(std::string(labels[i]) + " 설정을 확인합니다. 안내가 버튼을 가리지 않아야 합니다.");
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
        hud.active = true; hud.title = "Gameplay feedback preview"; hud.artist = "Synthetic / no audio or records";
        hud.visual_velocity = 1.0 / 48000.0;
        if (moved_labels) {
            hud.judgement_position = 0.4; hud.judgement_offset_x = -120;
            hud.combo_position = 0.5; hud.combo_offset_x = 120;
        }
        hud.lane_count = 10; hud.black_playfield_enabled = true; hud.gauge = 75; hud.gauge_label = "NORMAL";
        hud.lookahead_samples = 48000; hud.past_samples = 4800; hud.duration_samples = 48000 * 600;
        hud.lane_activity_count = hud.lane_pressed_count = 10;
        hud.has_feedback = true; hud.feedback = "PG"; hud.combo = 123;
        hud.peer_visible = players > 0; hud.peer_score_available = players > 0;
        hud.peer_score = 8860; hud.score = 9500;
        const auto lead = tenriff::app::peer_battle_score_lead(hud.score, hud.peer_score);
        hud.versus_score_difference = lead.difference; hud.versus_score_position = lead.position;
        hud.timing_history_count = 3;
        hud.timing_history_delta_ms[0] = -12; hud.timing_history_delta_ms[1] = 9; hud.timing_history_delta_ms[2] = 28;
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
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
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
            hud.lane_activity[static_cast<std::size_t>(hit % 10)] = static_cast<float>(std::max(0.0, 1.0 - std::fmod(seconds, 0.5) * 5.0));
            hud.note_count = 0;
            for (int lane = 1; lane <= 10; ++lane) {
                GameplayNoteData note;
                note.lane = lane;
                note.start_sample = hud.current_sample + static_cast<int64_t>((0.1 + std::fmod(lane * 0.13 + seconds * 0.5, 1.0)) * 40000);
                note.visual_position = note.start_sample / 48000.0;
                note.tail_visual_position = note.visual_position;
                hud.notes[hud.note_count++] = note;
            }
        }
        window.render(data);
        while (const auto click = window.poll_click_event()) {
            std::cout << "hit kind=" << static_cast<int>(click->kind)
                      << " index=" << click->index << std::endl;
            if (click->kind == MenuHitTargetKind::SongDifficultyTable) {
                const auto action = static_cast<SongDifficultyTableAction>(click->index);
                if (action == SongDifficultyTableAction::EditUrl) songs.difficulty_table_editing = true;
                if (action == SongDifficultyTableAction::Cancel) songs.difficulty_table_editing = false;
                if (action == SongDifficultyTableAction::Reset) { songs.difficulty_table_active = false; songs.difficulty_table_name = "기본 LV"; }
                if (action == SongDifficultyTableAction::LocalFile) songs.difficulty_table_status = "FILE target received / synthetic preview";
                if (action == SongDifficultyTableAction::Apply) songs.difficulty_table_status = "APPLY target received / no network in preview";
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
