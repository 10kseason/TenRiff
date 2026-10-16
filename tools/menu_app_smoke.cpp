#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Developer integration smoke. Run only in a newly created sandbox; never point
// it at an installed game, an account, a real song library, or a user profile.
#include "app/MenuApp.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/GameSession.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "app/ReplayVerifier.h"
#include "app/ModeManager.h"
#include "app/PersistedRuntimeConfig.h"
#include "app/SitesLeaderboardClient.h"
#include "app/ProfileSetupFlow.h"
#include "config/KeycodeMap.h"
#include "audio/WasapiBackend.h"
#include "config/SimpleJson.h"
#include "timing/HighResClock.h"
#include "../tests/unit/bms_timing_fixture.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using tenriff::config::JsonArray;
using tenriff::config::JsonObject;
using tenriff::config::JsonValue;
using Screen = tenriff::app::menu::Screen;
using Kind = tenriff::render::MenuHitTargetKind;
using Part = tenriff::render::MenuHitPart;

struct Report {
    JsonArray checks;
    unsigned failed = 0;
    void check(const std::string& name, bool passed, std::string detail = {}) {
        checks.emplace_back(JsonObject{{"name", JsonValue(name)}, {"passed", JsonValue(passed)},
                                      {"detail", JsonValue(std::move(detail))}});
        failed += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
    }
};

// MenuWindow's timestamp is a per-render result: the next render clears it to
// zero before doing any work. Read it on that same render thread immediately
// after render_tick, then publish a cumulative observation for slow test polls.
// This instrumentation exists only in the smoke executable, not the product.
struct PresentProbe {
    std::atomic<std::uint64_t> successful_presents{0};
    std::atomic<std::uint64_t> callbacks_without_present{0};
    std::atomic<std::uint64_t> table_loading_presents{0};
    std::atomic<std::uint64_t> raw_zero_reads_after_progress{0};
    std::atomic<std::int64_t> last_completion_ns{0};

    void record(std::int64_t completed_ns) {
        if (completed_ns <= 0) {
            callbacks_without_present.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        last_completion_ns.store(completed_ns, std::memory_order_release);
        successful_presents.fetch_add(1, std::memory_order_release);
    }
};
PresentProbe present_probe;

// Little-endian, mono 16-bit PCM containing silence. The fixture exercises the
// real decoder and audio clock without playing a sound or using external media.
void write_silent_wave(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    auto word = [&](std::uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>(value >> (8 * i)));
    };
    constexpr std::uint32_t bytes = 960;
    out.write("RIFF", 4); word(36 + bytes, 4); out.write("WAVEfmt ", 8);
    word(16, 4); word(1, 2); word(1, 2); word(48000, 4); word(96000, 4);
    word(2, 2); word(16, 2); out.write("data", 4); word(bytes, 4);
    for (unsigned i = 0; i < bytes; ++i) out.put(0);
    if (!out) throw std::runtime_error("Cannot write synthetic WAV");
}

void write_chart(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    write_silent_wave(directory / "silence.wav");
    std::ofstream out(directory / "smoke.bms", std::ios::binary);
    out << "#PLAYER 1\n#TITLE Menu App Smoke Fixture\n#ARTIST TenRiff QA\n"
           "#BPM 120\n#PLAYLEVEL 1\n#RANK 2\n#4K\n#WAV01 silence.wav\n"
           "#00011:01\n#00112:01\n#00213:01\n#00214:0001\n";
    if (!out) throw std::runtime_error("Cannot write synthetic BMS");
}
}  // namespace

namespace tenriff::app {

// These existing test friends are defined only in this executable. Menu actions
// run on their owner thread; the observer reads only mutex-protected HUD data.
struct MenuAppVisualTestAccess {
    static bool online_mode(MenuApp& app) { return app.online_play_mode_; }
    static void check_play_mode(MenuApp& app, Report& report) {
        report.check("playmode.startup_guest_selector", app.ranked_account_overlay_visible_ &&
            app.account_mode_selection_ && !app.online_play_mode_);
        key(app, "Enter");
        report.check("playmode.guest_enter_closes", !app.ranked_account_overlay_visible_ && !app.online_play_mode_);
        key(app, "F10"); key(app, "Right"); key(app, "Enter");
        report.check("playmode.online_opens_connection", app.online_play_mode_ &&
            app.ranked_account_overlay_visible_ && !app.account_mode_selection_ && app.ranked_account_sites_mode_);
        app.active_recording_permission_ = std::make_shared<std::atomic_bool>(true);
        app.handle_menu_click({Kind::AccountPlayMode, 2, Part::Activate});
        app.handle_menu_click({Kind::AccountPlayMode, 0, Part::Activate});
        report.check("playmode.guest_revokes_active_play", !app.online_play_mode_ &&
            !app.ranked_account_overlay_visible_ && !app.active_recording_permission_->load());
        key(app, "F10");
        app.handle_menu_click({Kind::AccountPlayMode, 1, Part::Activate});
        report.check("playmode.online_never_promotes_guest_record", app.online_play_mode_ &&
            !app.active_recording_permission_->load());
        app.choose_play_mode(false);
        app.active_recording_permission_.reset();
    }

