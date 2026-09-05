#include "render/MenuWindow.h"

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
        if (arg == "--result") result = true;
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
        window.render(data);
        while (const auto click = window.poll_click_event()) {
            std::cout << "hit kind=" << static_cast<int>(click->kind)
                      << " index=" << click->index << std::endl;
        }
        if (window.had_fatal_error()) return 1;
        if (std::chrono::steady_clock::now() - start > std::chrono::minutes(10)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    window.shutdown();
}