    static void check_import_loading(MenuApp& app, Report& report) {
        const auto original = app.config_;
        const auto visible = [&]() {
            std::lock_guard<std::mutex> lock(app.snapshot_mutex_);
            return app.snapshot_.render.loading_progress.visible;
        };
        {
            MenuApp::MenuLoadingScope loading(app, "Importing LR2 skins");
            report.check("loading.snapshot_visible", visible());
            std::string detail;
            report.check("loading.renderer_remains_live", present(app, detail), detail);
            try {
                MenuApp::MenuLoadingScope nested(app, "Nested fixture");
                throw std::runtime_error("fixture");
            } catch (const std::runtime_error&) {}
            report.check("loading.exception_restores_outer", visible() && app.menu_loading_.stage == "Importing LR2 skins");
        }
        report.check("loading.completion_clears_snapshot", !visible());
        report.check("loading.failed_import_clears_snapshot",
            !app.import_lr2_skin_path("missing-lr2-fixture") && !visible());
        const std::filesystem::path skin = "import-fixture/LR2";
        std::filesystem::create_directories(skin);
        const std::string content = "#INFORMATION,0,Loading fixture,Tester\n";
        { std::ofstream file(skin / "play.lr2skin"); file << content; }
        report.check("loading.successful_import_clears_snapshot",
            app.import_lr2_skin_path(std::filesystem::absolute(skin).u8string()) && !visible());
        app.config_ = original;
        app.refresh_available_lr2_skins();
        app.persist_runtime_config();
        app.publish_snapshot();
    }
    static void check_home_navigation(MenuApp& app, Report& report) {
        const int saved_cursor = app.title_cursor_;
        app.title_cursor_ = 0;
        key(app, "Down"); key(app, "Down"); key(app, "Right");
        report.check("title.compact_right_selects_exit", app.title_cursor_ == 3);
        key(app, "Up");
        report.check("title.compact_up_selects_multiplayer", app.title_cursor_ == 1);
        key(app, "Down"); key(app, "Right"); key(app, "Left");
        report.check("title.compact_left_selects_options", app.title_cursor_ == 2);
        render::MenuRenderData view;
        app.populate_title_render_data(view, "Fixture", {});
        report.check("title.single_play_label", view.title.buttons[0].label == app.ui_text("Single Play", "싱글 플레이"));
        app.title_cursor_ = saved_cursor;
        app.publish_snapshot();
    }
    static void check_home_language_cycle(MenuApp& app, Report& report) {
        const std::string saved_language = app.config_.ui.language;
        const int saved_cursor = app.title_cursor_;
        app.config_.ui.language = "en";
        app.publish_snapshot();
        for (const auto token : {"ko", "ja", "zh-CN", "en"}) {
            click(app, Kind::TitleLanguageButton, 0);
            const std::string name = std::string("title.language_") + token;
            report.check(name + ".cycle", app.config_.ui.language == token &&
                app.current_screen() == Screen::Title && app.title_cursor_ == saved_cursor);
            {
                std::lock_guard<std::mutex> lock(app.snapshot_mutex_);
                const auto& view = app.snapshot_.render;
                report.check(name + ".immediate_snapshot", view.ui_language == ui::language_from_token(token) &&
                    view.title.buttons.size() == 4 && view.title.buttons[2].label == app.ui_text("OPTIONS", "옵션"));
            }
            const auto persisted = config::ConfigLoader{}.load_profile(app.profile_dir_);
            report.check(name + ".persisted", persisted.success() && persisted.config.ui.language == token);
        }
        click(app, Kind::TitleLanguageButton, 0, Part::SelectOnly);
        click(app, Kind::TitleLanguageButton, 1);
        report.check("title.language_ignores_non_activation", app.config_.ui.language == "en");
        app.reset_screen(Screen::OptionsHub);
        click(app, Kind::TitleLanguageButton, 0);
        report.check("title.language_scoped_to_home", app.config_.ui.language == "en");
        app.reset_screen(Screen::Title);
        app.config_.ui.language = saved_language;
        app.persist_runtime_config();
        app.publish_snapshot();
    }
    static void check_browser_sort_and_range(MenuApp& app, Report& report) {
        const auto saved_songs = app.indexed_songs_;
        const auto saved_ui = app.config_.ui;
        const auto saved_sort = app.song_sort_mode_;
        const auto saved_group = app.song_group_mode_;
        const int saved_key = app.song_key_filter_, saved_min = app.song_level_min_filter_, saved_max = app.song_level_max_filter_;
        const std::string saved_path = app.selected_song_path();
        app.reset_screen(Screen::SongBrowser);
        app.config_.ui.difficulty_table_path.clear();
        app.config_.ui.song_collection_filter = "all";
        app.song_key_filter_ = app.song_level_min_filter_ = app.song_level_max_filter_ = 0;
        app.song_group_mode_ = MenuApp::SongGroupMode::None;
        app.indexed_songs_.clear();
        for (const auto& item : std::array<std::pair<const char*, int>, 4>{{{"Gamma", 30}, {"Alpha", 2}, {"Beta", 10}, {"Unknown", 0}}}) {
            auto entry = saved_songs.front();
            entry.title = item.first; entry.path = std::string(item.first) + ".bms";
            entry.level = entry.native_level = item.second;
            entry.difficulty_table_level.clear(); entry.difficulty_table_order = -1;
            app.indexed_songs_.push_back(std::move(entry));
        }
        const auto levels = [&]() {
            std::vector<int> result;
            for (std::size_t i = 0; i < app.visible_song_count(); ++i) result.push_back(app.visible_song_entry(i)->level);
            return result;
        };
        app.apply_song_sort(MenuApp::SongSortMode::DifficultyAsc);
        app.settings_cursor_ = 0;
        app.publish_snapshot();
        report.check("filter.numeric_lv_ascending_missing_last", levels() == std::vector<int>({2, 10, 30, 0}));
        app.selected_song_ = 1;
        const std::string selected_path = app.selected_song_path();
        key(app, "Enter");
        report.check("filter.sort_enter_changes_order", app.song_sort_mode_ == MenuApp::SongSortMode::DifficultyDesc &&
            levels() == std::vector<int>({30, 10, 2, 0}));
        report.check("filter.sort_preserves_selected_chart", app.selected_song_path() == selected_path);
        click(app, Kind::SettingsRow, 0);
        report.check("filter.sort_value_click_changes_order", app.song_sort_mode_ == MenuApp::SongSortMode::TitleAsc &&
            app.visible_song_entry(0)->title == "Alpha");
        app.song_group_mode_ = MenuApp::SongGroupMode::Level;
        app.apply_song_sort(MenuApp::SongSortMode::DifficultyDesc);
        report.check("filter.grouped_desc_missing_lv_last", levels() == std::vector<int>({30, 10, 2, 0}));
        app.song_group_mode_ = MenuApp::SongGroupMode::None;
        app.publish_snapshot();
        render::MenuRenderData view;
        app.populate_song_browser_render_data(view);
        const auto range = std::find_if(view.generic.rows.begin(), view.generic.rows.end(), [](const auto& row) { return row.row_index == 3; });
        report.check("filter.range_is_one_row_two_endpoints", view.generic.rows.size() == 11 &&
            range != view.generic.rows.end() && range->secondary_row_index == 4 && !range->secondary_value.empty());
        click(app, Kind::SettingsRow, 3, Part::SelectOnly);
        key(app, "Right");
        report.check("filter.minimum_keyboard_adjust", app.song_level_min_filter_ == 1 && app.song_level_max_filter_ == 0);
        key(app, "Enter"); key(app, "Right");
        report.check("filter.maximum_keyboard_adjust", app.settings_cursor_ == 4 && app.song_level_max_filter_ == 1);
        click(app, Kind::SettingsRow, 4, Part::Increment);
        report.check("filter.maximum_mouse_adjust", app.song_level_max_filter_ == 2 && app.visible_song_count() == 1);
        click(app, Kind::SettingsRow, 3, Part::Decrement);
        report.check("filter.minimum_mouse_adjust", app.song_level_min_filter_ == 0 && app.song_level_max_filter_ == 2);
        key(app, "Down");
        report.check("filter.range_down_skips_duplicate_row", app.settings_cursor_ == 5);
        key(app, "Up");
        report.check("filter.range_up_selects_minimum", app.settings_cursor_ == 3);
        const auto saved = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("filter.range_saved_both_limits", saved.success() &&
            saved.config.ui.song_level_min_filter == 0 && saved.config.ui.song_level_max_filter == 2);
        app.indexed_songs_ = saved_songs;
        app.config_.ui = saved_ui;
        app.song_key_filter_ = saved_key; app.song_level_min_filter_ = saved_min; app.song_level_max_filter_ = saved_max;
        app.song_sort_mode_ = saved_sort; app.song_group_mode_ = saved_group;
        app.settings_cursor_ = 0;
        app.rebuild_visible_song_list(&saved_path);
        app.persist_runtime_config();
        app.reset_screen(Screen::SongSelect);
        app.publish_snapshot();
    }
    static void check_keymap_palette(MenuApp& app, Report& report) {
        const auto saved_skin = app.config_.skin;
        const auto saved_controller = app.keymap_settings_controller_;
        for (const char* mode : {"4k", "5k", "7k", "8k", "10k", "16k"}) {
            app.keymap_settings_controller_.reset(std::stoi(mode), mode);
            const auto count = app.keymap_settings_controller_.lane_ids().size();
            auto& palette = app.config_.skin.lane_colors[mode];
            palette.assign(count, "blue");
            palette.front() = "yellow";
            app.config_.skin.single_color = "off";
            for (const bool single : {false, true}) {
                app.config_.skin.single_color = single ? "red" : "off";
                const auto expected = config::resolved_skin_lane_colors(app.config_.skin, mode);
                render::MenuRenderData bindings, nkro;
                app.populate_keymap_render_data(bindings);
                app.populate_keymap_test_render_data(nkro);
                bool valid = expected.size() == count && count == static_cast<std::size_t>(std::stoi(mode));
                for (std::size_t i = 0; i < count; ++i) {
                    const auto rgb = config::skin_color_rgb(expected[i]);
                    valid = valid && bindings.generic.rows[i + 1].key_color == rgb &&
                        nkro.generic.rows[i].key_color == rgb;
                }
                report.check(std::string("keymap.palette_") + mode + (single ? "_single" : "_lanes"), valid);
            }
        }
        app.config_.skin = saved_skin;
        app.keymap_settings_controller_ = saved_controller;
    }
    static void check_sequential_keymap(MenuApp& app, Report& report) {
        check_keymap_palette(app, report);
        // This fixture owns synthetic down/up edges. A physical held-key
        // snapshot during capture must not turn its first down into a repeat.
        const bool resume_physical_input = app.input_thread_.is_running();
        app.input_thread_.stop();
        while (app.input_thread_.queue().pop()) {}
        app.pressed_keys_.clear();
        app.menu_key_owners_.clear();
        app.reset_song_select_repeat();
        const auto original = app.keymap_;
        app.keymap_settings_controller_.reset(4, "4k");
        click(app, Kind::SettingsRow, 1);
        const auto code = config::KeycodeMap::to_keycode("Q").value();
        const auto now = timing::HighResClock::now_ns();
        app.handle_input_event({code, input::InputState::Pressed, now});
        report.check("keymap.first_key_advances", app.keymap_settings_controller_.selected_row() == 2 &&
            app.keymap_settings_controller_.capture_active());
        app.handle_input_event({code, input::InputState::Pressed, now + 1});
        report.check("keymap.held_key_does_not_advance", app.keymap_settings_controller_.selected_row() == 2);
        app.handle_input_event({code, input::InputState::Released, now + 2});
        for (const char* name : {"W", "E", "R"}) key(app, name);
        report.check("keymap.last_key_stops_capture", app.keymap_settings_controller_.selected_row() == 4 &&
            !app.keymap_settings_controller_.capture_active());
        config::KeymapManager manager;
        const auto saved = manager.load_profile(app.profile_dir_);
        const auto bindings = manager.bindings_for_mode(saved.keymap, "4k");
        report.check("keymap.sequential_profile_readback", saved.success() &&
            bindings.at("lane1") == "Q" && bindings.at("lane2") == "W" &&
            bindings.at("lane3") == "E" && bindings.at("lane4") == "R");
        click(app, Kind::SettingsRow, 1, Part::Increment);
        key(app, "T");
        report.check("keymap.secondary_advances_in_same_slot", app.keymap_settings_controller_.selected_row() == 2 &&
            app.keymap_settings_controller_.secondary_selected() && app.keymap_settings_controller_.capture_active());
        key(app, "Delete");
        report.check("keymap.secondary_delete_stops_capture", !app.keymap_settings_controller_.capture_active());
        click(app, Kind::SettingsRow, 1);
        for (const char* name : {"PadA", "PadB", "PadLT", "PadRT"}) key(app, name);
        const auto gamepad_saved = manager.load_profile(app.profile_dir_);
        const auto gamepad_bindings = manager.bindings_for_mode(gamepad_saved.keymap, "4k");
        report.check("xinput.capture_and_profile_roundtrip", gamepad_saved.success() &&
            gamepad_bindings.at("lane1") == "PadA" && gamepad_bindings.at("lane2") == "PadB" &&
            gamepad_bindings.at("lane3") == "PadLT" && gamepad_bindings.at("lane4") == "PadRT" &&
            !app.keymap_settings_controller_.capture_active());
        key(app, "PadUp");
        report.check("xinput.dpad_menu_navigation", app.keymap_settings_controller_.selected_row() == 3);
        const auto pad_up = config::KeycodeMap::to_keycode("PadUp").value();
        const auto stamp = timing::HighResClock::now_ns();
        app.handle_input_event({app.key_up_, input::InputState::Pressed, stamp});
        app.handle_input_event({pad_up, input::InputState::Pressed, stamp + 1});
        app.handle_input_event({pad_up, input::InputState::Released, stamp + 2});
        report.check("xinput.menu_keyboard_owner_survives_pad_release", app.pressed_keys_.count(app.key_up_) == 1);
        app.handle_input_event({app.key_up_, input::InputState::Released, stamp + 3});
        report.check("xinput.menu_last_owner_releases", app.pressed_keys_.count(app.key_up_) == 0);


        std::string error;
        report.check("keymap.fixture_restored", manager.save_profile(app.profile_dir_, original, &error), error);
        app.keymap_ = app.working_keymap_ = original;
        if (resume_physical_input && !app.input_thread_.start())
            throw std::runtime_error("Cannot restore menu input after keymap fixture");
        app.refresh_menu_input_polling_scope();
        app.publish_snapshot();
    }
    static void select_hard_judge(MenuApp& app, Report& report) {
        constexpr int judge_row = static_cast<int>(menu::settings::ModeSettingId::JudgeWindow);
        click(app, Kind::SettingsRow, judge_row, Part::Increment);
        report.check("judge.menu_hard_selected", mode_mod_category_value("judge_window", app.config_.mode.mods) == "Judge Hard");
        click(app, Kind::SettingsRow, judge_row);
        report.check("judge.value_click_selects_ex", mode_judge_window_label(app.config_.mode.mods, ui::Language::English) == "EX");
        key(app, "Right");
        report.check("judge.ex_wraps_to_normal", mode_mod_category_value("judge_window", app.config_.mode.mods) == "Off");
        key(app, "Left");
        report.check("judge.normal_wraps_back_to_ex", mode_judge_window_label(app.config_.mode.mods, ui::Language::English) == "EX");
        key(app, "Left");
        report.check("judge.keyboard_returns_to_hard", mode_mod_category_value("judge_window", app.config_.mode.mods) == "Judge Hard");
        app.persist_runtime_config();
        const auto saved = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("judge.selection_stays_session_only", saved.success() &&
            mode_mod_category_value("judge_window", saved.config.mode.mods) == "Off");
    }
    static bool last_result_hard_judge(const MenuApp& app) {
        return mode_mod_category_value("judge_window", app.last_result_mods_) == "Judge Hard";
    }
    static void attach_present_probe(MenuApp& app) {
        // Replacing a running callback is a data race. Join it first and use
        // the same window/config/shutdown setup as restart_render_thread. The
        // fixture recreates its window once before any transition checks.
        app.render_thread_.shutdown();
        app.menu_window_.set_config(app.current_window_config());
        if (!app.render_thread_.initialize(app.current_render_config(), [&app]() {
                app.render_tick();
                const auto completed = app.menu_window_.last_present_completion_ns();
                const auto& table = app.render_cache_.song_select;
                if (completed > 0 && (table.difficulty_table_name == "Loading..." ||
                    table.difficulty_table_name == u8"로딩 중" ||
                    table.difficulty_table_name == u8"読み込み中")) {
                    present_probe.table_loading_presents.fetch_add(1, std::memory_order_release);
                }
                // Publish completion after the payload observation so a waiter
                // cannot see the new Present with the old loading-frame count.
                present_probe.record(completed);
            }, [&app]() { app.menu_window_.shutdown(); }) || !app.render_thread_.start()) {
            throw std::runtime_error("Cannot attach smoke Present observer");
        }
    }
    static Screen screen(const MenuApp& app) { return app.current_screen(); }
    static void key(MenuApp& app, const char* name) {
        const auto code = config::KeycodeMap::to_keycode(name);
        if (!code) throw std::runtime_error("Unknown fixture key");
        const auto now = timing::HighResClock::now_ns();
        app.handle_input_event({*code, input::InputState::Pressed, now});
        app.handle_input_event({*code, input::InputState::Released, now + 1});
    }
    static void click(MenuApp& app, Kind kind, int index, Part part = Part::Activate) {
        render::MenuClickEvent event;
        event.kind = kind; event.index = index; event.part = part;
        app.handle_menu_click(event);
    }
    static void isolate(MenuApp& app) {
        // ConfigLoader restores default URLs when reading empty strings. Clear
        // live URLs after initialization too; never open account/online routes.
        app.config_.ui.online_records_server_url.clear();
        app.config_.ui.tenriff_main_server_url.clear();
        app.config_.ui.private_server_url.clear();
        app.ranked_account_main_server_url_.clear();
        app.ranked_account_private_server_url_.clear();
        app.ranked_account_active_server_url_.clear();
        app.global_chat_service_.clear();
        app.global_chat_service_.shutdown();
        app.online_records_service_.shutdown();
        app.sites_leaderboard_service_.shutdown();
        // The existing practice launch also bypasses ranked authorization and
        // automatic Sites replay submission, independently of the empty URLs.
        app.bms_editor_practice_start_seconds_ = 0.0;
    }
    static bool offline(const MenuApp& app) {
        return app.config_.ui.online_records_server_url.empty() &&
               app.config_.ui.tenriff_main_server_url.empty() &&
               app.config_.ui.private_server_url.empty() &&
               app.ranked_account_signed_in_username_.empty() &&
               !app.sites_connection_saved_ && app.bms_editor_practice_start_seconds_.has_value();
    }
    static bool poll_library(MenuApp& app) {
        SongIndex index;
        std::vector<std::string> warnings;
        if (app.song_indexer_.poll_result(index, warnings)) {
            app.update_song_list(std::move(index));
            app.publish_snapshot();
        }
        return app.visible_song_count() == 1;
    }
    static bool present(MenuApp& app, std::string& detail) {
        const auto prior = present_probe.successful_presents.load(std::memory_order_acquire);
        const auto prior_without_present = present_probe.callbacks_without_present.load(std::memory_order_acquire);
        const auto deadline = Clock::now() + std::chrono::seconds(3);
        // Let render_tick consume the newly published snapshot. This verifies
        // presentation liveness, not pixels/hit geometry (a separate fixture).
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        while (Clock::now() < deadline) {
            const auto completed = present_probe.successful_presents.load(std::memory_order_acquire);
            // Keep the old asynchronous sample as diagnostic evidence only.
            // Zero alongside successful progress demonstrates why that sample
            // cannot decide whether the renderer is alive.
            const auto raw = app.menu_window_.last_present_completion_ns();
            const bool progressed = completed > prior;
            if (progressed && raw == 0)
                present_probe.raw_zero_reads_after_progress.fetch_add(1, std::memory_order_relaxed);
            detail = "Successful Presents=" + std::to_string(completed - prior) +
                "; callbacks without Present=" + std::to_string(
                    present_probe.callbacks_without_present.load(std::memory_order_acquire) - prior_without_present) +
                "; polled per-render timestamp=" + std::to_string(raw);
            if (app.menu_window_.had_fatal_error() || app.menu_window_.should_close()) {
                detail += "; window reports fatal error or close";
                return false;
            }
            if (progressed) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        detail += "; no successful Present within 3 seconds";
        return false;
    }
    static std::size_t rows(MenuApp& app) {
        std::lock_guard<std::mutex> lock(app.snapshot_mutex_);
        return app.snapshot_.render.generic.rows.size();
    }
    static const config::RuntimeConfig& config(const MenuApp& app) { return app.config_; }
    static bool result(const MenuApp& app) { return app.has_result_; }
    static int analysis_page(const MenuApp& app) { return app.result_analysis_page_; }
    static bool analysis_available(const MenuApp& app) { return app.last_result_.analysis.available; }
    static void digits(MenuApp& app, std::string_view value) {
        for (const char digit : value) { const char name[]{digit, 0}; key(app, name); }
    }
    static void check_free_resolution(MenuApp& app, Report& report) {
        click(app, Kind::SettingsRow, 1);
        report.check("graphics.custom_size_editor", app.graphics_settings_controller_.editing_resolution());
        digits(app, "1500");
        key(app, "Tab");
        digits(app, "1000");
        app.handle_text_input("1000");  // Delayed WM_CHAR must not duplicate raw numeric keys.
        key(app, "Enter");
        report.check("graphics.custom_size_applied", app.config_.graphics.resolution == "1500x1000" &&
            !app.graphics_settings_controller_.editing_resolution());
        std::string detail;
        report.check("graphics.custom_size_present", present(app, detail), detail);
        key(app, "Enter");
        report.check("graphics.keyboard_size_editor", app.graphics_settings_controller_.editing_resolution());
        for (const char* digit : {"Numpad1", "Numpad5", "Numpad0", "Numpad0"}) key(app, digit);
        key(app, "Tab"); digits(app, "1000"); key(app, "Enter");
        report.check("graphics.numpad_size_applied", app.config_.graphics.resolution == "1500x1000" &&
            !app.graphics_settings_controller_.editing_resolution());
        click(app, Kind::SettingsRow, 1);
        digits(app, "12");
        key(app, "F10");
        key(app, "F11");
        report.check("graphics.editor_blocks_account_and_chat", !app.ranked_account_overlay_visible_ && !app.chat_overlay_visible_);
        key(app, "Enter");
        report.check("graphics.invalid_size_retains_previous", app.graphics_settings_controller_.editing_resolution() &&
            app.graphics_settings_controller_.resolution_error() && app.config_.graphics.resolution == "1500x1000");
        key(app, "Escape");
        report.check("graphics.custom_size_cancel", !app.graphics_settings_controller_.editing_resolution() &&
            screen(app) == Screen::SettingsGraphics);

        const bool resized = resize_owned_window(1377, 777);
        const bool consumed = app.service_window_size_change();
        if (consumed) app.publish_snapshot();
        report.check("graphics.window_resize_native_mailbox", resized && consumed &&
            app.config_.graphics.resolution == "1377x777");
        const auto saved = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("graphics.window_resize_profile_readback", saved.success() &&
            saved.config.graphics.resolution == "1377x777" && saved.config.graphics.display_mode == "windowed");
        report.check("graphics.window_resize_present", present(app, detail), detail);
        const bool queued = resize_owned_window(1369, 769);
        click(app, Kind::SettingsRow, 1);  // Consumes pending resize before opening the edit draft.
        report.check("graphics.pending_resize_before_edit", queued &&
            app.graphics_settings_controller_.resolution_fields()[0] == "1369" &&
            app.graphics_settings_controller_.resolution_fields()[1] == "769");
        digits(app, "1500"); key(app, "Tab"); digits(app, "1000"); key(app, "Enter");
        report.check("graphics.explicit_size_wins_over_pending_resize", app.config_.graphics.resolution == "1500x1000");
        report.check("graphics.explicit_size_after_resize_present", present(app, detail), detail);
    }
    static void check_start_controls(MenuApp& app, Report& report) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            HWND game = nullptr;
            EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
                DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
                wchar_t type[64]{}; GetClassNameW(hwnd, type, 64);
                if (pid == GetCurrentProcessId() && std::wstring(type) == L"TenRiffMenuWindow") {
                    *reinterpret_cast<HWND*>(data) = hwnd; return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&game));
            if (game) SetForegroundWindow(game);
            std::atomic<bool> done{false}, observed{false}, owned{false};
            std::thread dismiss([&]() {
                const auto deadline = Clock::now() + std::chrono::seconds(10);
                while (!done.load() && Clock::now() < deadline) {
                    HWND picker = nullptr;
                    EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
                        DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
                        wchar_t type[64]{}; GetClassNameW(hwnd, type, 64);
                        if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) &&
                            std::wstring(type) == L"#32770") {
                            *reinterpret_cast<HWND*>(data) = hwnd; return FALSE;
                        }
                        return TRUE;
                    }, reinterpret_cast<LPARAM>(&picker));
                    if (picker) {
                        observed = true;
                        if (GetWindow(picker, GW_OWNER) == game && game && !IsWindowEnabled(game)) owned = true;
                        // Cancel only this smoke process's own file picker.
                        PostMessageW(picker, WM_COMMAND, IDCANCEL, 0);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            });
            const auto closed_before = app.native_dialog_closed_ns_;
            APTTYPE apartment{}; APTTYPEQUALIFIER qualifier{};
            const auto apartment_hr = CoGetApartmentType(&apartment, &qualifier);
            click(app, Kind::SettingsRow, profile_setup::kAvatarRow);
            done = true; dismiss.join();
            report.check("dialog.native_avatar_cancel_" + std::to_string(attempt),
                observed && owned && app.native_dialog_closed_ns_ > closed_before &&
                app.config_.ui.profile_avatar_path.empty() && !app.menu_window_.poll_click_event(),
                "observed=" + std::to_string(observed.load()) + "; owned=" + std::to_string(owned.load()) +
                "; input_reset=" + std::to_string(app.native_dialog_closed_ns_ > closed_before) +
                "; apartment_hr=" + std::to_string(apartment_hr) + "; apartment=" + std::to_string(apartment));
        }
        const auto decrease = app.config_.ui.hispeed_decrease_key;
        const auto increase = app.config_.ui.hispeed_increase_key;
        click(app, Kind::SettingsRow, profile_setup::kHispeedDecreaseKeyRow);
        key(app, "F6");
        report.check("profile.speed_conflict_rejected", app.profile_control_key_capture_active_ &&
            app.config_.ui.hispeed_decrease_key == decrease);
        key(app, "F12");
        report.check("profile.speed_key_assigned", !app.profile_control_key_capture_active_ &&
            app.config_.ui.hispeed_decrease_key == "F12");
        auto saved_speed = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("profile.speed_key_persisted", saved_speed.success() &&
            saved_speed.config.ui.hispeed_decrease_key == "F12");
        click(app, Kind::SettingsRow, profile_setup::kHispeedDecreaseKeyRow);
        key(app, "Delete");
        report.check("profile.speed_key_cleared", app.config_.ui.hispeed_decrease_key.empty());
        click(app, Kind::SettingsRow, profile_setup::kHispeedDecreaseKeyRow);
        key(app, "PadLB");
        const auto pad_profile = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("xinput.profile_speed_capture", !app.profile_control_key_capture_active_ &&
            pad_profile.success() && pad_profile.config.ui.hispeed_decrease_key == "PadLB");

        click(app, Kind::SettingsRow, profile_setup::kHispeedDecreaseKeyRow);
        key(app, "F5");
        click(app, Kind::SettingsRow, profile_setup::kHispeedIncreaseKeyRow);
        key(app, "Escape");
        report.check("profile.speed_capture_cancel", app.config_.ui.hispeed_increase_key == increase &&
            !app.profile_control_key_capture_active_);
        click(app, Kind::SettingsRow, profile_setup::kFunctionKeysFirstRow);
        key(app, "F10");
        report.check("profile.function_conflict_rejected", app.profile_control_key_capture_active_ &&
            config::function_key(app.config_.ui, 1) == "F1");
        key(app, "F12");
        report.check("profile.function_assigned", !app.profile_control_key_capture_active_ && app.key_f1_ ==
            config::KeycodeMap::to_keycode("F12").value());
        const auto saved_function = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("profile.function_persisted", saved_function.success() &&
            config::function_key(saved_function.config.ui, 1) == "F12");
        report.check("profile.function_hint", app.ui_text("F1 / F10", "F1 / F10") == "F12 / F10");
        key(app, "F1");
        report.check("profile.old_function_inactive", !app.help_overlay_visible_);
        key(app, "F12");
        report.check("profile.new_function_dispatch", app.help_overlay_visible_);
        key(app, "F12");
        click(app, Kind::SettingsRow, profile_setup::kFunctionKeysFirstRow);
        key(app, "F1");
        report.check("profile.function_restored", app.key_f1_ == config::KeycodeMap::to_keycode("F1").value());
        // Synthetic stale Enter edges model input collected inside a modal
        // picker. The real picker smoke below checks native ownership too.
        app.input_thread_.queue().push({app.key_enter_, input::InputState::Pressed, 1});
        app.song_select_repeat_key_ = app.key_right_;
        app.discard_native_dialog_input();
        report.check("dialog.queued_keyboard_drained", !app.input_thread_.queue().pop().has_value() &&
            app.song_select_repeat_key_ == 0 && app.pressed_keys_.empty());
        click(app, Kind::SettingsRow, profile_setup::kNicknameRow, Part::SelectOnly);
        app.handle_input_event({app.key_enter_, input::InputState::Pressed, app.native_dialog_closed_ns_ - 1});
        report.check("dialog.delayed_stale_edge_ignored", !app.profile_nickname_edit_active_);
        app.native_dialog_held_keys_.insert(app.key_enter_);
        key(app, "Enter");
        report.check("dialog.held_close_key_ignored", !app.profile_nickname_edit_active_);
        key(app, "Enter");
        report.check("dialog.fresh_press_after_release", app.profile_nickname_edit_active_);
        key(app, "Escape");
        click(app, Kind::SettingsRow, profile_setup::kSkipStartCountdownRow, Part::SelectOnly);
        key(app, "Enter");
        report.check("profile.skip_countdown_toggle", app.config_.ui.skip_start_countdown);
        click(app, Kind::SettingsRow, profile_setup::kInstantRestartKeyRow);
        report.check("profile.restart_key_capture", app.profile_control_key_capture_active_);
        key(app, "F9"); key(app, "F10"); key(app, "F11");
        report.check("profile.restart_reserved_rejected", app.profile_control_key_capture_active_ &&
            !app.profile_control_key_status_.empty() && !app.ranked_account_overlay_visible_ && !app.chat_overlay_visible_);
        const auto bindings = config::KeymapManager{}.bindings_for_mode(app.keymap_, "10k");
        if (!bindings.empty()) key(app, bindings.begin()->second.c_str());
        report.check("profile.restart_lane_conflict", app.profile_control_key_capture_active_ &&
            !app.profile_control_key_status_.empty());
        for (const char* modifier : {"LControl", "LAlt"}) {
            const auto code = config::KeycodeMap::to_keycode(modifier).value();
            app.handle_input_event({code, input::InputState::Pressed, timing::HighResClock::now_ns()});
            key(app, "F12");
            report.check(std::string("profile.restart_rejects_chord_") + modifier,
                app.profile_control_key_capture_active_ && app.config_.ui.instant_restart_key.empty());
            app.handle_input_event({code, input::InputState::Released, timing::HighResClock::now_ns()});
        }
        key(app, "F12");
        report.check("profile.restart_key_assigned", !app.profile_control_key_capture_active_ &&
            app.config_.ui.instant_restart_key == "F12");
        key(app, "Enter"); key(app, "Esc");
        report.check("profile.restart_capture_cancel", !app.profile_control_key_capture_active_ &&
            app.config_.ui.instant_restart_key == "F12" && screen(app) == Screen::QuickSetup);
        key(app, "Enter"); key(app, "Delete");
        report.check("profile.restart_capture_clear", !app.profile_control_key_capture_active_ &&
            app.config_.ui.instant_restart_key.empty());
        key(app, "Enter"); key(app, "F12");
        const auto saved = config::ConfigLoader{}.load_profile(app.profile_dir_);
        report.check("profile.start_controls_readback", saved.success() && saved.config.ui.skip_start_countdown &&
            saved.config.ui.instant_restart_key == "F12");
        // Preserve the existing ordinary-start countdown smoke below.
        click(app, Kind::SettingsRow, profile_setup::kSkipStartCountdownRow, Part::Decrement);
        report.check("profile.start_controls_restored", !app.config_.ui.skip_start_countdown);
        click(app, Kind::SettingsRow, profile_setup::kInstantRestartKeyRow);
        app.replace_screen(Screen::QuickSetup);
        report.check("profile.capture_cleared_on_navigation", !app.profile_control_key_capture_active_);
        app.publish_snapshot();
        render::MenuRenderData profile_view;
        app.populate_quick_setup_render_data(profile_view);
        report.check("profile.has_assignable_speed_rows", profile_view.generic.rows.size() ==
            static_cast<std::size_t>(profile_setup::row_count(profile_setup::Entry::Options)));
        app.menu_window_.request_screenshot();
        std::string detail;
        report.check("profile.final_controls_present", present(app, detail), detail);
    }
    static bool resize_owned_window(int width, int height) {
        // Only the fixture's own window receives this native sizing sequence.
        // It verifies the WM_SIZE/drag-end mailbox and profile path, not physical dragging.
        HWND owned = nullptr;
        EnumWindows([](HWND hwnd, LPARAM output) -> BOOL {
            DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
            wchar_t type[64]{}; GetClassNameW(hwnd, type, 64);
            if (pid == GetCurrentProcessId() && std::wstring(type) == L"TenRiffMenuWindow") {
                *reinterpret_cast<HWND*>(output) = hwnd; return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&owned));
        bool resized = false;
        if (owned) {
            RECT outer{}, client{};
            GetWindowRect(owned, &outer); GetClientRect(owned, &client);
            SendMessageW(owned, WM_ENTERSIZEMOVE, 0, 0);
            resized = SetWindowPos(owned, nullptr, 0, 0,
                width + outer.right - outer.left - client.right,
                height + outer.bottom - outer.top - client.bottom,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
            SendMessageW(owned, WM_EXITSIZEMOVE, 0, 0);
            GetClientRect(owned, &client);
            resized = resized && client.right == width && client.bottom == height;
        }
        return resized;
    }
    static void check_result_screenshot(MenuApp& app, Report& report, const std::string& page) {
        const auto directory = std::filesystem::current_path() / "screenshots";
        std::vector<std::filesystem::path> previous;
        if (std::filesystem::exists(directory)) {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) previous.push_back(entry.path());
        }
        std::string detail;
        const bool live = present(app, detail);  // Let the selected result page reach the renderer first.
        key(app, "F9");
        const auto deadline = Clock::now() + std::chrono::seconds(3);
        bool complete = false;
        while (Clock::now() < deadline && !complete) {
            if (std::filesystem::exists(directory)) {
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.path().extension() != ".png" ||
                        std::find(previous.begin(), previous.end(), entry.path()) != previous.end()) continue;
                    std::ifstream input(entry.path(), std::ios::binary | std::ios::ate);
                    if (!input || input.tellg() < 32) continue;
                    std::array<unsigned char, 8> header{}, end{};
                    input.seekg(0); input.read(reinterpret_cast<char*>(header.data()), header.size());
                    input.seekg(-12, std::ios::end); input.read(reinterpret_cast<char*>(end.data()), end.size());
                    complete = input.good() && header == std::array<unsigned char, 8>{137,80,78,71,13,10,26,10} &&
                        end == std::array<unsigned char, 8>{0,0,0,0,73,69,78,68};
                    if (complete) { detail = entry.path().filename().string(); break; }
                }
            }
            if (!complete) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        report.check("result.screenshot_" + page, live && complete && screen(app) == Screen::Result, detail);
    }
    static void check_analysis_boundaries(MenuApp& app, Report& report) {
        const auto saved_lanes = app.last_result_.analysis.lane_count;
        app.last_result_.analysis.lane_count = 32;  // Loaded replay evidence can exceed menu key modes.
        app.result_analysis_page_ = 1;
        app.result_analysis_lane_bank_ = 0;
        click(app, render::MenuHitTargetKind::SettingsRow, 7);
        report.check("result.analysis_lane_bank_mouse", app.result_analysis_lane_bank_ == 1);
        key(app, "Down");
        report.check("result.analysis_lane_bank_keyboard", app.result_analysis_lane_bank_ == 0);
        app.last_result_.analysis.lane_count = saved_lanes;
        app.result_analysis_page_ = 0;
        const bool saved_multiplayer = app.last_game_was_multiplayer_;
        app.last_game_was_multiplayer_ = true;
        key(app, "F2");
        click(app, render::MenuHitTargetKind::SettingsRow, 4);
        report.check("result.analysis_multiplayer_guard", app.result_analysis_page_ == 0);
        app.last_game_was_multiplayer_ = saved_multiplayer;
        app.publish_snapshot();
    }
    static void check_ln_mix_record_boundaries(MenuApp& app, Report& report) {
        const auto saved_records = app.local_play_records_;
        const auto saved_best = app.chart_best_results_;
        const auto saved_indices = app.chart_play_record_indices_;
        const auto saved_current = app.current_song_record_indices_;
        const auto saved_mods = app.config_.mode.mods;
        app.local_play_records_.clear();
        app.chart_best_results_.clear();
        app.chart_play_record_indices_.clear();
        menu_records::ParsedResultRecord record;
        record.chart_path = app.last_chart_path_;
        record.mods = {"ln_mix_30"};
        record.stats = app.last_result_;
        record.long_note_mix_version = 1;
        ReplayVerificationResult verified;
        verified.status = ReplayVerificationStatus::Verified;
        verified.official_eligible = true;
        verified.stats = record.stats;
        verified.final_score = 100;
        verified.clear_status = "CLEAR";
        verified.final_gauge = "normal";
        app.append_chart_result_record(record, "fixture-v1-result", "fixture-v1-replay", verified);
        const auto legacy_best = app.chart_best_results_;
        report.check("records.legacy_ln_best_retained", !legacy_best.empty());
        record.long_note_mix_version = 2;
        verified.final_score = 200;
        // Deliberately present an eligible claim: the chart-best storage itself
        // must still refuse to mix conversion versions, independently of verification.
        app.append_chart_result_record(record, "fixture-v2-result", "fixture-v2-replay", verified);
        bool best_unchanged = app.chart_best_results_.size() == legacy_best.size();
        for (const auto& [key, best] : legacy_best) {
            const auto found = app.chart_best_results_.find(key);
            best_unchanged = best_unchanged && found != app.chart_best_results_.end() &&
                found->second.best_score == best.best_score;
        }
        report.check("records.long_ln_history_without_legacy_best_overwrite",
            best_unchanged && app.local_play_records_.size() == 2 &&
            app.local_play_records_.back().long_note_mix_version == 2);
        app.config_.mode.mods = record.mods;
        app.current_song_record_indices_ = {0};
        report.check("records.auto_ghost_skips_legacy_mix", app.best_replay_path_for_selected_song().empty());
        app.current_song_record_indices_ = {0, 1};
        report.check("records.auto_ghost_uses_current_mix", app.best_replay_path_for_selected_song() == "fixture-v2-replay");
        record.mods.clear();
        record.bms_timing_version = 2;
        verified.final_score = 50;
        app.append_chart_result_record(record, "timing-v2-result", "timing-v2-replay", verified);
        bool corrected_best = !app.chart_best_results_.empty();
        for (const auto& [key, best] : app.chart_best_results_)
            corrected_best &= best.bms_timing_version == 2 && best.best_score == 50;
        report.check("records.corrected_timing_starts_separate_best", corrected_best);
        record.bms_timing_version = 1;
        verified.final_score = 300;
        app.append_chart_result_record(record, "timing-v1-result", "timing-v1-replay", verified);
        for (const auto& [key, best] : app.chart_best_results_)
            corrected_best &= best.bms_timing_version == 2 && best.best_score == 50;
        report.check("records.legacy_timing_cannot_replace_corrected_best", corrected_best);
        app.config_.mode.mods.clear();
        app.current_song_record_indices_ = {3, 2}; // Higher legacy score first.
        report.check("records.ghost_prefers_corrected_timing", app.best_replay_path_for_selected_song() == "timing-v2-replay");
        SongEntry alias_entry;
        alias_entry.path = "alias-fixture/chart.bms";
        const auto alias_keys = menu_songs::build_chart_path_keys(alias_entry.path, app.songs_path_);
        report.check("records.timing_alias_fixture_has_two_keys", alias_keys.size() >= 2);
        if (alias_keys.size() >= 2) {
            for (int order = 0; order < 2; ++order) {
                auto legacy = app.chart_best_results_.begin()->second;
                auto corrected = legacy;
                legacy.bms_timing_version = 1; legacy.best_score = 300;
                corrected.bms_timing_version = 2; corrected.best_score = 50;
                app.chart_best_results_[alias_keys[order]] = legacy;
                app.chart_best_results_[alias_keys[1 - order]] = corrected;
                const auto best = app.best_result_for_song_entry(alias_entry);
                report.check("records.timing_alias_prefers_corrected_" + std::to_string(order),
                    best.bms_timing_version == 2 && best.best_score == 50);
            }
        }
        app.local_play_records_ = saved_records;
        app.chart_best_results_ = saved_best;
        app.chart_play_record_indices_ = saved_indices;
        app.current_song_record_indices_ = saved_current;
        app.config_.mode.mods = saved_mods;
    }
    static void check_table_loading_present(MenuApp& app, Report& report) {
        const auto saved_path = app.config_.ui.difficulty_table_path;
        const auto saved_url = app.config_.ui.difficulty_table_url;
        const auto saved_input = app.difficulty_table_url_input_;
        const auto saved_status = app.song_browser_status_message_;
        const auto saved_display_path = app.difficulty_table_display_path_;
        const auto saved_display_name = app.difficulty_table_display_name_;
        const bool saved_editing = app.difficulty_table_url_editing_;
        const bool saved_loading = app.difficulty_table_loading_;
        const bool saved_loading_snapshot = app.difficulty_table_loading_snapshot_;
        const auto restore = [&] {
            app.config_.ui.difficulty_table_path = saved_path;
            app.config_.ui.difficulty_table_url = saved_url;
            app.difficulty_table_url_input_ = saved_input;
            app.song_browser_status_message_ = saved_status;
            app.difficulty_table_display_path_ = saved_display_path;
            app.difficulty_table_display_name_ = saved_display_name;
            app.difficulty_table_url_editing_ = saved_editing;
            app.difficulty_table_loading_ = saved_loading;
            app.difficulty_table_loading_snapshot_ = saved_loading_snapshot;
            app.publish_snapshot();
        };
        try {
            for (const bool editor : {false, true}) {
                const std::string prefix = editor ? "table.loading_editor" : "table.loading_card";
                app.difficulty_table_url_editing_ = editor;
                app.difficulty_table_url_input_ = "https://fixture.invalid/loading.json";
                bool fetched = false;
                const bool applied = app.apply_difficulty_table_url(app.difficulty_table_url_input_,
                    [&](std::string_view, std::size_t) {
                        fetched = true;
                        bool published = false;
                        {
                            std::lock_guard<std::mutex> lock(app.snapshot_mutex_);
                            const auto& table = app.snapshot_.render.song_select;
                            published = table.difficulty_table_name == app.ui_text("Loading...", "로딩 중") &&
                                table.difficulty_table_status == app.ui_text("Loading...", "로딩 중") &&
                                table.difficulty_table_editing == editor &&
                                app.snapshot_.render.loading_progress.visible &&
                                app.snapshot_.render.loading_progress.percent == -1;
                        }
                        report.check(prefix + ".published_before_fetch", published);
                        const auto before = present_probe.table_loading_presents.load(std::memory_order_acquire);
                        const auto began = Clock::now();
                        std::string detail;
                        const bool live = present(app, detail);
                        const auto loading_frames = present_probe.table_loading_presents.load(std::memory_order_acquire) - before;
                        report.check(prefix + ".present_during_blocking_fetch", live && loading_frames > 0,
                            detail + "; loading Presents=" + std::to_string(loading_frames));

                        // Capture only this app's render target, keeping all previous PNGs.
                        const auto directory = std::filesystem::current_path() / "screenshots";
                        std::vector<std::filesystem::path> previous;
                        if (std::filesystem::exists(directory))
                            for (const auto& entry : std::filesystem::directory_iterator(directory)) previous.push_back(entry.path());
                        app.menu_window_.request_screenshot();
                        const auto deadline = Clock::now() + std::chrono::seconds(3);
                        bool complete = false;
                        std::string filename;
                        while (Clock::now() < deadline && !complete) {
                            if (std::filesystem::exists(directory)) {
                                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                                    if (entry.path().extension() != ".png" ||
                                        std::find(previous.begin(), previous.end(), entry.path()) != previous.end()) continue;
                                    std::ifstream input(entry.path(), std::ios::binary | std::ios::ate);
                                    if (!input || input.tellg() < 32) continue;
                                    std::array<unsigned char, 8> header{}, end{};
                                    input.seekg(0); input.read(reinterpret_cast<char*>(header.data()), header.size());
                                    input.seekg(-12, std::ios::end); input.read(reinterpret_cast<char*>(end.data()), end.size());
                                    complete = input.good() && header == std::array<unsigned char, 8>{137,80,78,71,13,10,26,10} &&
                                        end == std::array<unsigned char, 8>{0,0,0,0,73,69,78,68};
                                    if (complete) { filename = entry.path().filename().string(); break; }
                                }
                            }
                            if (!complete) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        report.check(prefix + ".screenshot_during_fetch", complete && app.difficulty_table_loading_,
                            filename + "; blocked_ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - began).count()));
                        DifficultyTableHttpResponse response;
                        response.error = "Intentional offline loading fixture";
                        return response;
                    });
                report.check(prefix + ".cleared_after_failure", fetched && !applied && !app.difficulty_table_loading_ &&
                    app.snapshot_.render.song_select.difficulty_table_name != app.ui_text("Loading...", "로딩 중") &&
                    app.config_.ui.difficulty_table_path == saved_path && app.config_.ui.difficulty_table_url == saved_url);
            }
        } catch (...) {
            restore();
            throw;
        }
        restore();
    }
    static void check_table_input_and_filter(MenuApp& app, Report& report) {
        const auto saved_url = app.difficulty_table_url_input_;
        app.difficulty_table_url_editing_ = true;
        app.difficulty_table_url_input_ = u8"https://fixture.invalid/가漢😀";
        app.handle_input_event({app.key_backspace_, input::InputState::Pressed, timing::HighResClock::now_ns()});
        const auto due = app.song_select_repeat_next_ns_;
        const bool first = app.difficulty_table_url_input_ == u8"https://fixture.invalid/가漢";
        app.service_song_select_repeat(due, true);
        report.check("table.url_utf8_hold_repeat", first && app.difficulty_table_url_input_ == u8"https://fixture.invalid/가");
        app.handle_input_event({app.key_backspace_, input::InputState::Released, timing::HighResClock::now_ns()});
        app.service_song_select_repeat(due + 1'000'000'000LL, true);
        report.check("table.url_release_stops", app.song_select_repeat_key_ == 0 && app.difficulty_table_url_input_ == u8"https://fixture.invalid/가");
        key(app, "Esc");
        report.check("table.url_cancel", !app.difficulty_table_url_editing_);
        app.difficulty_table_url_input_ = saved_url;

        if (!app.indexed_songs_.empty()) {
            const auto saved_entry = app.indexed_songs_.front();
            const auto saved_path = app.config_.ui.difficulty_table_path;
            const int saved_min = app.song_level_min_filter_, saved_max = app.song_level_max_filter_;
            auto& entry = app.indexed_songs_.front();
            entry.level = 50;
            entry.difficulty_table_level = "st12";
            app.config_.ui.difficulty_table_path = "fixture-table-filter.json";
            app.song_level_min_filter_ = 12;
            app.song_level_max_filter_ = 12;
            app.rebuild_visible_song_list();
            report.check("table.filter_uses_prefixed_level", app.visible_song_count() == 1);
            entry.difficulty_table_level = "st11";
            app.rebuild_visible_song_list();
            report.check("table.filter_excludes_lower_table_level", app.visible_song_count() == 0);
            entry.difficulty_table_level = "?";
            app.rebuild_visible_song_list();
            report.check("table.filter_never_falls_back_to_native", app.visible_song_count() == 0);
            app.config_.ui.difficulty_table_path.clear();
            app.song_level_max_filter_ = 0;
            app.rebuild_visible_song_list();
            report.check("table.native_filter_restored", app.visible_song_count() == 1);
            entry = saved_entry;
            app.config_.ui.difficulty_table_path = saved_path;
            app.song_level_min_filter_ = saved_min;
            app.song_level_max_filter_ = saved_max;
            app.rebuild_visible_song_list();
        }
        app.publish_snapshot();
    }
    static void check_level_search_input(MenuApp& app, Report& report) {
        const auto saved_songs = app.indexed_songs_;
        const auto saved_ui = app.config_.ui;
        const auto saved_speed = app.config_.speed;
        const auto saved_sort = app.song_sort_mode_;
        const auto saved_group = app.song_group_mode_;
        const auto saved_view = app.song_select_view_;
        const auto saved_focus = app.song_select_focus_;
        const auto saved_query = app.song_search_query_;
        const bool saved_search_active = app.song_select_search_active_;
        const int saved_nav = app.song_select_nav_cursor_;
        const int saved_key = app.song_key_filter_, saved_min = app.song_level_min_filter_, saved_max = app.song_level_max_filter_;
        const auto saved_pressed = app.pressed_keys_;
        const auto saved_owners = app.menu_key_owners_;
        const std::string saved_path = app.selected_song_path();
        app.pressed_keys_.clear();
        app.menu_key_owners_.clear();
        app.config_.ui.difficulty_table_path = "fixture-level-search.json";
        app.config_.ui.all_song_sources = true;
        app.config_.ui.song_collection_filter = "all";
        app.song_key_filter_ = app.song_level_min_filter_ = app.song_level_max_filter_ = 0;
        app.song_group_mode_ = MenuApp::SongGroupMode::None;
        app.song_sort_mode_ = MenuApp::SongSortMode::DifficultyDesc;
        app.indexed_songs_.clear();
        for (const int level : {15, 16, 18, 19, 20, 21}) {
            SongEntry entry;
            entry.title = level == 20 ? "Blue Fixture" : "Other Fixture";
            entry.path = "search-" + std::to_string(level) + ".bms";
            entry.format = "bms";
            entry.key_count = 7;
            entry.level = entry.native_level = 50;
            entry.difficulty_table_level = "st" + std::to_string(level);
            entry.difficulty_table_order = level;
            app.indexed_songs_.push_back(std::move(entry));
        }
        for (const std::string label : {"?", ""}) {
            SongEntry entry;
            entry.title = "Unranked Fixture";
            entry.path = label.empty() ? "search-nonmember.bms" : "search-unranked.bms";
            entry.format = "bms";
            entry.key_count = 7;
            entry.level = entry.native_level = 18;
            entry.difficulty_table_level = label;
            app.indexed_songs_.push_back(std::move(entry));
        }
        app.song_search_query_.clear();
        app.song_select_search_active_ = false;
        app.song_select_view_ = MenuApp::SongSelectView::Songs;
        app.song_select_focus_ = MenuApp::SongSelectFocus::LeftNav;
        app.song_select_nav_cursor_ = 2;
        app.rebuild_visible_song_list();
        key(app, "Enter");
        report.check("search.enter_starts_editing", app.song_select_search_active_);

        const auto chord = [&](const char* modifier, const char* name) {
            const auto code = config::KeycodeMap::to_keycode(modifier).value();
            app.handle_input_event({code, input::InputState::Pressed, timing::HighResClock::now_ns()});
            key(app, name);
            app.handle_input_event({code, input::InputState::Released, timing::HighResClock::now_ns()});
        };
        const auto type_query = [&](std::string_view query, const char* shift = "LShift") {
            key(app, "Delete");
            for (const char ch : query) {
                if (ch == ':') chord(shift, "Semicolon");
                else if (ch == '+') chord(shift, "Plus");
                else if (ch == '-') key(app, "Minus");
                else if (ch == ' ') key(app, "Space");
                else {
                    const char name[]{static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch), 0};
                    key(app, name);
                }
            }
        };
        const auto table_levels = [&]() {
            std::vector<int> result;
            for (std::size_t i = 0; i < app.visible_song_count(); ++i)
                result.push_back(app.visible_song_entry(i)->difficulty_table_order);
            return result;
        };
        type_query("lv:16to20");
        report.check("search.raw_range_inclusive_preserves_sort", app.song_search_query_ == "lv:16to20" &&
            app.song_select_search_active_ && table_levels() == std::vector<int>({20, 19, 18, 16}) &&
            app.song_sort_mode_ == MenuApp::SongSortMode::DifficultyDesc);
        app.handle_text_input("lv:16to20");
        report.check("search.wm_char_does_not_duplicate_raw_keys", app.song_search_query_ == "lv:16to20");
        type_query("lv:16", "RShift");
        report.check("search.raw_exact_right_shift", app.song_search_query_ == "lv:16" &&
            app.song_select_search_active_ && table_levels() == std::vector<int>({16}));
        type_query("lv:19-");
        report.check("search.raw_upper_inclusive", app.song_search_query_ == "lv:19-" &&
            app.song_select_search_active_ && table_levels() == std::vector<int>({19, 18, 16, 15}),
            "query=" + app.song_search_query_ + "; minus=" +
            config::KeycodeMap::to_name(config::KeycodeMap::to_keycode("Minus").value()));
        type_query("lv:18+", "RShift");
        report.check("search.raw_lower_inclusive", app.song_search_query_ == "lv:18+" &&
            app.song_select_search_active_ && table_levels() == std::vector<int>({21, 20, 19, 18}));
        type_query("blue lv:16to20");
        report.check("search.raw_text_and_level_intersect", app.song_search_query_ == "blue lv:16to20" &&
            app.song_select_search_active_ && table_levels() == std::vector<int>({20}));
        type_query("lv:18");
        report.check("search.table_excludes_native_only_matches", table_levels() == std::vector<int>({18}));
        app.config_.ui.difficulty_table_path.clear();
        app.rebuild_visible_song_list();
        report.check("search.native_selection_uses_native_level", app.visible_song_count() == 2 &&
            app.visible_song_entry(0)->level == 18 && app.visible_song_entry(1)->level == 18);
        type_query("lv:50");
        report.check("search.native_query_ignores_retained_table_labels", app.visible_song_count() == 6);
        app.song_level_max_filter_ = 19;
        app.rebuild_visible_song_list();
        report.check("search.existing_level_filter_still_intersects", app.visible_song_count() == 0);
        app.song_level_max_filter_ = 0;
        for (const char* modifier : {"LShift", "RShift", "LControl", "RControl", "LAlt", "RAlt", "LWin", "RWin"}) {
            key(app, modifier);
            report.check(std::string("search.modifier_keeps_editing.") + modifier,
                app.song_select_search_active_ && app.song_search_query_ == "lv:50");
        }
        key(app, "Delete"); key(app, "Semicolon"); key(app, "Plus");
        report.check("search.unshifted_punctuation_is_literal", app.song_search_query_ == ";=" && app.song_select_search_active_);
        report.check("search.punctuation_does_not_change_speed", app.config_.speed.rate == saved_speed.rate &&
            app.config_.speed.hi_speed == saved_speed.hi_speed);
        key(app, "Esc");
        report.check("search.escape_finishes_editing", !app.song_select_search_active_ && screen(app) == Screen::SongSelect);

        app.indexed_songs_ = saved_songs;
        app.config_.ui = saved_ui;
        app.config_.speed = saved_speed;
        app.song_sort_mode_ = saved_sort; app.song_group_mode_ = saved_group;
        app.song_key_filter_ = saved_key; app.song_level_min_filter_ = saved_min; app.song_level_max_filter_ = saved_max;
        app.song_search_query_ = saved_query; app.song_select_search_active_ = saved_search_active;
        app.song_select_view_ = saved_view; app.song_select_focus_ = saved_focus; app.song_select_nav_cursor_ = saved_nav;
        app.pressed_keys_ = saved_pressed; app.menu_key_owners_ = saved_owners;
        app.rebuild_visible_song_list(&saved_path);
        app.persist_runtime_config();
        app.publish_snapshot();
    }
    static void check_lane_shortcuts(MenuApp& app, Report& report) {
        const auto saved_controller = app.skin_settings_controller_;
        const auto shortcut_state = [&]() {
            std::string state = "mode=" + std::string(app.skin_settings_controller_.edit_mode()) +
                "; lane=" + std::to_string(app.skin_settings_controller_.edit_lane()) +
                "; capture=" + std::to_string(app.keymap_settings_controller_.capture_active()) +
                "; preview=" + std::to_string(app.skin_preview_fullscreen_) +
                "; help=" + std::to_string(app.help_overlay_visible_) +
                "; chat=" + std::to_string(app.chat_overlay_visible_) +
                "; account=" + std::to_string(app.ranked_account_overlay_visible_) +
                "; warning=" + std::to_string(app.chat_url_warning_visible_) +
                "; nickname=" + std::to_string(app.profile_nickname_edit_active_) +
                "; table_url=" + std::to_string(app.difficulty_table_url_editing_) +
                "; records_url=" + std::to_string(app.online_records_url_editing_);
            state += "; held=";
            for (const auto key : app.pressed_keys_) state += config::KeycodeMap::to_name(key) + ",";
            state += "; owners=";
            for (const auto& [physical, alias] : app.menu_key_owners_)
                state += config::KeycodeMap::to_name(physical) + ":" + config::KeycodeMap::to_name(alias) + ",";
            state += "; physical_modifiers=";
            for (const auto key : {VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL,
                                   VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN})
                if ((GetAsyncKeyState(key) & 0x8000) != 0) state += std::to_string(key) + ",";
            return state;
        };
        render::MenuRenderData skin_view;
        app.populate_skin_settings_render_data(skin_view);
        bool removed = true;
        bool adjacent = false;
        for (std::size_t i = 0; i < skin_view.generic.rows.size(); ++i) {
            const auto id = static_cast<SkinSettingsRowId>(skin_view.generic.rows[i].row_index);
            removed = removed && id != SkinSettingsRowId::TargetGap && id != SkinSettingsRowId::VisualPreset &&
                id != SkinSettingsRowId::IndicatorBackground && id != SkinSettingsRowId::ExportPreset;
            if (id == SkinSettingsRowId::TargetLane && i + 1 < skin_view.generic.rows.size())
                adjacent = skin_view.generic.rows[i + 1].row_index == static_cast<int>(SkinSettingsRowId::LaneColor);
        }
        report.check("skin.retired_rows_absent", removed);
        report.check("skin.target_lane_above_note_color", adjacent);
        app.skin_settings_controller_.reset("16k");
        (void)app.skin_settings_controller_.select(SkinSettingsRowId::LaneWidth, false);
        key(app, "0");
        report.check("skin.lane_zero_selects_ten", app.skin_settings_controller_.edit_lane() == 9, shortcut_state());
        const auto shift = config::KeycodeMap::to_keycode("RShift").value();
        app.handle_input_event({shift, input::InputState::Pressed, timing::HighResClock::now_ns()});
        key(app, "6");
        app.handle_input_event({shift, input::InputState::Released, timing::HighResClock::now_ns()});
        report.check("skin.lane_shift_six_selects_sixteen", app.skin_settings_controller_.edit_lane() == 15 &&
            app.skin_settings_controller_.selected_id() == SkinSettingsRowId::LaneWidth && !app.skin_settings_controller_.dirty(),
            shortcut_state());
        key(app, "Numpad2");
        report.check("skin.lane_numpad_selects_two", app.skin_settings_controller_.edit_lane() == 1, shortcut_state());
        std::string detail;
        report.check("skin.lane_shortcut_present", present(app, detail), std::move(detail));
        app.skin_settings_controller_ = saved_controller;
        app.publish_snapshot();
    }
    static void check_hud_feedback_followup(MenuApp& app, Report& report) {
        const auto saved_skin = app.config_.skin;
        MenuApp::GameplayHudState saved_hud;
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            saved_hud = app.gameplay_hud_;
            app.reset_gameplay_hud_state(app.gameplay_hud_);
            app.gameplay_hud_.has_retained_timing_feedback = true;
            app.gameplay_hud_.timing_feedback_delta_ms = -28;
            app.gameplay_hud_.ghost_has_retained_timing_feedback = true;
            app.gameplay_hud_.ghost_timing_feedback_delta_ms = 31;
            app.gameplay_hud_.measure_line_count = 2;
            app.gameplay_hud_.measure_line_positions[0] = 1.25;
            app.gameplay_hud_.measure_line_positions[1] = 1.75;
        }
        app.config_.skin.source = "native";
        app.config_.skin.timing_feedback_override = true;
        app.config_.skin.timing_bar_always_visible = true;
        app.config_.skin.show_timing_feedback = true;
        app.config_.skin.show_timing_bar = false;
        render::GameplayHudData hud;
        app.populate_gameplay_render_data(hud);
        report.check("hud.always_keeps_expired_real_timing", hud.has_timing_feedback &&
            hud.timing_feedback_delta_ms == -28 && hud.ghost_has_timing_feedback &&
            hud.ghost_timing_feedback_delta_ms == 31);
        report.check("hud.always_preserves_independent_text_bar", hud.show_timing_feedback && !hud.show_timing_bar);
        report.check("hud.measure_positions_reach_render", hud.measure_line_count == 2 &&
            hud.measure_line_positions[0] == 1.25 && hud.measure_line_positions[1] == 1.75);
        app.config_.skin.timing_bar_always_visible = false;
        app.populate_gameplay_render_data(hud);
        report.check("hud.conditional_expires_real_timing", !hud.has_timing_feedback && !hud.ghost_has_timing_feedback);
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            app.gameplay_hud_.has_retained_timing_feedback = false;
            app.gameplay_hud_.ghost_has_retained_timing_feedback = false;
        }
        app.config_.skin.timing_bar_always_visible = true;
        app.populate_gameplay_render_data(hud);
        report.check("hud.always_never_fabricates_initial_fast", !hud.has_timing_feedback && !hud.ghost_has_timing_feedback);
        app.config_.skin = saved_skin;
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            app.gameplay_hud_ = std::move(saved_hud);
        }
    }
    static void check_launch_level_metadata(MenuApp& app, Report& report) {
        const auto saved_path = app.last_chart_path_;
        const auto saved_title = app.last_chart_title_;
        const auto saved_artist = app.last_chart_artist_;
        const auto saved_level = app.last_chart_difficulty_label_;
        const auto saved_entry = app.last_chart_entry_;
        const bool saved_valid = app.last_chart_entry_valid_;
        const double saved_bpm = app.last_chart_bpm_;
        const auto saved_screen = app.current_screen();
        const auto directory = std::filesystem::absolute("hud-level-fixture");
        std::filesystem::create_directory(directory);
        const auto original = directory / "original.bms";
        { std::ofstream file(original, std::ios::binary);
          file << "#TITLE Practice level\n#BPM 120\n#PLAYLEVEL 17\n#4K\n#00111:01\n"; }
        BmsEditorDocument editor;
        std::string error;
        const bool loaded = editor.load_file(original.u8string(), &error);
        report.check("hud.level_editor_fixture_loaded", loaded, error);
        if (loaded) {
            (void)editor.set_base_bpm(135.0); // Serialize an unsaved editor change.
            const auto practice = directory / ".tenriff-practice-fixture.bms";
            { std::ofstream file(practice, std::ios::binary); file << editor.serialize_text(&error); }
            SongEntry unrelated;
            unrelated.path = original.u8string();
            unrelated.level = 88;
            unrelated.difficulty_table_symbol = "TABLE ";
            unrelated.difficulty_table_level = "99";
            app.update_last_chart_metadata(practice.u8string(), &unrelated);
            report.check("hud.level_practice_uses_serialized_target", !app.last_chart_entry_valid_ &&
                app.last_chart_difficulty_label_ == "LV 17");
            app.reset_screen(Screen::Gameplay);
            app.publish_snapshot();
            {
                std::lock_guard<std::mutex> lock(app.snapshot_mutex_);
                report.check("hud.level_practice_reaches_gameplay_snapshot",
                    app.snapshot_.render.gameplay.difficulty_label == "LV 17");
            }
            app.update_last_chart_metadata(original.u8string(), nullptr);
            report.check("hud.level_unindexed_direct_launch", !app.last_chart_entry_valid_ &&
                app.last_chart_difficulty_label_ == "LV 17");
            app.update_last_chart_metadata(original.u8string(), &unrelated);
            // Table symbols are sanitized separately, including trailing spaces.
            report.check("hud.level_matching_index_retains_table", app.last_chart_entry_valid_ &&
                app.last_chart_difficulty_label_ == "TABLE99", app.last_chart_difficulty_label_);
            app.update_last_chart_metadata((directory / "missing.bms").u8string(), &unrelated);
            report.check("hud.level_missing_target_cannot_reuse_selection", !app.last_chart_entry_valid_ &&
                app.last_chart_difficulty_label_.empty());
        }
        app.last_chart_path_ = saved_path;
        app.last_chart_title_ = saved_title;
        app.last_chart_artist_ = saved_artist;
        app.last_chart_difficulty_label_ = saved_level;
        app.last_chart_entry_ = saved_entry;
        app.last_chart_entry_valid_ = saved_valid;
        app.last_chart_bpm_ = saved_bpm;
        app.reset_screen(saved_screen);
        app.publish_snapshot();
    }
    static void check_loading_pacing(MenuApp& app, Report& report) {
        const auto saved_refresh = app.config_.graphics.refresh_hz;
        app.config_.graphics.refresh_hz = 0;
        app.reset_screen(Screen::Gameplay);
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            app.reset_gameplay_hud_state(app.gameplay_hud_);
        }
        app.update_gameplay_loading_state(56, "Parsing chart");
        report.check("loading.render_cap_60", app.current_render_config().fps_limit == 60);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto previous = present_probe.successful_presents.load(std::memory_order_acquire);
        const auto deadline = Clock::now() + std::chrono::milliseconds(600);
        std::this_thread::sleep_until(deadline);
        const auto presents = present_probe.successful_presents.load(std::memory_order_acquire) - previous;
        report.check("loading.bounded_actual_presents", presents >= 10 && presents <= 42,
                     "Render-thread successful Presents over 600 ms=" + std::to_string(presents));
        app.reset_screen(Screen::SongSelect);
        app.config_.graphics.refresh_hz = saved_refresh;
        {
            std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
            app.reset_gameplay_hud_state(app.gameplay_hud_);
        }
        app.apply_runtime_graphics_config();
        app.publish_snapshot();
        report.check("loading.lobby_pacing_restored", app.current_render_config().fps_limit > 60);
    }
    struct Observation {
        bool loading = false, countdown = false, playing = false;
        int64_t sample = 0;
        int64_t presented_ns = 0;
        bool measure_lines = false;
    };
    static Observation observe(MenuApp& app) {
        std::lock_guard<std::mutex> lock(app.gameplay_hud_mutex_);
        const auto& hud = app.gameplay_hud_;
        return {hud.loading, hud.countdown_active,
                hud.active && !hud.loading && !hud.countdown_active && hud.current_sample > 0,
                hud.current_sample, present_probe.last_completion_ns.load(std::memory_order_acquire),
                hud.measure_line_count > 0 && std::isfinite(hud.measure_line_positions[0])};
    }
};

struct MenuAppFeedbackTestAccess {
    static void check_gamepad_session(GameSession& session, Report& report) {
        const auto a = config::KeycodeMap::to_keycode("PadA").value();
        const auto lt = config::KeycodeMap::to_keycode("PadLT").value();
        const auto lb = config::KeycodeMap::to_keycode("PadLB").value();
        const auto rb = config::KeycodeMap::to_keycode("PadRB").value();
        report.check("xinput.session_resolves_bindings", session.key_to_lane_.count(a) &&
            session.key_to_lane_.count(lt) && session.f5_keycode_ == lb && session.f6_keycode_ == rb);
        const double initial = session.config_.speed.hi_speed;
        static_cast<void>(session.handle_control_input({rb, input::InputState::Pressed, 1}));
        static_cast<void>(session.handle_control_input({rb, input::InputState::Pressed, 2}));
        report.check("xinput.coarse_speed_once", session.config_.speed.hi_speed == std::min(50.0, initial * 2.0));
        static_cast<void>(session.handle_control_input({rb, input::InputState::Released, 3}));
        static_cast<void>(session.handle_control_input({lb, input::InputState::Pressed, 4}));
        report.check("xinput.coarse_speed_decrease", session.config_.speed.hi_speed == initial);
        static_cast<void>(session.handle_control_input({lb, input::InputState::Released, 5}));
        // The portable native-state decoder feeds the same binding aggregator
        // used by gameplay; both primary and secondary ownership are retained.
        input::gamepad::GamepadTracker tracker;
        std::array<std::optional<input::gamepad::GamepadState>, 4> states{};
        states[0] = input::gamepad::GamepadState{};
        static_cast<void>(tracker.update(states, true, 0));
        states[0]->buttons = 0x1000;
        auto events = tracker.update(states, true, 10);
        for (const auto& event : events) static_cast<void>(session.lane_binding_state_.apply(event.keycode, event.state));
        report.check("xinput.session_button_reaches_lane", session.lane_binding_state_.pressed(session.key_to_lane_.at(a)));
        states[0].reset();
        events = tracker.update(states, true, 20);
        for (const auto& event : events) static_cast<void>(session.lane_binding_state_.apply(event.keycode, event.state));
        report.check("xinput.session_disconnect_releases_lane", !session.lane_binding_state_.pressed(session.key_to_lane_.at(a)));
    }
    static bool timing_state(const GameSession& session, int version, bool ghost) {
        return session.bms_timing_version_ == version && session.ghost_replay_enabled_ == ghost &&
            static_cast<bool>(session.ghost_engine_) == ghost;
    }
    static uint32_t check_bms_session_policy(const GameSession& session, Report& report, int attempt) {
        const auto name = "bms_branch.initialize_" + std::to_string(attempt);
        report.check(name + ".independent_fixed_lane_seed", session.config_.mode.random_seed == 24680u);
        report.check(name + ".policies", session.bms_control_flow_version_ == 2 &&
            session.bms_gauge_version_ == 2 && session.chart_.bms_has_random &&
            session.chart_.bms_total == std::optional<double>(200.0));
        return session.bms_random_seed_;
    }
    static std::vector<gameplay::ReplayEvent> timing_inputs(const GameSession& session) {
        std::vector<gameplay::ReplayEvent> events;
        for (const auto& note : session.chart_.notes) {
            events.push_back({note.lane, input::InputState::Pressed, note.start_sample});
            events.push_back({note.lane, input::InputState::Released, note.start_sample + session.sample_rate_ / 20});
        }
        std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
        return events;
    }
    static bool corrected_without_legacy_ghost(const GameSession& session) {
        return session.key_conversion_version_ == 3 && !session.ghost_replay_enabled_ &&
               !session.ghost_engine_ && session.chart_.notes.size() == 4;
    }
    static std::vector<gameplay::ReplayEvent> delta_inputs(GameSession& session, Report& report) {
        report.check("delta.session_version", session.key_conversion_version_ == 3);
        report.check("delta.session_note_count", session.chart_.notes.size() == 4 && session.engine_->stats().total_notes == 4);
        const auto holds = std::count_if(session.chart_.notes.begin(), session.chart_.notes.end(),
            [](const auto& n) { return n.end_sample.has_value(); });
        report.check("delta.session_hold_count", holds == 2);
        std::vector<gameplay::ReplayEvent> events;
        for (const auto& note : session.chart_.notes) {
            events.push_back({note.lane, input::InputState::Pressed, note.start_sample});
            events.push_back({note.lane, input::InputState::Released, note.end_sample.value_or(note.start_sample) + 1});
        }
        std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
        return events;
    }
    static void submit_delta_inputs(GameSession& session, const std::vector<gameplay::ReplayEvent>& events,
                                    std::size_t& index, int64_t playback) {
        std::lock_guard<std::mutex> lock(session.engine_mutex_);
        while (index < events.size() && playback >= events[index].sample) {
            const auto& event = events[index++];
            session.dispatch_lane_input(event.lane, event.state, event.sample, playback);
        }
    }
    static void submit_judge_fixture_notes(GameSession& session, std::size_t& index, int64_t playback) {
        std::lock_guard<std::mutex> lock(session.engine_mutex_);
        while (index < session.chart_.notes.size()) {
            const auto& note = session.chart_.notes[index];
            if (playback < note.start_sample + session.sample_rate_ / 100) break;
            // Script a precise sample-domain input through the production lane
            // dispatcher. This checks rules, not physical keyboard latency.
            const int64_t sample = note.start_sample + session.sample_rate_ / 50; // +20 ms
            session.dispatch_lane_input(note.lane, input::InputState::Pressed, sample, sample);
            session.dispatch_lane_input(note.lane, input::InputState::Released, sample + 1, sample);
            ++index;
        }
    }
    static void restart_edge(GameSession& session, bool pressed, bool force_held = false) {
        std::lock_guard<std::mutex> lock(session.engine_mutex_);
        if (force_held) session.instant_restart_key_armed_ = false;
        (void)session.handle_control_input({session.instant_restart_keycode_,
            pressed ? input::InputState::Pressed : input::InputState::Released, timing::HighResClock::now_ns()});
    }
    static int64_t first_note(const GameSession& session) {
        return session.chart_.notes.empty() ? -1 : session.chart_.notes.front().start_sample;
    }
    static bool fresh_start(const GameSession& session) {
        return session.engine_ && session.engine_->stats().raw_score == 0 &&
            session.engine_->replay().events.empty() && session.last_audio_sample_.load() == 0 &&
            session.replay_event_index_ == 0 && session.ghost_replay_event_index_ == 0 &&
            session.autoplay_event_index_ == 0 && session.next_chart_audio_event_ == 0 &&
            session.chart_audio_voices_.empty();
    }
    static bool first_note_hit(GameSession& session) {
        std::lock_guard<std::mutex> lock(session.engine_mutex_);
        return session.engine_ && session.engine_->stats().counts.pg > 0;
    }
    static void stop_physical_input(GameSession& session) {
        // This independent session tests control flow, not a physical device.
        // Stop its sole queue producer before playback to prevent external keys
        // from affecting the deterministic control sequence.
        session.input_thread_.stop();
        while (session.input_thread_.queue().pop().has_value()) {}
        // Playback-start/resume also polls held bindings directly. Isolate that
        // baseline from desktop keys as well; fixture events use the dispatcher.
        session.polled_gameplay_keys_.clear();
    }
    static void escape(GameSession& session) {
        // The normal audio callback applies controls while holding this mutex.
        // Use the same serialization rather than writing pause state directly
        // or introducing a second producer into the input SPSC queue.
        std::lock_guard<std::mutex> lock(session.engine_mutex_);
        const input::InputEvent event{session.escape_keycode_, input::InputState::Pressed,
                                      timing::HighResClock::now_ns()};
        (void)session.handle_control_input(event);
    }
    static void timeout(GameSession& session) {
        session.user_aborted_.store(true, std::memory_order_release);
        session.stop_requested_.store(true, std::memory_order_release);
        session.finished_.store(true, std::memory_order_release);
    }
};
}  // namespace tenriff::app

namespace {
using Access = tenriff::app::MenuAppVisualTestAccess;

void expect_screen(tenriff::app::MenuApp& app, Report& report,
                   const std::string& name, Screen expected) {
    const bool matched = Access::screen(app) == expected;
    report.check(name + ".screen", matched,
        "expected=" + std::to_string(static_cast<int>(expected)) +
        "; actual=" + std::to_string(static_cast<int>(Access::screen(app))));
    std::string present_detail;
    const bool presented = Access::present(app, present_detail);
    report.check(name + ".present", presented, std::move(present_detail));
    if (!matched) throw std::runtime_error(name + " screen transition failed");
}

void menu_flow(tenriff::app::MenuApp& app, Report& report) {
    Access::isolate(app);
    report.check("network.disabled_and_practice_guarded", Access::offline(app));
    expect_screen(app, report, "title", Screen::Title);
    const auto deadline = Clock::now() + std::chrono::seconds(20);
    bool indexed = false;
    while (Clock::now() < deadline && !(indexed = Access::poll_library(app)))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    report.check("library.indexed_exactly_one_synthetic_chart", indexed);
    if (!indexed) throw std::runtime_error("Synthetic library did not index");
    Access::check_home_navigation(app, report);
    Access::check_home_language_cycle(app, report);
    Access::check_import_loading(app, report);

    Access::click(app, Kind::TitleButton, 2);
    expect_screen(app, report, "options", Screen::OptionsHub);
    report.check("options.eight_consolidated_entries", Access::rows(app) == 8);
    struct Page { int index; const char* name; Screen screen; };
    const Page pages[] = {
        {0, "mode", Screen::ModeSelect}, {1, "keymap", Screen::Keymap},
        {2, "skins", Screen::SettingsSkins}, {3, "graphics", Screen::SettingsGraphics},
        {4, "audio", Screen::SettingsAudio}, {5, "input", Screen::SettingsInput},
        {6, "calibration", Screen::SettingsCalibration}, {7, "profile", Screen::QuickSetup}};
    for (const auto& page : pages) {
        Access::click(app, Kind::OptionsItem, page.index);
        expect_screen(app, report, page.name, page.screen);
        if (page.index == 0) {
            Access::select_hard_judge(app, report);
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::ModeSettingId::Mods), Part::SelectOnly);
            Access::key(app, "Enter");
            expect_screen(app, report, "mode.nested_mods", Screen::ModeMods);
            report.check("mode.nested_mods_excludes_judge", Access::rows(app) == 7);
            Access::key(app, "Esc");
            expect_screen(app, report, "mode.nested_mods.back", Screen::ModeSelect);
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::ModeSettingId::Mods));
            expect_screen(app, report, "mode.nested_mods.pointer", Screen::ModeMods);
            Access::key(app, "Esc");
            expect_screen(app, report, "mode.nested_mods.pointer.back", Screen::ModeSelect);
        } else if (page.index == 1) {
            Access::check_sequential_keymap(app, report);
            Access::click(app, Kind::KeymapButton,
                static_cast<int>(tenriff::app::menu::settings::KeymapActionId::NkroTest));
            expect_screen(app, report, "keymap.nested_test", Screen::KeymapTest);
            Access::key(app, "Esc");
            expect_screen(app, report, "keymap.nested_test.back", Screen::Keymap);
        } else if (page.index == 2) {
            Access::check_lane_shortcuts(app, report);
        } else if (page.index == 3) {
            Access::check_free_resolution(app, report);
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::GraphicsSettingId::Bga), Part::Increment);
            report.check("graphics.bga_toggle", !Access::config(app).graphics.bga_enabled);
        } else if (page.index == 4) {
            Access::click(app, Kind::SettingsRow,
                static_cast<int>(tenriff::app::menu::settings::AudioSettingId::Normalize), Part::Increment);
            report.check("audio.normalize_toggle", Access::config(app).audio_ui.normalize_audio);
            const auto outputs = tenriff::audio::WasapiBackend::enumerate_output_devices();
            report.check("audio.output_list", !outputs.empty());
            if (!outputs.empty()) {
                Access::click(app, Kind::SettingsRow,
                    static_cast<int>(tenriff::app::menu::settings::AudioSettingId::OutputDevice), Part::Increment);
                report.check("audio.output_selection", Access::config(app).audio.device_id == outputs.front().id);
                tenriff::audio::WasapiBackend selected;
                auto selected_config = Access::config(app).audio;
                selected_config.exclusive_mode = false;
                const auto initialized = selected.initialize(selected_config);
                report.check("audio.selected_output_initialize", initialized == tenriff::audio::AudioResult::Success);
                selected.shutdown();
                Access::click(app, Kind::SettingsRow,
                    static_cast<int>(tenriff::app::menu::settings::AudioSettingId::OutputDevice), Part::Decrement);
                report.check("audio.output_default_restored", Access::config(app).audio.device_id.empty());
            }
            tenriff::audio::WasapiBackend missing;
            auto invalid = Access::config(app).audio;
            invalid.device_id = "tenriff-nonexistent-output-test";
            report.check("audio.missing_output_no_fallback", missing.initialize(invalid) == tenriff::audio::AudioResult::DeviceNotFound);
        } else if (page.index == 7) {
            Access::check_start_controls(app, report);
        }
        Access::key(app, "Esc");
        expect_screen(app, report, std::string(page.name) + ".back", Screen::OptionsHub);
    }
    const auto saved = tenriff::config::ConfigLoader{}.load_profile("profiles/menu-smoke");
    report.check("settings.profile_readback", saved.success() &&
        !saved.config.graphics.bga_enabled && saved.config.audio_ui.normalize_audio);
    report.check("settings.live_network_guards_preserved", Access::offline(app));
    Access::key(app, "Esc");
    expect_screen(app, report, "options.back", Screen::Title);
    Access::click(app, Kind::TitleButton, 0);
    expect_screen(app, report, "lobby", Screen::SongSelect);
    Access::check_table_loading_present(app, report);
    Access::check_table_input_and_filter(app, report);
    Access::check_level_search_input(app, report);
    Access::check_browser_sort_and_range(app, report);
    Access::check_loading_pacing(app, report);
    Access::check_hud_feedback_followup(app, report);
    Access::check_launch_level_metadata(app, report);

    std::atomic<bool> observing{true};
    bool loading = false, countdown = false, playing = false, gameplay_present = false;
    bool gameplay_resize_attempted = false, gameplay_resized = false;
    bool measure_lines_observed = false;
    int64_t first_play_present = 0, last_sample = 0;
    std::thread observer([&] {
        while (observing.load(std::memory_order_acquire)) {
            const auto value = Access::observe(app);
            loading |= value.loading; countdown |= value.countdown; playing |= value.playing;
            measure_lines_observed |= value.measure_lines;
            last_sample = std::max(last_sample, value.sample);
            if (value.playing) {
                if (!gameplay_resize_attempted) {
                    gameplay_resize_attempted = true;
                    gameplay_resized = Access::resize_owned_window(1439, 809);
                }
                if (!first_play_present) first_play_present = value.presented_ns;
                gameplay_present |= value.presented_ns > first_play_present;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    try {
        // Actual SongStartButton dispatch -> launch_selected_song -> synchronous
        // MenuApp::launch_gameplay -> real GameSession -> actual Result state.
        Access::click(app, Kind::SongStartButton, 0);
    } catch (...) {
        observing.store(false, std::memory_order_release); observer.join(); throw;
    }
    observing.store(false, std::memory_order_release); observer.join();
    report.check("menu_gameplay.loading_observed", loading);
    report.check("menu_gameplay.start_countdown_observed", countdown);
    report.check("menu_gameplay.audio_clock_advanced", playing && last_sample > 0);
    report.check("menu_gameplay.presents_advanced", gameplay_present);
    report.check("menu_gameplay.authored_measure_lines_reach_hud", measure_lines_observed);
    report.check("menu_gameplay.window_resize_retained_on_result", gameplay_resized &&
        Access::config(app).graphics.resolution == "1439x809");
    const auto resized_profile = tenriff::config::ConfigLoader{}.load_profile("profiles/menu-smoke");
    report.check("menu_gameplay.window_resize_profile_readback", resized_profile.success() &&
        resized_profile.config.graphics.resolution == "1439x809");
    expect_screen(app, report, "natural_result", Screen::Result);
    report.check("natural_result.has_stats", Access::result(app));
    report.check("judge.menu_launch_result_retains_hard", Access::last_result_hard_judge(app));
    Access::key(app, "Space");
    report.check("result.analysis_available", Access::analysis_available(app));
    Access::key(app, "F2");
    report.check("result.analysis_keyboard_lanes", Access::analysis_page(app) == 1);
    Access::click(app, Kind::SettingsRow, 5);
    report.check("result.analysis_mouse_types", Access::analysis_page(app) == 2);
    Access::click(app, Kind::SettingsRow, 6);
    report.check("result.analysis_mouse_timeline", Access::analysis_page(app) == 3);
    Access::check_result_screenshot(app, report, "timeline");
    Access::key(app, "Escape");
    report.check("result.analysis_escape_summary", Access::analysis_page(app) == 0 && Access::screen(app) == Screen::Result);
    Access::check_result_screenshot(app, report, "summary");
    Access::check_analysis_boundaries(app, report);
    Access::check_ln_mix_record_boundaries(app, report);
    Access::click(app, Kind::SettingsRow, 4);
    const auto result_rows = Access::rows(app);
    Access::click(app, Kind::SettingsRow, 0);
    const bool mouse_return = Access::screen(app) == Screen::SongSelect;
    report.check("result.continue_mouse_dispatch", mouse_return,
                 "Result generic row count=" + std::to_string(result_rows));
    if (!mouse_return) {
        // Keep the failure above: keyboard recovery must not hide a broken mouse
        // route or make the final process exit successful.
        Access::key(app, "Enter");
        report.check("result.keyboard_recovery", Access::screen(app) == Screen::SongSelect);
    }
    expect_screen(app, report, "result_to_lobby", Screen::SongSelect);
    report.check("lobby.no_stale_gameplay_hud", !Access::observe(app).playing);
    report.check("lobby.network_guards_preserved", Access::offline(app));
}

void separate_pause_flow(const tenriff::app::CommandLineOptions& options, Report& report) {
    using Session = tenriff::app::GameSession;
    using SessionAccess = tenriff::app::MenuAppFeedbackTestAccess;
    auto session = std::make_unique<Session>();
    session->set_practice_no_fail_override(true);
    int phase = 0;
    bool countdown = false, resume_countdown = false, frozen = true, timed_out = false;
    int64_t paused_sample = -1, resumed_sample = -1;
    auto phase_time = Clock::now();
    const auto deadline = Clock::now() + std::chrono::seconds(25);
    session->set_hud_callback([&](const Session::HudSnapshot& hud) {
        countdown |= hud.countdown_active && !hud.paused;
        if (Clock::now() > deadline) {
            timed_out = true; SessionAccess::timeout(*session); return;
        }
        if (phase == 0 && hud.active && !hud.countdown_active && hud.current_sample > hud.sample_rate / 5) {
            SessionAccess::escape(*session); phase = 1; phase_time = Clock::now();
        } else if (phase == 1 && hud.paused && !hud.countdown_active) {
            // Permit the first paused audio buffer to establish the anchor.
            if (Clock::now() - phase_time < std::chrono::milliseconds(80)) return;
            if (paused_sample < 0) paused_sample = hud.current_sample;
            frozen &= hud.current_sample == paused_sample;
            if (Clock::now() - phase_time > std::chrono::milliseconds(300)) {
                session->request_pause_action(static_cast<int>(tenriff::app::GameplayPauseAction::Continue));
                phase = 2;
            }
        } else if (phase == 2) {
            resume_countdown |= hud.paused && hud.countdown_active;
            if (!hud.paused && !hud.countdown_active && hud.current_sample > paused_sample) {
                resumed_sample = hud.current_sample; phase = 3;
            }
        } else if (phase == 3 && hud.current_sample > resumed_sample + hud.sample_rate / 5) {
            SessionAccess::escape(*session); phase = 4;
        } else if (phase == 4 && hud.paused && !hud.countdown_active) {
            session->request_pause_action(static_cast<int>(tenriff::app::GameplayPauseAction::Exit));
            phase = 5;
        }
    });
    const bool initialized = session->initialize(options);
    report.check("separate_session.initialize", initialized);
    if (initialized) {
        SessionAccess::stop_physical_input(*session);
        session->run();
        report.check("separate_session.countdown", countdown);
        report.check("separate_session.pause_freezes_sample", paused_sample >= 0 && frozen);
        report.check("separate_session.resume_countdown", resume_countdown);
        report.check("separate_session.resumed_sample_advances", resumed_sample > paused_sample);
        report.check("separate_session.pause_exit_aborts", phase == 5 && session->was_exit_requested() &&
                                                        session->was_user_aborted());
        report.check("separate_session.bounded_and_audio_ok", !timed_out && session->audio_error().empty(),
                     session->audio_error());
    }
    session->shutdown();
}

void separate_restart_flow(tenriff::app::CommandLineOptions options, Report& report) {
    using Session = tenriff::app::GameSession;
    using SessionAccess = tenriff::app::MenuAppFeedbackTestAccess;
    tenriff::config::ConfigLoader loader;
    auto config = loader.load_profile("profiles/menu-smoke").config;
    config.ui.skip_start_countdown = true;
    config.ui.instant_restart_key = "F12";
    config.mode.autoplay_enabled = true;
    config.audio_ui.normalize_audio = false;
    options.profile = "restart-smoke";
    if (!loader.save_profile("profiles/restart-smoke", config) ||
        !tenriff::config::KeymapManager{}.save_profile("profiles/restart-smoke", tenriff::config::KeymapManager{}.default_keymap()))
        throw std::runtime_error("Cannot seed restart smoke profile");
    int64_t original_first_note = -1;
    for (int attempt = 0; attempt < 2; ++attempt) {
        // Attempt two proves the immediate-retry override independently of the
        // user's ordinary-start preference; each uses a new production session.
        if (attempt == 1) {
            config.ui.skip_start_countdown = false;
            if (!loader.save_profile("profiles/restart-smoke", config)) throw std::runtime_error("Cannot save start option");
        }
        auto session = std::make_unique<Session>();
        session->set_instant_restart_start(attempt == 1);
        if (attempt == 1) session->set_practice_start_seconds(4.0);
        bool countdown = false, held_guard = false, first_hit = false, timed_out = false;
        int phase = 0;
        const auto deadline = Clock::now() + std::chrono::seconds(15);
        session->set_hud_callback([&](const Session::HudSnapshot& hud) {
            countdown |= hud.countdown_active;
            if (Clock::now() > deadline) { timed_out = true; SessionAccess::timeout(*session); return; }
            if (!session->did_start_gameplay() || hud.current_sample <= hud.sample_rate / 5) return;
            if (phase == 0) {
                SessionAccess::restart_edge(*session, true, true);
                SessionAccess::restart_edge(*session, true);
                held_guard = !session->was_restart_requested();
                phase = 1;
            }
            if (phase == 1 && hud.current_sample > original_first_note + hud.sample_rate / 5) {
                first_hit = SessionAccess::first_note_hit(*session);
                SessionAccess::restart_edge(*session, false);
                SessionAccess::restart_edge(*session, true);
                phase = 2;
            }
        });
        const std::string name = "restart.attempt" + std::to_string(attempt + 1);
        const bool initialized = session->initialize(options);
        report.check(name + ".initialize", initialized);
        if (initialized) {
            SessionAccess::stop_physical_input(*session);
            const auto first_note = SessionAccess::first_note(*session);
            if (attempt == 0) original_first_note = first_note;
            report.check(name + ".zero_countdown_and_fresh_state", !session->hud_snapshot().countdown_active &&
                SessionAccess::fresh_start(*session));
            report.check(name + ".canonical_first_note_preserved", first_note == original_first_note &&
                first_note == static_cast<int64_t>(session->hud_snapshot().sample_rate) * 3);
            session->run();
            report.check(name + ".started_without_countdown", session->did_start_gameplay() && !countdown);
            report.check(name + ".held_key_requires_release", held_guard);
            report.check(name + ".first_note_hit", first_hit);
            report.check(name + ".instant_requested", phase == 2 && session->was_instant_restart_requested() &&
                session->was_restart_requested() && session->was_user_aborted() && !session->was_exit_requested());
            report.check(name + ".audio_and_deadline", !timed_out && session->audio_error().empty(), session->audio_error());
        }
        session->shutdown();
        report.check(name + ".abandoned_attempt_not_exported", !session->result().has_value &&
            session->result().result_path.empty() && session->result().replay_path.empty());
    }
    config.ui.skip_start_countdown = true;
    loader.save_profile("profiles/restart-smoke", config);
    auto multiplayer = std::make_unique<Session>();
    multiplayer->set_peer_battle_mode(true);
    multiplayer->set_instant_restart_start(true);
    bool synchronized_countdown = false, restart_blocked = false;
    multiplayer->set_hud_callback([&](const Session::HudSnapshot& hud) {
        synchronized_countdown |= hud.countdown_active && hud.countdown_value == 3;
        SessionAccess::restart_edge(*multiplayer, true);
        restart_blocked = !multiplayer->was_restart_requested();
        SessionAccess::timeout(*multiplayer);
    });
    const bool initialized = multiplayer->initialize(options);
    report.check("restart.multiplayer_initialize", initialized);
    if (initialized) {
        SessionAccess::stop_physical_input(*multiplayer);
        multiplayer->run();
        report.check("restart.multiplayer_keeps_countdown_and_blocks_restart", synchronized_countdown && restart_blocked &&
            !multiplayer->was_instant_restart_requested());
    }
    multiplayer->shutdown();
}
void separate_judge_flow(tenriff::app::CommandLineOptions options, Report& report) {
    using namespace tenriff;
    // This fixture builds an eligible payload locally; it never sends HTTP.
    options.online_recording_enabled = true;
    using Session = app::GameSession;
    using SessionAccess = app::MenuAppFeedbackTestAccess;
    std::filesystem::create_directories("judge-fixture");
    write_silent_wave("judge-fixture/silence.wav");
    {
        std::ofstream chart("judge-fixture/judge.bms", std::ios::binary);
        chart << "#PLAYER 1\n#TITLE Judge Boundary Fixture\n#BPM 600\n#RANK 3\n#4K\n"
                 "#WAV01 silence.wav\n#00011:01\n#00112:01\n#00213:01\n#00314:01\n";
        if (!chart) throw std::runtime_error("Cannot write judge fixture");
    }
    options.chart_path = "judge-fixture/judge.bms";
    config::ConfigLoader loader;
    for (const bool hard : {false, true}) {
        const std::string name = hard ? "judge.hard" : "judge.normal";
        options.profile = hard ? "judge-hard-smoke" : "judge-normal-smoke";
        const std::string profile = "profiles/" + options.profile;
        auto config = loader.load_profile("profiles/menu-smoke").config;
        config.ui.skip_start_countdown = true;
        config.mode.practice_no_fail_enabled = false;
        config.mode.autoplay_enabled = false;
        config.mode.one_miss_fail_enabled = false;
        config.mode.mods = hard ? std::vector<std::string>{"judge_hard"} : std::vector<std::string>{};
        config.mode.key_mode = "4k";
        config.mode.random = "off";
        config.mode.gauge = "normal";
        config.mode.key_conversion_algorithm = "krrcream";
        config.speed.rate = 1.0;
        config.input_offset_ms = 0;
        config.sound_offset_ms = 0;
        if (!loader.save_profile(profile, app::build_persisted_runtime_config(config)) ||
            !config::KeymapManager{}.save_profile(profile, config::KeymapManager{}.default_keymap()))
            throw std::runtime_error("Cannot seed judge profile");
        auto session = std::make_unique<Session>();
        session->set_session_mode_mods(config.mode.mods);
        std::size_t submitted = 0;
        bool timed_out = false;
        const auto deadline = Clock::now() + std::chrono::seconds(15);
        session->set_hud_callback([&](const Session::HudSnapshot& hud) {
            if (Clock::now() > deadline) { timed_out = true; SessionAccess::timeout(*session); return; }
            if (session->did_start_gameplay()) SessionAccess::submit_judge_fixture_notes(*session, submitted, hud.current_sample);
        });
        const bool initialized = session->initialize(options);
        report.check(name + ".initialize", initialized);
        if (initialized) {
            SessionAccess::stop_physical_input(*session);
            session->run();
            report.check(name + ".audio_and_deadline", !timed_out && session->audio_error().empty(), session->audio_error());
        }
        session->shutdown();
        const auto& result = session->result();
        report.check(name + ".twenty_ms_boundary", result.has_value && result.finished && submitted == 4 &&
            result.stats.counts.pg == (hard ? 0 : 4) && result.stats.counts.gr == (hard ? 4 : 0) &&
            result.stats.counts.gd == 0 && result.stats.counts.bd == 0 && result.stats.counts.pr == 0,
            "BMS RANK 3, scripted +20 ms: Normal PG / Judge Hard GR");
        report.check(name + ".result_mods", result.mods == config.mode.mods);
        const auto replay = gameplay::load_replay_json(result.replay_path);
        report.check(name + ".replay_mods", replay.success() && replay.replay->mods == config.mode.mods);
        std::string error;
        const auto local = app::menu_records::parse_result_file(std::filesystem::u8path(result.result_path), &error);
        report.check(name + ".local_record_mods", local && local->mods == config.mode.mods &&
            local->stats.counts.pg == result.stats.counts.pg && local->stats.counts.gr == result.stats.counts.gr, error);
        if (replay.success()) {
            const auto verified = app::verify_replay_file(std::filesystem::u8path(result.replay_path),
                std::filesystem::u8path(options.chart_path), result.replay_sha256);
            report.check(name + ".replay_verifies", verified.verified() && verified.claims_match, verified.detail);
            std::string payload;
            const bool built = app::build_sites_score_json(*replay.replay, result.replay_sha256,
                "Judge Boundary Fixture", result.clear_status, payload, error);
            report.check(name + ".new_bms_policy_upload_metadata", built && error.empty() &&
                replay.replay->mode.bms_gauge_version == 2 &&
                payload.find("\"bms_gauge_version\":2") != std::string::npos &&
                payload.find("\"bms_control_flow_version\":1") != std::string::npos &&
                payload.find("bms_random_seed") == std::string::npos &&
                payload.find("bms_branch_sha256") == std::string::npos, error);
        }
        const auto saved = loader.load_profile(profile);
        report.check(name + ".profile_stays_unmodified", saved.success() && saved.config.mode.mods.empty());
    }
}

void separate_delta_flow(tenriff::app::CommandLineOptions options, Report& report) {
    using namespace tenriff;
    using SessionAccess = app::MenuAppFeedbackTestAccess;
    std::filesystem::create_directories("delta-fixture");
    write_silent_wave("delta-fixture/silence.wav");
    {
        std::ofstream chart("delta-fixture/duplicate.bms", std::ios::binary);
        chart << "#PLAYER 1\n#TITLE Duplicate LN Fixture\n#BPM 600\n#RANK 3\n#LNTYPE 1\n"
                 "#WAV01 silence.wav\n#00011:01\n#00051:0101\n#00013:01\n"
                 "#00112:01\n#00152:0101\n#00115:01\n";
    }
    options.chart_path = "delta-fixture/duplicate.bms";
    options.profile = "delta-smoke";
    config::ConfigLoader loader;
    auto config = loader.load_profile("profiles/menu-smoke").config;
    config.ui.skip_start_countdown = true;
    config.mode.practice_no_fail_enabled = false;
    config.mode.autoplay_enabled = false;
    config.mode.mods.clear();
    config.mode.key_mode = "native";
    config.mode.random = "off";
    config.mode.gauge = "normal";
    config.speed.rate = 1.0;
    config.input_offset_ms = config.sound_offset_ms = 0;
    const std::string profile = "profiles/" + options.profile;
    if (!loader.save_profile(profile, app::build_persisted_runtime_config(config)) ||
        !config::KeymapManager{}.save_profile(profile, config::KeymapManager{}.default_keymap()))
        throw std::runtime_error("Cannot seed duplicate LN profile");
    auto session = std::make_unique<app::GameSession>();
    std::vector<gameplay::ReplayEvent> events;
    std::size_t submitted = 0;
    bool timed_out = false;
    const auto deadline = Clock::now() + std::chrono::seconds(15);
    session->set_hud_callback([&](const app::GameSession::HudSnapshot& hud) {
        if (Clock::now() > deadline) { timed_out = true; SessionAccess::timeout(*session); return; }
        if (session->did_start_gameplay()) SessionAccess::submit_delta_inputs(*session, events, submitted, hud.current_sample);
    });
    const bool initialized = session->initialize(options);
    report.check("delta.initialize", initialized);
    if (initialized) {
        events = SessionAccess::delta_inputs(*session, report);
        SessionAccess::stop_physical_input(*session);
        session->run();
        report.check("delta.audio_and_deadline", !timed_out && session->audio_error().empty(), session->audio_error());
    }
    session->shutdown();
    const auto& result = session->result();
    report.check("delta.single_press_holds_finish", result.has_value && result.finished && submitted == 8 &&
        result.stats.total_notes == 4 && result.stats.counts.bd == 0 && result.stats.counts.pr == 0);
    report.check("delta.result_version", result.key_conversion_version == 3);
    const auto replay = gameplay::load_replay_json(result.replay_path);
    report.check("delta.replay_version", replay.success() && replay.replay->mode.key_conversion_version == 3,
                 replay.error);
    std::string error;
    const auto saved = app::menu_records::parse_result_file(std::filesystem::u8path(result.result_path), &error);
    report.check("delta.local_result_version", saved && saved->key_conversion_version == 3 && saved->stats.total_notes == 4, error);
    if (replay.success()) {
        const auto verified = app::verify_replay_file(std::filesystem::u8path(result.replay_path),
            std::filesystem::u8path(options.chart_path), result.replay_sha256);
        report.check("delta.replay_verifies", verified.verified() && verified.claims_match, verified.detail);
        // Keep files for comparison with the previously released verifier too.
        // The same single-press trace has six objects under the old policy and
        // four under v3; replay metadata must select the original policy.
        const auto loaded = app::ChartLoader{}.load(options.chart_path, replay.replay->sample_rate, 1.0);
        report.check("delta.legacy_source_loaded", loaded.success() && loaded.chart.notes.size() == 6);
        for (int version : {1, 2}) {
            auto legacy = *replay.replay;
            legacy.mode.key_conversion_version = version;
            const auto old = app::verify_replay_against_chart(legacy, loaded.chart, loaded.format, loaded.base_bpm);
            report.check("delta.legacy_policy_" + std::to_string(version), old.stats.total_notes == 6 &&
                old.stats.counts.bd + old.stats.counts.pr > 0 && !old.claims_match);
            legacy.stats = old.stats;
            legacy.final_score = old.final_score;
            const auto path = std::filesystem::absolute("delta-fixture/legacy-" + std::to_string(version) + ".json").u8string();
            const auto exported = gameplay::save_replay_json(path, legacy);
            report.check("delta.legacy_export_" + std::to_string(version), exported.success());
            const auto checked = app::verify_replay_file(std::filesystem::u8path(path),
                std::filesystem::u8path(options.chart_path));
            report.check("delta.legacy_verifies_" + std::to_string(version), checked.verified() && checked.claims_match, checked.detail);
            auto ghost_options = options;
            ghost_options.ghost_replay_path = path;
            auto ghost_session = std::make_unique<app::GameSession>();
            const bool ghost_initialized = ghost_session->initialize(ghost_options);
            report.check("delta.legacy_ghost_excluded_" + std::to_string(version), ghost_initialized &&
                SessionAccess::corrected_without_legacy_ghost(*ghost_session));
            ghost_session->shutdown();
        }
    }
}
void separate_gamepad_flow(tenriff::app::CommandLineOptions options, Report& report) {
    using namespace tenriff;
    using SessionAccess = app::MenuAppFeedbackTestAccess;
    options.profile = "gamepad_fixture";
    options.chart_path = "delta-fixture/duplicate.bms";
    const auto profile = std::filesystem::current_path() / "profiles" / options.profile;
    std::filesystem::copy(std::filesystem::current_path() / "profiles" / "delta-smoke", profile,
                          std::filesystem::copy_options::recursive);
    auto loaded = config::ConfigLoader{}.load_profile(profile.u8string());
    loaded.config.mode.key_mode = "4k";
    loaded.config.ui.hispeed_decrease_key = "PadLB";
    loaded.config.ui.hispeed_increase_key = "PadRB";
    std::string error;
    if (!config::ConfigLoader{}.save_profile(profile.u8string(), loaded.config, &error)) throw std::runtime_error(error);
    config::KeymapManager manager;
    auto keymap = manager.load_profile(profile.u8string());
    manager.assign_binding(keymap.keymap, "4k", "lane1", "PadA", false);
    manager.assign_binding(keymap.keymap, "4k", "lane2", "PadLT", false);
    if (!manager.save_profile(profile.u8string(), keymap.keymap, &error)) throw std::runtime_error(error);
    auto session = std::make_unique<app::GameSession>();
    const bool ready = session->initialize(options);
    report.check("xinput.session_initialize", ready);
    if (ready) {
        SessionAccess::stop_physical_input(*session);
        SessionAccess::check_gamepad_session(*session, report);
    }
    session->shutdown();
}

void separate_timing_flow(tenriff::app::CommandLineOptions options, Report& report) {
    using namespace tenriff;
    using SessionAccess = app::MenuAppFeedbackTestAccess;
    std::filesystem::create_directory("timing-fixture");
    { std::ofstream file("timing-fixture/timing.bms"); file << bpm_stop_mine_fixture(); }
    options.chart_path = "timing-fixture/timing.bms";
    options.profile = "timing-smoke";
    config::ConfigLoader loader;
    auto config = loader.load_profile("profiles/delta-smoke").config;
    config.ui.skip_start_countdown = true;
    config.mode.gauge = "exhard";
    const std::string profile = "profiles/" + options.profile;
    if (!loader.save_profile(profile, app::build_persisted_runtime_config(config)) ||
        !config::KeymapManager{}.save_profile(profile, config::KeymapManager{}.default_keymap()))
        throw std::runtime_error("Cannot seed timing profile");
    auto session = std::make_unique<app::GameSession>();
    std::vector<gameplay::ReplayEvent> events;
    std::size_t submitted = 0;
    bool timed_out = false;
    const auto timing_flow_start = Clock::now();
    const auto deadline = timing_flow_start + std::chrono::seconds(15);
    int64_t max_observed_sample = 0;
    bool observed_gameplay_start = false;
    session->set_hud_callback([&](const app::GameSession::HudSnapshot& hud) {
        max_observed_sample = std::max(max_observed_sample, hud.current_sample);
        observed_gameplay_start = observed_gameplay_start || session->did_start_gameplay();
        if (Clock::now() > deadline) { timed_out = true; SessionAccess::timeout(*session); return; }
        if (session->did_start_gameplay()) SessionAccess::submit_delta_inputs(*session, events, submitted, hud.current_sample);
    });
    const bool initialized = session->initialize(options);
    const auto timing_run_start = Clock::now();
    report.check("timing.corrected_live_initialize", initialized && SessionAccess::timing_state(*session, 2, false));
    if (initialized) {
        events = SessionAccess::timing_inputs(*session);
        SessionAccess::stop_physical_input(*session);
        session->run();
    }
    session->shutdown();
    const auto result = session->result();
    // Keep watchdog/clock failures distinguishable from actual note judgements.
    const std::string timing_detail = "timeout=" + std::to_string(timed_out) +
        " initialize_ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(timing_run_start - timing_flow_start).count()) +
        " run_ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - timing_run_start).count()) +
        " started=" + std::to_string(observed_gameplay_start) + " max_sample=" + std::to_string(max_observed_sample) +
        " submitted=" + std::to_string(submitted);
    report.check("timing.corrected_live_50ms_tap_survives", !timed_out && result.has_value && result.finished &&
        !result.game_over && result.stats.counts.pg == 2 && result.stats.counts.pr == 0, timing_detail);
    report.check("timing.result_exports_v2", result.bms_timing_version == 2);
    const auto replay = gameplay::load_replay_json(result.replay_path);
    report.check("timing.replay_exports_v2", replay.success() && replay.replay->mode.bms_timing_version == 2);
    if (!replay.success()) return;
    std::string error;
    const auto parsed = app::menu_records::parse_result_file(std::filesystem::u8path(result.result_path), &error);
    report.check("timing.saved_result_v2", parsed && parsed->bms_timing_version == 2, error);
    const auto verified = app::verify_replay_file(std::filesystem::u8path(result.replay_path), std::filesystem::u8path(options.chart_path));
    report.check("timing.live_replay_verifies", verified.verified() && verified.claims_match, verified.detail);
    for (int version : {1, 2}) {
        auto fixture = *replay.replay;
        fixture.mode.bms_timing_version = version;
        const auto path = std::filesystem::absolute("timing-fixture/v" + std::to_string(version) + ".json").u8string();
        report.check("timing.ghost_fixture_" + std::to_string(version), gameplay::save_replay_json(path, fixture).success());
        auto ghost_options = options;
        ghost_options.ghost_replay_path = path;
        auto ghost = std::make_unique<app::GameSession>();
        const bool ready = ghost->initialize(ghost_options);
        report.check("timing.ghost_compatibility_" + std::to_string(version), ready && SessionAccess::timing_state(*ghost, 2, version == 2));
        ghost->shutdown();
        auto playback_options = options;
        playback_options.replay_path = path;
        auto playback = std::make_unique<app::GameSession>();
        const bool playable = playback->initialize(playback_options);
        report.check("timing.replay_initializes_original_policy_" + std::to_string(version), playable && SessionAccess::timing_state(*playback, version, false));
        playback->shutdown();
    }
    options.chart_path = "songs/fixture/smoke.bms";
    auto unchanged = std::make_unique<app::GameSession>();
    const bool ready = unchanged->initialize(options);
    report.check("timing.unchanged_chart_keeps_v1", ready && SessionAccess::timing_state(*unchanged, 1, false));
    unchanged->shutdown();

    // Exercise initialize(), not just the seed helper: fixed SR must remain
    // fixed while authored RANDOM gets a fresh independent seed each attempt.
    options.chart_path = "timing-fixture/branch.bms";
    {
        std::ofstream file(options.chart_path);
        file << "#TITLE Branch\n#KEYS 5\n#BPM 120\n#TOTAL 200\n#RANDOM 4\n";
        for (int branch = 1; branch <= 4; ++branch)
            file << "#IF " << branch << "\n#0011" << branch << ":01\n#ENDIF\n";
        file << "#ENDRANDOM\n#00215:01\n";
    }
    config.mode.key_mode = "auto";
    config.mode.random = "sr";
    config.mode.random_seed = 24680u;
    if (!loader.save_profile(profile, app::build_persisted_runtime_config(config)))
        throw std::runtime_error("Cannot seed branch profile");
    std::optional<uint32_t> previous_branch_seed;
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto branch_session = std::make_unique<app::GameSession>();
        const bool initialized = branch_session->initialize(options);
        report.check("bms_branch.initialize_" + std::to_string(attempt), initialized);
        if (initialized) {
            const auto seed = SessionAccess::check_bms_session_policy(*branch_session, report, attempt);
            if (previous_branch_seed) report.check("bms_branch.fresh_seed_" + std::to_string(attempt), seed != *previous_branch_seed);
            previous_branch_seed = seed;
        }
        branch_session->shutdown();
    }
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "Usage: menu_app_smoke NEW_SANDBOX_DIRECTORY\n"
                     "Creates only synthetic songs/profile and smoke-report.json in a new directory.\n";
        return 2;
    }
    const auto original_cwd = std::filesystem::current_path();
    const auto sandbox = std::filesystem::absolute(argv[1]).lexically_normal();
    std::error_code error;
    // create_directory is exclusive: existing empty folders also fail closed.
    if (!std::filesystem::create_directory(sandbox, error)) {
        std::cerr << "Refusing an existing or unavailable sandbox directory.\n"; return 2;
    }
    Report report;
    const auto began = Clock::now();
    try {
        std::filesystem::current_path(sandbox);
        write_chart("songs/fixture");
        tenriff::config::ConfigLoader loader;
        auto config = loader.defaults();
        config.graphics.display_mode = "windowed";
        config.graphics.resolution = "1080p";
        config.graphics.vsync = false;
        config.graphics.refresh_hz = -1;
        config.graphics.performance_overlay = false;
        config.audio.exclusive_mode = false;
        config.audio_ui.master_volume = 0.0;
        config.audio_ui.title_music = "none";
        config.audio_ui.play_to_end = false;
        config.ui.profile_nickname = "Synthetic Smoke";
        config.ui.require_enter_to_exit = false;
        config.ui.online_records_server_url.clear();
        config.ui.tenriff_main_server_url.clear();
        config.ui.private_server_url.clear();
        config.ui.difficulty_table_url.clear();
        config.ui.difficulty_table_path.clear();
        config.ui.last_played_chart_path.clear();
        config.ui.active_song_source.clear();
        config.ui.recent_song_sources.clear();
        config.ui.result_tail_ms = 100.0;
        config.mode.practice_no_fail_enabled = true;
        std::string reason;
        tenriff::config::KeymapManager keymaps;
        if (!loader.save_profile("profiles/menu-smoke", config, &reason) ||
            !keymaps.save_profile("profiles/menu-smoke", keymaps.default_keymap(), &reason))
            throw std::runtime_error("Cannot seed sandbox profile: " + reason);
        tenriff::app::CommandLineOptions options;
        options.profile = "menu-smoke";
        options.songs_path = "songs";
        {
            auto app = std::make_unique<tenriff::app::MenuApp>();
            const bool initialized = app->initialize(options);
            report.check("menu.initialize", initialized);
            try {
                if (initialized) {
                    Access::attach_present_probe(*app);
                    Access::check_play_mode(*app, report);
                    menu_flow(*app, report);
                }
            } catch (const std::exception& exception) {
                report.check("menu.flow_exception", false, exception.what());
            }
            app->shutdown();
            report.check("menu.exit_code", app->exit_code() == 0);
        }
        {
            // A fresh application instance must read the persisted custom size,
            // not merely reuse the first instance's in-memory configuration.
            auto restarted = std::make_unique<tenriff::app::MenuApp>();
            const bool initialized = restarted->initialize(options);
            report.check("menu.restart_custom_size", initialized &&
                Access::config(*restarted).graphics.resolution == "1439x809" &&
                Access::config(*restarted).graphics.display_mode == "windowed");
            if (initialized) {
                report.check("playmode.restart_defaults_guest", !Access::online_mode(*restarted));
                Access::isolate(*restarted);
                Access::attach_present_probe(*restarted);
                std::string detail;
                report.check("menu.restart_custom_size_present", Access::present(*restarted, detail), detail);
            }
            restarted->shutdown();
        }
        options.chart_path = "songs/fixture/smoke.bms";
        separate_pause_flow(options, report);
        separate_restart_flow(options, report);
        separate_judge_flow(options, report);
        separate_delta_flow(options, report);
        separate_gamepad_flow(options, report);
        separate_timing_flow(options, report);
        report.check("sandbox.no_account_or_sites_connection",
            !std::filesystem::exists("config/sites-leaderboard.json") &&
            !std::filesystem::exists("config/sites-leaderboard.dpapi") &&
            !std::filesystem::exists("profiles/menu-smoke/ranked-account.dpapi"),
            "Fresh sandbox; account and online routes are never invoked.");
    } catch (const std::exception& exception) {
        report.check("exception", false, exception.what());
    }
    const JsonObject result{
        {"passed", JsonValue(report.failed == 0)}, {"failed_checks", JsonValue(double(report.failed))},
        {"elapsed_seconds", JsonValue(std::chrono::duration<double>(Clock::now() - began).count())},
        {"scope", JsonValue("Actual MenuApp initialize/index/controller/launch/GameSession/result/return with real window, renderer and muted shared audio; owner-thread internal input dispatch, not MenuApp::run main-loop or physical OS input.")},
        {"pause_scope", JsonValue("Separate real GameSession, serialized internal Escape and public pause actions; its physical input producer is stopped. No claim of full MenuApp pause-overlay integration.")},
        {"present_observer", JsonValue(JsonObject{
            {"method", JsonValue("Smoke-only render callback wraps unchanged MenuApp::render_tick and accumulates its successful Present result before the next render clears it. One fixture window restart installs the callback before transition checks; FPS, VSync and overlay settings are unchanged by the observer.")},
            {"successful_presents", JsonValue(double(present_probe.successful_presents.load(std::memory_order_acquire)))},
            {"callbacks_without_present", JsonValue(double(present_probe.callbacks_without_present.load(std::memory_order_acquire)))},
            {"raw_zero_reads_after_progress", JsonValue(double(present_probe.raw_zero_reads_after_progress.load(std::memory_order_acquire)))}})},
        {"limits", JsonValue("Synthetic library only. Presents mean liveness, not screenshot equality. No physical latency, device input, online, account, multiplayer, native file dialog, long-session or multi-PC coverage. External runner should impose a 90-second process timeout for driver hangs.")},
        {"checks", JsonValue(std::move(report.checks))}};
    std::ofstream output(sandbox / "smoke-report.json", std::ios::binary);
    output << tenriff::config::json_stringify(JsonValue(result), 2) << '\n';
    output.flush();
    const bool written = output.good();
    std::filesystem::current_path(original_cwd, error);
    return written ? (report.failed == 0 ? 0 : 4) : 3;
}
