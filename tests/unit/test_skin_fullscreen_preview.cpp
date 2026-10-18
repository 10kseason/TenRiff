#include "doctest/doctest.h"

#ifdef _WIN32
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "app/MenuApp.h"
#include "app/GameSession.h"
#include "app/ProfileSetupFlow.h"
#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

struct MenuAppSkinPreviewTestAccess {
    static void check_overhaul() {
        auto menu = fixture();
        for (const char* mode : {"5k", "7+1", "16k"}) {
            menu->skin_settings_controller_.reset(mode);
            menu->config_.skin.scratch_position = "right";
            menu->publish_snapshot();
            const int count = menu->snapshot_.render.generic.skin_preview.lane_count;
            if (std::string(mode) == "5k") {
                CHECK(menu->snapshot_.render.generic.skin_preview.lane_colors[2] == 0xF6F8FFu);
            }
            CHECK(menu->snapshot_.render.generic.skin_preview.lane_center_gap_scale ==
                  doctest::Approx(std::string(mode) == "16k" ? 1.0 : 0.0));
            for (const auto& row : menu->snapshot_.render.generic.rows) {
                CHECK(SkinSettingsRows{false}.index_of(static_cast<SkinSettingsRowId>(row.row_index)) >= 0);
            }
            static_cast<void>(menu->skin_settings_controller_.select(SkinSettingsRowId::LaneWidth, false));
            for (int lane = 1; lane <= count; ++lane) {
                render::MenuClickEvent click{};
                click.kind = render::MenuHitTargetKind::SkinPreviewLane;
                click.index = lane;
                click.part = render::MenuHitPart::Activate;
                menu->handle_menu_click(click);
                const int source = std::string(mode) == "7+1" ? (lane == count ? 0 : lane) : lane - 1;
                CHECK(menu->skin_settings_controller_.edit_lane() == source);
                CHECK(menu->snapshot_.render.generic.skin_preview.selected_lane == lane);
                CHECK(menu->skin_settings_controller_.selected_id() == SkinSettingsRowId::LaneWidth);
                CHECK_FALSE(menu->skin_settings_controller_.dirty());
            }
            render::MenuClickEvent click{};
            click.kind = render::MenuHitTargetKind::SkinPreviewLane; click.index = 1;
            click.part = render::MenuHitPart::Activate;
            menu->help_overlay_visible_ = true;
            menu->handle_menu_click(click);
            CHECK(menu->snapshot_.render.generic.skin_preview.selected_lane == count);
            menu->help_overlay_visible_ = false;
            click.index = count + 1; menu->handle_menu_click(click);
            CHECK(menu->snapshot_.render.generic.skin_preview.selected_lane == count);
            menu->skin_preview_fullscreen_ = true;
            click.index = 1; menu->handle_menu_click(click);
            CHECK(menu->skin_settings_controller_.edit_lane() == (std::string(mode) == "7+1" ? 0 : count - 1));
            menu->skin_preview_fullscreen_ = false;
        }
        for (const char* mode : {"auto", "5k", "8k"}) {
            menu->config_.mode.key_mode = mode;
            menu->publish_snapshot();
            bool scratch_visible = false;
            for (const auto& row : menu->snapshot_.render.generic.rows) {
                scratch_visible |= row.row_index == static_cast<int>(SkinSettingsRowId::ScratchPosition);
            }
            CHECK(scratch_visible == (std::string(mode) == "auto"));
        }
        menu->reset_screen(MenuApp::Screen::QuickSetup);
        menu->publish_snapshot();
        const auto& rows = menu->snapshot_.render.generic.rows;
        CHECK(rows[profile_setup::kUiFontRow].row_index == profile_setup::kUiFontRow);
        CHECK(rows[profile_setup::kUiFontFileRow].activatable);
        CHECK(rows.size() == static_cast<std::size_t>(profile_setup::row_count(profile_setup::entry(menu->first_run_profile_))));
        // Persist to a test-owned directory, never the user's active profile.
        const auto path = std::filesystem::temp_directory_path() / ("tenriff_font_move_" + std::to_string(timing::HighResClock::now_ns()));
        REQUIRE(std::filesystem::create_directory(path));
        menu->profile_dir_ = path.u8string();
        const auto before = menu->config_.skin.ui_font;
        render::MenuClickEvent click{};
        click.kind = render::MenuHitTargetKind::SettingsRow; click.index = profile_setup::kUiFontRow;
        click.part = render::MenuHitPart::Increment;
        menu->handle_menu_click(click);
        CHECK(menu->config_.skin.ui_font != before);
        CHECK(config::ConfigLoader{}.load_profile(path.u8string()).config.skin.ui_font == menu->config_.skin.ui_font);
        menu->settings_cursor_ = profile_setup::kUiFontRow;
        menu->handle_quick_setup_input(menu->key_left_);
        CHECK(menu->config_.skin.ui_font == before);
        click.index = profile_setup::kGameplayCursorRow;
        click.part = render::MenuHitPart::Increment;
        const bool cursor_before = menu->config_.ui.show_cursor_in_gameplay;
        menu->handle_menu_click(click);
        CHECK(menu->config_.ui.show_cursor_in_gameplay != cursor_before);
        CHECK(config::ConfigLoader{}.load_profile(path.u8string()).config.ui.show_cursor_in_gameplay == !cursor_before);
        menu->settings_cursor_ = profile_setup::kGameplayCursorRow;
        menu->handle_quick_setup_input(menu->key_enter_);
        CHECK(menu->config_.ui.show_cursor_in_gameplay == cursor_before);
        std::error_code ec; std::filesystem::remove(path / "config.json", ec); std::filesystem::remove(path, ec);
    }

    static void check_preview_drag() {
        struct TempProfile {
            std::filesystem::path path;
            ~TempProfile() {
                if (path.empty()) return;
                std::error_code ec;
                std::filesystem::remove(path / "config.json", ec);
                std::filesystem::remove(path, ec);
            }
        } profile;
        for (int attempt = 0; attempt < 32 && profile.path.empty(); ++attempt) {
            const auto candidate = std::filesystem::temp_directory_path() /
                ("tenriff_preview_drag_" + std::to_string(timing::HighResClock::now_ns()) + "_" + std::to_string(attempt));
            std::error_code ec;
            if (std::filesystem::create_directory(candidate, ec)) profile.path = candidate;
        }
        REQUIRE(!profile.path.empty());
        auto menu = fixture();
        menu->profile_dir_ = profile.path.u8string();
        config::ConfigLoader loader;
        for (bool fullscreen : {false, true}) {
            if (fullscreen) click_preview(*menu);
            REQUIRE(menu->skin_preview_fullscreen_ == fullscreen);
            for (double requested : {120.0, -240.0, 100000.0}) {
                render::MenuClickEvent drag{};
                drag.kind = render::MenuHitTargetKind::GameplayFieldDrag;
                drag.part = render::MenuHitPart::Activate;
                drag.value = requested;
                menu->handle_menu_click(drag);
                const double expected = std::clamp(requested, config::kGameplayFieldOffsetXMin, config::kGameplayFieldOffsetXMax);
                CHECK(menu->config_.skin.gameplay_field_offset_x == expected);
                CHECK(menu->snapshot_.render.generic.skin_preview.gameplay_field_offset_x == expected);
                if (fullscreen) CHECK(menu->snapshot_.render.gameplay.gameplay_field_offset_x == expected);
                const auto saved = loader.load_profile(menu->profile_dir_);
                REQUIRE(saved.success());
                CHECK(saved.config.skin.gameplay_field_offset_x == expected);
                check_config_and_selection(*menu);
            }
        }
        tap(*menu, menu->key_escape_);
        CHECK(menu->snapshot_.render.generic.skin_preview.gameplay_field_offset_x == config::kGameplayFieldOffsetXMax);
        menu->reset_screen(MenuApp::Screen::Title);
        render::MenuClickEvent stale{};
        stale.kind = render::MenuHitTargetKind::GameplayFieldDrag;
        stale.value = 0;
        menu->handle_menu_click(stale);
        CHECK(menu->config_.skin.gameplay_field_offset_x == config::kGameplayFieldOffsetXMax);
    }
    static void check_loading_render_cap() {
        auto menu = std::make_unique<MenuApp>();
        menu->config_.graphics.refresh_hz = 0;
        for (bool vsync : {false, true}) {
            menu->config_.graphics.vsync = vsync;
            menu->reset_screen(MenuApp::Screen::Gameplay);
            menu->gameplay_hud_.loading = false;
            menu->gameplay_hud_.active = true;
            const auto playing = menu->current_render_config();
            REQUIRE(playing.fps_limit > 60);
            menu->gameplay_hud_.loading = true;
            menu->gameplay_hud_.active = false;
            CHECK(menu->current_render_config().fps_limit == 60);
            CHECK(menu->current_render_config().vsync == vsync);
            menu->gameplay_hud_.loading = false;
            menu->gameplay_hud_.active = true;
            CHECK(menu->current_render_config().fps_limit == playing.fps_limit);
            menu->reset_screen(MenuApp::Screen::SongSelect);
            const auto lobby = menu->current_render_config();
            menu->gameplay_hud_.loading = true;
            CHECK(menu->current_render_config().fps_limit == lobby.fps_limit);
            CHECK(menu->config_.graphics.refresh_hz == 0);
        }
    }

    static std::unique_ptr<MenuApp> fixture(const char* display_mode = "windowed") {
        auto menu = std::make_unique<MenuApp>();
        menu->key_left_ = 37;
        menu->key_up_ = 38;
        menu->key_right_ = 39;
        menu->key_down_ = 40;
        menu->key_enter_ = 13;
        menu->key_escape_ = 27;
        menu->key_backspace_ = 8;
        menu->key_f1_ = 112;
        menu->key_f6_ = 117;
        menu->key_f9_ = 120;
        menu->key_f10_ = 121;
        // Exercise production dispatch/snapshot code without starting native
        // windows, input/audio threads or writing a real player's profile.
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->config_.graphics.display_mode = display_mode;
        menu->config_.skin.source = "native";
        menu->config_.mode.key_mode = "10k";
        menu->config_.skin.note_width_scale = 1.3;
        menu->config_.skin.note_height_scale = 2.4;
        menu->config_.skin.note_width_scales["7k"] = 0.10;
        menu->config_.skin.note_height_scales["7k"] = 2.15;
        menu->reset_screen(MenuApp::Screen::OptionsHub);
        menu->push_screen(MenuApp::Screen::SettingsSkins);
        menu->skin_settings_controller_.reset("7k");
        static_cast<void>(menu->skin_settings_controller_.select(SkinSettingsRowId::NoteHeight, false));
        static_cast<void>(menu->skin_settings_controller_.mark_external_change());
        menu->publish_snapshot();
        return menu;
    }

    static void key(MenuApp& menu, uint32_t keycode, input::InputState state) {
        input::InputEvent event{};
        event.keycode = keycode;
        event.state = state;
        menu.handle_input_event(event);
    }

    static void tap(MenuApp& menu, uint32_t keycode) {
        key(menu, keycode, input::InputState::Pressed);
        key(menu, keycode, input::InputState::Released);
    }

    static void click_preview(MenuApp& menu) {
        render::MenuClickEvent click{};
        click.kind = render::MenuHitTargetKind::SkinPreviewButton;
        click.index = 0;
        menu.handle_menu_click(click);
    }

    static void check_config_and_selection(const MenuApp& menu) {
        CHECK(menu.config_.mode.key_mode == "10k");
        CHECK(menu.config_.skin.note_width_scale == doctest::Approx(1.3));
        CHECK(menu.config_.skin.note_height_scale == doctest::Approx(2.4));
        REQUIRE(menu.config_.skin.note_width_scales.count("7k") == 1);
        REQUIRE(menu.config_.skin.note_height_scales.count("7k") == 1);
        CHECK(menu.config_.skin.note_width_scales.at("7k") == doctest::Approx(0.10));
        CHECK(menu.config_.skin.note_height_scales.at("7k") == doctest::Approx(2.15));
        CHECK(menu.skin_settings_controller_.edit_mode() == "7k");
        CHECK(menu.skin_settings_controller_.selected_id() == SkinSettingsRowId::NoteHeight);
        CHECK(menu.skin_settings_controller_.dirty());
    }

    static void check_entry_and_exit() {
        for (const char* display_mode : {"windowed", "borderless", "fullscreen"}) {
            auto menu = fixture(display_mode);
            const auto depth = menu->menu_navigator_.depth();
            REQUIRE(menu->snapshot_.render.kind == render::MenuScreenKind::GenericList);
            CHECK(menu->snapshot_.render.generic.skin_preview.note_width_scale == doctest::Approx(0.10));
            tap(*menu, menu->key_f6_);
            REQUIRE(menu->skin_preview_fullscreen_);
            CHECK(menu->current_screen() == MenuApp::Screen::SettingsSkins);
            CHECK(menu->menu_navigator_.depth() == depth);
            CHECK(menu->snapshot_.render.kind == render::MenuScreenKind::GameplayHud);
            CHECK(menu->snapshot_.render.generic.skin_preview.fullscreen);
            CHECK(menu->snapshot_.render.gameplay.active);
            CHECK(menu->snapshot_.render.gameplay.lane_count == 7);
            CHECK(menu->snapshot_.render.gameplay.note_width_scale == doctest::Approx(0.10));
            CHECK(menu->snapshot_.render.gameplay.note_height_scale == doctest::Approx(2.15));
            CHECK(menu->current_window_config().display_mode ==
                (std::string(display_mode) == "windowed" ? "borderless" : display_mode));
            CHECK(menu->config_.graphics.display_mode == display_mode);
            check_config_and_selection(*menu);

            tap(*menu, menu->key_escape_);
            CHECK_FALSE(menu->skin_preview_fullscreen_);
            CHECK(menu->current_screen() == MenuApp::Screen::SettingsSkins);
            CHECK(menu->menu_navigator_.depth() == depth);
            CHECK(menu->snapshot_.render.kind == render::MenuScreenKind::GenericList);
            CHECK_FALSE(menu->snapshot_.render.generic.skin_preview.fullscreen);
            CHECK(menu->current_window_config().display_mode == display_mode);
            check_config_and_selection(*menu);

            click_preview(*menu);
            REQUIRE(menu->skin_preview_fullscreen_);
            click_preview(*menu);
            CHECK_FALSE(menu->skin_preview_fullscreen_);
            CHECK(menu->current_screen() == MenuApp::Screen::SettingsSkins);
            CHECK_FALSE(menu->audio_thread_.is_running());
            CHECK_FALSE(menu->input_thread_.is_running());
            CHECK_FALSE(menu->render_thread_.is_running());
        }
    }

    static void check_hidden_input_isolation() {
        auto menu = fixture();
        // Entering preview must cancel an adjustment already being held.
        menu->pressed_keys_.insert(menu->key_right_);
        menu->song_select_repeat_key_ = menu->key_right_;
        menu->song_select_repeat_next_ns_ = 1;
        menu->song_select_repeat_row_ = static_cast<int>(SkinSettingsRowId::NoteHeight);
        click_preview(*menu);
        REQUIRE(menu->skin_preview_fullscreen_);
        CHECK(menu->song_select_repeat_key_ == 0);
        menu->service_song_select_repeat(1'000'000'000LL, true);
        key(*menu, menu->key_right_, input::InputState::Released);

        // The repeat timer dispatches directly to the settings handler, so a
        // held key must not register a timer before the preview input gate.
        for (const auto keycode : {menu->key_left_, menu->key_right_, menu->key_up_, menu->key_down_}) {
            key(*menu, keycode, input::InputState::Pressed);
            CHECK(menu->song_select_repeat_key_ == 0);
            menu->service_song_select_repeat(menu->song_select_repeat_next_ns_ + 1'000'000'000LL, true);
            check_config_and_selection(*menu);
            key(*menu, keycode, input::InputState::Released);
        }
        for (const auto keycode : {menu->key_enter_, menu->key_f1_, menu->key_f10_})
            tap(*menu, keycode);

        render::MenuClickEvent row{};
        row.kind = render::MenuHitTargetKind::SettingsRow;
        row.index = static_cast<int>(SkinSettingsRowId::NoteHeight);
        row.part = render::MenuHitPart::Increment;
        menu->handle_menu_click(row);
        render::MenuClickEvent wheel{};
        wheel.kind = render::MenuHitTargetKind::MouseWheel;
        wheel.wheel_steps = -4;
        menu->handle_menu_click(wheel);

        CHECK(menu->skin_preview_fullscreen_);
        CHECK(menu->song_select_repeat_key_ == 0);
        CHECK_FALSE(menu->help_overlay_visible_);
        CHECK_FALSE(menu->ranked_account_overlay_visible_);
        check_config_and_selection(*menu);
        // Releasing a hidden key must not leave it latched after preview closes.
        CHECK(menu->pressed_keys_.empty());
        tap(*menu, menu->key_backspace_);
        CHECK(menu->skin_preview_fullscreen_);
        tap(*menu, menu->key_escape_);
        CHECK_FALSE(menu->skin_preview_fullscreen_);
        CHECK(menu->current_screen() == MenuApp::Screen::SettingsSkins);
    }

    static void check_modal_entry_guard() {
        const std::array<bool MenuApp::*, 4> modal_flags{
            &MenuApp::help_overlay_visible_, &MenuApp::chat_overlay_visible_,
            &MenuApp::ranked_account_overlay_visible_, &MenuApp::chat_url_warning_visible_};
        for (const auto modal : modal_flags) {
            auto menu = fixture();
            menu.get()->*modal = true;
            tap(*menu, menu->key_f6_);
            CHECK_FALSE(menu->skin_preview_fullscreen_);
            CHECK(menu->current_window_config().display_mode == "windowed");
            CHECK(menu.get()->*modal);
            // A queued thumbnail click cannot bypass the modal's input owner.
            click_preview(*menu);
            CHECK_FALSE(menu->skin_preview_fullscreen_);
            check_config_and_selection(*menu);
        }
    }

    static void check_feedback_keyboard() {
        auto menu = fixture();
        menu->config_.ui.hispeed_increase_key = "F12";
        menu->refresh_function_keys();
        const auto f6 = config::KeycodeMap::to_keycode("F6").value();
        const auto probes = menu->current_menu_probe_keycodes();
        CHECK(std::find(probes.begin(), probes.end(), f6) != probes.end());
        key(*menu, f6, input::InputState::Pressed);
        REQUIRE(menu->skin_preview_fullscreen_);
        key(*menu, f6, input::InputState::Pressed);
        CHECK(menu->skin_preview_fullscreen_);
        key(*menu, f6, input::InputState::Released);
        tap(*menu, f6);
        CHECK_FALSE(menu->skin_preview_fullscreen_);

        menu->reset_screen(MenuApp::Screen::SongSelect);
        menu->song_select_search_active_ = true;
        menu->song_search_query_ = u8"A가😀";
        key(*menu, 8, input::InputState::Pressed);
        CHECK(menu->song_search_query_ == u8"A가");
        const auto due = menu->song_select_repeat_next_ns_;
        REQUIRE(due > 0);
        key(*menu, 8, input::InputState::Pressed);
        CHECK(menu->song_search_query_ == u8"A가");
        menu->service_song_select_repeat(due, true);
        CHECK(menu->song_search_query_ == "A");
        menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
        CHECK(menu->song_search_query_.empty());
        menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
        CHECK(menu->song_select_search_active_);
        CHECK(menu->current_screen() == MenuApp::Screen::SongSelect);
        key(*menu, 8, input::InputState::Released);
        tap(*menu, 27);
        tap(*menu, 8);
        CHECK(menu->current_screen() == MenuApp::Screen::SongSelect);
    }

    static void check_feedback_folders() {
        const auto directory = std::filesystem::temp_directory_path() /
            ("tenriff-level-folders-" + std::to_string(timing::HighResClock::now_ns()));
        REQUIRE(std::filesystem::create_directory(directory));
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path,ec); } } cleanup{directory};
        std::ofstream(directory / "header.json") << R"({"name":"Test","symbol":"T","level_order":["20","2","16+","1","16","10"],"data_url":"data.json"})";
        std::ofstream(directory / "data.json") << R"([])";
        auto menu = fixture();
        menu->reset_screen(MenuApp::Screen::SongSelect);
        menu->song_select_focus_ = MenuApp::SongSelectFocus::SongList;
        menu->config_.ui.difficulty_table_path = (directory / "header.json").u8string();
        for (const std::string level : {"1", "2", "16", "16+"}) {
            SongEntry entry;
            entry.path = level + ".bms";
            entry.title = "Song " + level;
            entry.artist = "Artist";
            entry.difficulty_table_level = level;
            menu->indexed_songs_.push_back(entry);
        }
        menu->rebuild_visible_song_list();
        menu->publish_snapshot();
        REQUIRE(menu->showing_table_level_folders());
        CHECK((menu->table_levels_ == std::vector<std::string>{"1","2","10","16","16+","20"}));
        CHECK(menu->snapshot_.render.song_select.songs.size() == 6);
        CHECK((menu->table_level_counts_ == std::vector<int>{1,1,0,1,1,0}));
        tap(*menu, 13);
        REQUIRE(menu->visible_song_count() == 1);
        CHECK(menu->visible_song_entry(0)->difficulty_table_level == "1");
        tap(*menu, 27);
        REQUIRE(menu->showing_table_level_folders());
        tap(*menu, 40);
        tap(*menu, 13);
        REQUIRE(menu->visible_song_count() == 1);
        CHECK(menu->visible_song_entry(0)->difficulty_table_level == "2");
        tap(*menu, 27);
        render::MenuClickEvent click{};
        click.kind = render::MenuHitTargetKind::SongCard;
        click.index = 4;
        click.double_click = true;
        menu->handle_menu_click(click);
        REQUIRE(menu->visible_song_count() == 1);
        CHECK(menu->visible_song_entry(0)->difficulty_table_level == "16+");
        tap(*menu, 27);
        click.index = 2;
        menu->handle_menu_click(click);
        CHECK(menu->visible_song_count() == 0);
        CHECK(menu->active_table_level_ == "10");
        tap(*menu, 27);
        CHECK(menu->showing_table_level_folders());
        click.kind = render::MenuHitTargetKind::SongScrollbar;
        click.index = 5;
        menu->handle_menu_click(click);
        CHECK(menu->selected_table_level_ == 5);
        CHECK(menu->selected_song_absolute_path().empty());
        click.kind = render::MenuHitTargetKind::SongStartButton;
        menu->handle_menu_click(click);
        CHECK(menu->active_table_level_ == "20");
        CHECK(menu->visible_song_count() == 0);
    }

    static void check_table_url_repeat() {
        for (const auto screen : {MenuApp::Screen::SongSelect, MenuApp::Screen::SongBrowser}) {
            auto menu = fixture();
            menu->reset_screen(screen);
            menu->difficulty_table_url_editing_ = true;
            menu->difficulty_table_url_input_ = u8"https://host/가漢😀";
            key(*menu, menu->key_backspace_, input::InputState::Pressed);
            CHECK(menu->difficulty_table_url_input_ == u8"https://host/가漢");
            const auto first_due = menu->song_select_repeat_next_ns_;
            REQUIRE(first_due > 0);
            key(*menu, menu->key_backspace_, input::InputState::Pressed);
            CHECK(menu->difficulty_table_url_input_ == u8"https://host/가漢");
            menu->service_song_select_repeat(first_due - 1, true);
            CHECK(menu->difficulty_table_url_input_ == u8"https://host/가漢");
            menu->service_song_select_repeat(first_due, true);
            CHECK(menu->difficulty_table_url_input_ == u8"https://host/가");
            menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
            CHECK(menu->difficulty_table_url_input_ == "https://host/");
            key(*menu, menu->key_backspace_, input::InputState::Released);
            CHECK(menu->song_select_repeat_key_ == 0);
            menu->service_song_select_repeat(first_due + 1'000'000'000LL, true);
            CHECK(menu->difficulty_table_url_input_ == "https://host/");

            key(*menu, menu->key_backspace_, input::InputState::Pressed);
            const auto after_press = menu->difficulty_table_url_input_;
            menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, false);
            CHECK(menu->difficulty_table_url_input_ == after_press);
            CHECK(menu->pressed_keys_.empty());
            CHECK(menu->song_select_repeat_key_ == 0);
            key(*menu, menu->key_backspace_, input::InputState::Pressed);
            menu->help_overlay_visible_ = true;
            const auto before_overlay = menu->difficulty_table_url_input_;
            menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
            CHECK(menu->difficulty_table_url_input_ == before_overlay);
            CHECK(menu->song_select_repeat_key_ == 0);
            menu->help_overlay_visible_ = false;
            key(*menu, menu->key_backspace_, input::InputState::Released);
            key(*menu, menu->key_backspace_, input::InputState::Pressed);
            tap(*menu, menu->key_escape_);
            CHECK_FALSE(menu->difficulty_table_url_editing_);
            CHECK(menu->song_select_repeat_key_ == 0);
            const auto cancelled = menu->difficulty_table_url_input_;
            menu->service_song_select_repeat(first_due + 2'000'000'000LL, true);
            CHECK(menu->difficulty_table_url_input_ == cancelled);

            key(*menu, menu->key_backspace_, input::InputState::Released);
            menu->difficulty_table_url_editing_ = true;
            menu->difficulty_table_url_input_ = u8"가";
            key(*menu, menu->key_backspace_, input::InputState::Pressed);
            CHECK(menu->difficulty_table_url_input_.empty());
            menu->service_song_select_repeat(menu->song_select_repeat_next_ns_, true);
            CHECK(menu->song_select_repeat_key_ == 0);
            key(*menu, menu->key_backspace_, input::InputState::Released);
            menu->difficulty_table_url_input_ = std::string(510, 'x');
            menu->handle_text_input(u8"가");
            CHECK(menu->difficulty_table_url_input_ == std::string(510, 'x'));
            menu->difficulty_table_url_input_ = std::string(509, 'x');
            menu->handle_text_input(u8"가");
            CHECK(menu->difficulty_table_url_input_ == std::string(509, 'x') + u8"가");
            tap(*menu, menu->key_backspace_);
            CHECK(menu->difficulty_table_url_input_ == std::string(509, 'x'));
        }
    }

    static void check_lane_number_shortcuts() {
        auto menu = fixture();
        menu->skin_settings_controller_.reset("16k");
        static_cast<void>(menu->skin_settings_controller_.select(SkinSettingsRowId::LaneWidth, false));
        const auto digit = [](char value) { return config::KeycodeMap::to_keycode(std::string(1, value)).value(); };
        const auto left_shift = config::KeycodeMap::to_keycode("LShift").value();
        const auto left_control = config::KeycodeMap::to_keycode("LControl").value();
        const auto left_alt = config::KeycodeMap::to_keycode("LAlt").value();
        for (int lane = 1; lane <= 10; ++lane) {
            tap(*menu, digit(lane == 10 ? '0' : static_cast<char>('0' + lane)));
            CHECK(menu->skin_settings_controller_.edit_lane() == lane - 1);
            CHECK(menu->skin_settings_controller_.selected_id() == SkinSettingsRowId::LaneWidth);
            CHECK_FALSE(menu->skin_settings_controller_.dirty());
        }
        key(*menu, left_shift, input::InputState::Pressed);
        for (int lane = 11; lane <= 16; ++lane) {
            tap(*menu, digit(static_cast<char>('0' + lane - 10)));
            CHECK(menu->skin_settings_controller_.edit_lane() == lane - 1);
        }
        tap(*menu, digit('0'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 15);
        key(*menu, left_shift, input::InputState::Released);
        tap(*menu, config::KeycodeMap::to_keycode("Numpad3").value());
        CHECK(menu->skin_settings_controller_.edit_lane() == 2);
        menu->help_overlay_visible_ = true;
        tap(*menu, digit('1'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 2);
        menu->help_overlay_visible_ = false;
        menu->skin_preview_fullscreen_ = true;
        tap(*menu, digit('1'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 2);
        menu->skin_preview_fullscreen_ = false;
        menu->key_lcontrol_ = left_control;
        for (auto modifier : {left_control, left_alt}) {
            key(*menu, modifier, input::InputState::Pressed);
            tap(*menu, digit('1'));
            CHECK(menu->skin_settings_controller_.edit_lane() == 2);
            key(*menu, modifier, input::InputState::Released);
        }
        menu->difficulty_table_url_editing_ = true;
        menu->handle_skins_settings_input(digit('1'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 2);
        menu->difficulty_table_url_editing_ = false;
        menu->skin_settings_controller_.reset("4k");
        tap(*menu, digit('0'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 0);
        menu->skin_settings_controller_.reset("7+1");
        menu->config_.skin.scratch_position = "right";
        tap(*menu, digit('1'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 1);
        CHECK(menu->snapshot_.render.generic.skin_preview.selected_lane == 1);
        tap(*menu, digit('8'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 0);
        CHECK(menu->snapshot_.render.generic.skin_preview.selected_lane == 8);
        const auto probes = menu->current_menu_probe_keycodes();
        CHECK(std::find(probes.begin(), probes.end(), digit('1')) != probes.end());
        CHECK(std::find(probes.begin(), probes.end(), left_shift) != probes.end());
        menu->reset_screen(MenuApp::Screen::Title);
        menu->handle_skins_settings_input(digit('2'));
        CHECK(menu->skin_settings_controller_.edit_lane() == 0);
    }

    static void check_navigation_reset() {
        auto menu = fixture();
        tap(*menu, menu->key_f6_);
        REQUIRE(menu->skin_preview_fullscreen_);
        menu->reset_screen(MenuApp::Screen::Title);
        CHECK_FALSE(menu->skin_preview_fullscreen_);
        CHECK(menu->current_window_config().display_mode == "windowed");
        menu->push_screen(MenuApp::Screen::SettingsSkins);
        menu->publish_snapshot();
        CHECK(menu->snapshot_.render.kind == render::MenuScreenKind::GenericList);
        CHECK_FALSE(menu->snapshot_.render.generic.skin_preview.fullscreen);
    }
};

}  // namespace tenriff::app

TEST_CASE("skin overhaul lane buttons select visual lanes and fonts persist from Profile") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_overhaul();
}
TEST_CASE("skin overhaul automatic aspect follows configured destination dimensions") {
    using tenriff::render::gameplay_note_auto_preserves_aspect;
    CHECK(gameplay_note_auto_preserves_aspect(60, 60));
    CHECK(gameplay_note_auto_preserves_aspect(10, 10.00001));
    CHECK_FALSE(gameplay_note_auto_preserves_aspect(60, 32));
    CHECK_FALSE(gameplay_note_auto_preserves_aspect(32, 60));
    CHECK_FALSE(gameplay_note_auto_preserves_aspect(10, 10.01));
    CHECK_FALSE(gameplay_note_auto_preserves_aspect(0, 0));
}
TEST_CASE("chart loading caps rendering without changing gameplay or saved graphics settings") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_loading_render_cap();
}

TEST_CASE("feedback regression preview gear drag saves embedded and fullscreen offsets without changing skin selection") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_preview_drag();
}

TEST_CASE("skin fullscreen preview restores display mode and retains skin editing state") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_entry_and_exit();
}

TEST_CASE("skin fullscreen preview isolates hidden settings from keyboard pointer and repeat input") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_hidden_input_isolation();
}

TEST_CASE("skin fullscreen preview respects existing modal input ownership") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_modal_entry_guard();
}

TEST_CASE("skin fullscreen preview cannot survive a navigation reset") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_navigation_reset();
}
TEST_CASE("feedback physical F6 is independent and search Backspace repeats without navigation") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_feedback_keyboard();
}
TEST_CASE("feedback table folders retain empty tiers sort naturally and open exact levels") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_feedback_folders();
}
TEST_CASE("difficulty table URL held Backspace repeats complete UTF8 characters only while focused") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_table_url_repeat();
}
TEST_CASE("skin lane number shortcuts honor visible order bounds and modal ownership") {
    tenriff::app::MenuAppSkinPreviewTestAccess::check_lane_number_shortcuts();
}
#endif

#ifdef _WIN32
namespace tenriff::app {
struct MenuAppFeedbackTestAccess {
    static void check_key_transfer_capture_profile() {
        struct TempProfile {
            std::filesystem::path path;
            ~TempProfile() {
                if (path.empty()) return;
                std::error_code ec;
                std::filesystem::remove(path / "keymap.json", ec);
                std::filesystem::remove(path, ec);
            }
        } profile;
        const auto temp_root = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 32 && profile.path.empty(); ++attempt) {
            const auto candidate = temp_root / ("tenriff_key_transfer_" +
                std::to_string(timing::HighResClock::now_ns()) + "_" + std::to_string(attempt));
            std::error_code ec;
            if (std::filesystem::create_directory(candidate, ec)) profile.path = candidate;
        }
        REQUIRE_FALSE(profile.path.empty());
        config::KeymapManager manager;
        for (int key_count : {4, 10}) {
            const auto mode = std::to_string(key_count) + "k";
            for (bool source_secondary : {false, true}) {
                for (bool target_secondary : {false, true}) {
                    for (int target_lane : {1, 2}) {
                        auto menu = std::make_unique<MenuApp>();
                        menu->config_.audio_ui.background_sound_enabled = false;
                        menu->profile_dir_ = profile.path.u8string();
                        menu->working_keymap_ = manager.default_keymap();
                        const auto key_name = source_secondary ? std::string("LShift")
                            : menu->working_keymap_.mode_bindings.at(mode).at("lane1");
                        if (source_secondary)
                            menu->working_keymap_.secondary_mode_bindings[mode]["lane1"] = key_name;
                        menu->keymap_ = menu->working_keymap_;
                        const auto before = menu->keymap_;
                        REQUIRE(manager.save_profile(menu->profile_dir_, menu->keymap_));
                        menu->reset_screen(MenuApp::Screen::Keymap);
                        menu->keymap_settings_controller_.reset(key_count, mode);

                        render::MenuClickEvent click{};
                        click.kind = render::MenuHitTargetKind::SettingsRow;
                        click.index = target_lane;
                        click.part = target_secondary ? render::MenuHitPart::Increment : render::MenuHitPart::Activate;
                        menu->handle_menu_click(click);
                        REQUIRE(menu->keymap_settings_controller_.capture_active());
                        MenuAppSkinPreviewTestAccess::tap(*menu, config::KeycodeMap::to_keycode(key_name).value());
                        CHECK(menu->keymap_settings_controller_.capture_active());
                        CHECK(menu->keymap_settings_controller_.selected_row() == target_lane + 1);
                        CHECK(menu->keymap_settings_controller_.secondary_selected() == target_secondary);
                        auto saved = manager.load_profile(menu->profile_dir_);
                        REQUIRE(saved.success());
                        CHECK_FALSE(saved.rewritten());
                        for (const auto* keymap : {&saved.keymap, &menu->working_keymap_, &menu->keymap_}) {
                            const auto primary = manager.bindings_for_mode(*keymap, mode);
                            const auto secondary = manager.secondary_bindings_for_mode(*keymap, mode);
                            CHECK((target_secondary ? secondary : primary).at("lane" + std::to_string(target_lane)) == key_name);
                            if (target_lane != 1 || source_secondary != target_secondary) {
                                if (source_secondary) CHECK(secondary.count("lane1") == 0);
                                else CHECK(primary.at("lane1").empty());
                            }
                            CHECK(manager.validate_unique_bindings(*keymap).empty());
                            CHECK(keymap->bindings == keymap->mode_bindings.at("10k"));
                            for (const auto& [other_mode, bindings] : before.mode_bindings) {
                                if (other_mode != mode) CHECK(keymap->mode_bindings.at(other_mode) == bindings);
                            }
                        }
                        CHECK(menu->pressed_keys_.empty());
                    }
                }
            }
        }
    }

    static void check_shift_capture_profile() {
        struct TempProfile {
            std::filesystem::path path;
            ~TempProfile() {
                if (path.empty()) return;
                std::error_code ec;
                std::filesystem::remove(path / "keymap.json", ec);
                std::filesystem::remove(path, ec);
            }
        } profile;
        const auto temp_root = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 32 && profile.path.empty(); ++attempt) {
            const auto candidate = temp_root / ("tenriff_shift_capture_" +
                std::to_string(timing::HighResClock::now_ns()) + "_" + std::to_string(attempt));
            std::error_code ec;
            if (std::filesystem::create_directory(candidate, ec)) profile.path = candidate;
        }
        REQUIRE_FALSE(profile.path.empty());

        config::KeymapManager manager;
        for (bool secondary : {false, true}) {
            for (const char* shift : {"LShift", "RShift"}) {
                auto menu = std::make_unique<MenuApp>();
                menu->config_.audio_ui.background_sound_enabled = false;
                menu->profile_dir_ = profile.path.u8string();
                menu->working_keymap_ = manager.default_keymap();
                menu->working_keymap_.mode_bindings["4k"]["lane3"] = "VK_10";
                menu->keymap_ = menu->working_keymap_;
                REQUIRE(manager.save_profile(menu->profile_dir_, menu->keymap_));
                menu->reset_screen(MenuApp::Screen::Keymap);
                menu->keymap_settings_controller_.reset(4, "4k");

                render::MenuClickEvent click{};
                click.kind = render::MenuHitTargetKind::SettingsRow;
                click.index = 1;
                click.part = secondary ? render::MenuHitPart::Increment : render::MenuHitPart::Activate;
                menu->handle_menu_click(click);
                REQUIRE(menu->keymap_settings_controller_.capture_active());
                const auto deadline = menu->keymap_settings_controller_.capture_deadline_ns();
                const auto original = manager.bindings_for_mode(menu->working_keymap_, "4k").at("lane1");

                // Reproduce the actual empty-scope polling order, not just the
                // separate menu health-probe key list.
                MenuAppSkinPreviewTestAccess::key(*menu, 0x10u, input::InputState::Pressed);
                CHECK(menu->keymap_settings_controller_.capture_active());
                CHECK(menu->keymap_settings_controller_.capture_deadline_ns() == deadline);
                auto saved = manager.load_profile(menu->profile_dir_);
                REQUIRE(saved.success());
                CHECK(manager.bindings_for_mode(saved.keymap, "4k").at("lane1") == original);
                CHECK(manager.secondary_bindings_for_mode(saved.keymap, "4k").empty());

                const auto side_keycode = config::KeycodeMap::to_keycode(shift).value();
                MenuAppSkinPreviewTestAccess::key(*menu, side_keycode, input::InputState::Pressed);
                CHECK(menu->keymap_settings_controller_.capture_active());
                CHECK(menu->keymap_settings_controller_.selected_row() == 2);
                saved = manager.load_profile(menu->profile_dir_);
                REQUIRE(saved.success());
                for (const auto* keymap : {&saved.keymap, &menu->working_keymap_, &menu->keymap_}) {
                    const auto bindings = secondary ? manager.secondary_bindings_for_mode(*keymap, "4k")
                                                    : manager.bindings_for_mode(*keymap, "4k");
                    REQUIRE(bindings.count("lane1") == 1);
                    CHECK(bindings.at("lane1") == shift);
                    CHECK(manager.bindings_for_mode(*keymap, "4k").at("lane3") == "VK_10");
                }
                MenuAppSkinPreviewTestAccess::key(*menu, side_keycode, input::InputState::Released);
                MenuAppSkinPreviewTestAccess::key(*menu, 0x10u, input::InputState::Released);
                CHECK(menu->pressed_keys_.empty());
            }
        }
    }

    static std::unique_ptr<MenuApp> result_fixture(bool ready = true, bool multiplayer = false) {
        auto menu = std::make_unique<MenuApp>();
        menu->config_ = config::ConfigLoader{}.defaults();
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->key_enter_ = 13;
        menu->key_left_ = 37;
        menu->key_escape_ = 27;
        menu->key_backspace_ = 8;
        menu->key_space_ = 32;
        menu->key_f1_ = 112;
        menu->has_result_ = true;
        menu->last_game_was_multiplayer_ = multiplayer;
        menu->result_presentation_start_ns_ = timing::HighResClock::now_ns();
        menu->result_presentation_skipped_ = ready;
        menu->reset_screen(multiplayer ? MenuApp::Screen::Multiplayer : MenuApp::Screen::SongSelect);
        menu->push_screen(MenuApp::Screen::Result);
        menu->publish_snapshot();
        REQUIRE(menu->snapshot_.screen == MenuApp::Screen::Result);
        REQUIRE(menu->snapshot_.render.kind == render::MenuScreenKind::ResultScreen);
        // The result renderer registers SettingsRow/Activate for buttons 0, 1,
        // and 2 directly. They are not generic settings rows.
        REQUIRE(menu->snapshot_.render.generic.rows.empty());
        REQUIRE(menu->result_presentation_ready() == ready);
        return menu;
    }

    static void click_result(MenuApp& menu, int index,
                             render::MenuHitPart part = render::MenuHitPart::Activate) {
        render::MenuClickEvent event{};
        event.kind = render::MenuHitTargetKind::SettingsRow;
        event.index = index;
        event.part = part;
        menu.handle_menu_click(event);
    }

    static void check_result_continue() {
        for (const bool multiplayer : {false, true}) {
            auto menu = result_fixture(true, multiplayer);
            click_result(*menu, 0);
            const auto destination = multiplayer ? MenuApp::Screen::Multiplayer : MenuApp::Screen::SongSelect;
            CHECK(menu->current_screen() == destination);
            CHECK(menu->snapshot_.screen == destination);
            CHECK_FALSE(menu->last_game_was_multiplayer_);
            CHECK_FALSE(menu->multiplayer_waiting_for_result_exit_);
        }
    }

    static void check_result_pending() {
        auto menu = result_fixture(false);
        for (const int index : {0, 1, 2}) {
            // A queued click must not bypass the presentation gate even though
            // the current pending frame exposes no result button hit regions.
            REQUIRE_FALSE(menu->result_presentation_ready());
            click_result(*menu, index);
            CHECK(menu->current_screen() == MenuApp::Screen::Result);
            CHECK_FALSE(menu->result_presentation_skipped_);
        }
    }

    static void check_result_unavailable_replay() {
        auto menu = result_fixture();
        REQUIRE(menu->last_replay_path_.empty());
        REQUIRE_FALSE(menu->snapshot_.render.result.replay_available);
        // The disabled Replay button has no current hit region. A stale queued
        // Replay click must still remain on Result, not become Continue.
        click_result(*menu, 1);
        CHECK(menu->current_screen() == MenuApp::Screen::Result);
        CHECK(menu->snapshot_.screen == MenuApp::Screen::Result);
    }

    static void check_result_retry_without_chart() {
        auto menu = result_fixture();
        REQUIRE(menu->last_chart_path_.empty());
        REQUIRE(menu->visible_song_count() == 0);
        click_result(*menu, 2);
        CHECK(menu->current_screen() == MenuApp::Screen::Result);
        CHECK_FALSE(menu->audio_thread_.is_running());
        CHECK_FALSE(menu->input_thread_.is_running());
        CHECK_FALSE(menu->render_thread_.is_running());
    }

    static void check_result_invalid_pointer_actions() {
        auto menu = result_fixture();
        for (const int index : {-1, 3}) {
            click_result(*menu, index);
            CHECK(menu->current_screen() == MenuApp::Screen::Result);
        }
        for (const auto part : {render::MenuHitPart::SelectOnly,
                               render::MenuHitPart::Increment,
                               render::MenuHitPart::Decrement,
                               render::MenuHitPart::SetValue}) {
            click_result(*menu, 0, part);
            CHECK(menu->current_screen() == MenuApp::Screen::Result);
        }
    }

    static void check_gameplay_chat_latency_routing() {
        auto menu = std::make_unique<MenuApp>();
        auto session = std::make_unique<GameSession>();
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->key_f8_ = config::KeycodeMap::to_keycode("F8").value();
        menu->key_f11_ = config::KeycodeMap::to_keycode("F11").value();
        menu->key_escape_ = config::KeycodeMap::to_keycode("Esc").value();
        session->f8_keycode_ = menu->key_f8_;
        const uint32_t left_shift = config::KeycodeMap::to_keycode("LShift").value();
        const uint32_t right_shift = config::KeycodeMap::to_keycode("RShift").value();
        session->key_to_lane_ = {{left_shift, 1}, {right_shift, 2}};
        session->set_control_input_callback([&](const input::InputEvent& event) {
            return menu->queue_gameplay_chat_input(event);
        });
        const auto edge = [&](uint32_t key, input::InputState state, bool consumed = true) {
            input::InputEvent event{};
            event.keycode = key; event.state = state; event.input_time_ns = 1000000;
            CHECK(session->handle_control_input(event) == consumed);
        };
        using input::InputState;
        edge(menu->key_f8_, InputState::Pressed);
        CHECK(session->config_.visual_offset_ms == 1.0);
        CHECK(menu->gameplay_chat_control_actions_.empty());
        edge(menu->key_f8_, InputState::Released);
        CHECK_FALSE(session->visual_offset_increase_repeat_.held);

        // With the production menu callback installed, both Shift bindings keep
        // reaching lane handling; neither one changes the meaning of F8.
        for (bool release_left_first : {false, true}) {
            edge(left_shift, InputState::Pressed, false);
            edge(right_shift, InputState::Pressed, false);
            CHECK(session->lane_from_keycode(left_shift) == 1);
            CHECK(session->lane_from_keycode(right_shift) == 2);
            edge(release_left_first ? left_shift : right_shift, InputState::Released, false);
            const double before = session->config_.visual_offset_ms;
            edge(menu->key_f8_, InputState::Pressed);
            CHECK(session->config_.visual_offset_ms == before + 1.0);
            CHECK(session->visual_offset_increase_repeat_.held);
            CHECK(menu->gameplay_chat_control_actions_.empty());
            edge(release_left_first ? right_shift : left_shift, InputState::Released, false);
            edge(menu->key_f8_, InputState::Released);
            CHECK_FALSE(session->visual_offset_increase_repeat_.held);
            CHECK_FALSE(menu->chat_overlay_visible_);
        }

        // If chat opens while F8 is already held, its release must still reach
        // the session and stop calibration repeat even before UI actions drain.
        edge(menu->key_f8_, InputState::Pressed);
        const double before_chat = session->config_.visual_offset_ms;
        CHECK(session->visual_offset_increase_repeat_.held);
        edge(menu->key_f11_, InputState::Pressed);
        REQUIRE(menu->gameplay_chat_control_actions_.size() == 1);
        CHECK(menu->gameplay_chat_control_actions_[0].kind == MenuApp::GameplayOverlayActionKind::ChatVisibility);
        edge(menu->key_f8_, InputState::Released);
        CHECK_FALSE(session->visual_offset_increase_repeat_.held);
        edge(menu->key_f11_, InputState::Released);
        menu->drain_gameplay_chat_input();
        CHECK(menu->chat_overlay_visible_);
        CHECK(session->config_.visual_offset_ms == before_chat);

        // Chat captures new gameplay presses, but never strands prior releases.
        edge(left_shift, InputState::Pressed);
        edge(left_shift, InputState::Released, false);
        edge(menu->key_f8_, InputState::Pressed);
        edge(menu->key_f8_, InputState::Released);
        CHECK_FALSE(session->visual_offset_increase_repeat_.held);
        CHECK(session->config_.visual_offset_ms == before_chat);
        CHECK(menu->gameplay_chat_control_actions_.empty());
        CHECK(menu->chat_overlay_visible_);

        edge(menu->key_f11_, InputState::Pressed);
        REQUIRE(menu->gameplay_chat_control_actions_.size() == 1);
        CHECK(menu->gameplay_chat_control_actions_[0].kind == MenuApp::GameplayOverlayActionKind::Input);
        CHECK(menu->gameplay_chat_control_actions_[0].keycode == menu->key_escape_);
        menu->drain_gameplay_chat_input();
        CHECK_FALSE(menu->chat_overlay_visible_);
        edge(menu->key_f11_, InputState::Released); // Owned even after close.
        CHECK(session->config_.visual_offset_ms == before_chat);
        edge(right_shift, InputState::Pressed, false);
        edge(right_shift, InputState::Released, false);

        // F11 remains close-only in another active overlay.
        menu->ranked_account_overlay_visible_ = true;
        menu->gameplay_overlay_capture_active_.store(true);
        edge(menu->key_f11_, InputState::Pressed);
        menu->drain_gameplay_chat_input();
        edge(menu->key_f11_, InputState::Released);
        CHECK_FALSE(menu->ranked_account_overlay_visible_);
        CHECK_FALSE(menu->chat_overlay_visible_);
        CHECK(session->config_.visual_offset_ms == before_chat);

        // Menus use the same standalone F11 shortcut; F8 never opens chat.
        menu->reset_screen(MenuApp::Screen::Title);
        input::InputEvent menu_edge{};
        menu_edge.keycode = menu->key_f8_; menu_edge.state = InputState::Pressed;
        menu->handle_input_event(menu_edge);
        CHECK_FALSE(menu->chat_overlay_visible_);
        menu_edge.state = InputState::Released;
        menu->handle_input_event(menu_edge);
        menu_edge.keycode = menu->key_f11_; menu_edge.state = InputState::Pressed;
        menu->handle_input_event(menu_edge);
        CHECK(menu->chat_overlay_visible_);
        menu_edge.state = InputState::Released;
        menu->handle_input_event(menu_edge);
        menu_edge.state = InputState::Pressed;
        menu->handle_input_event(menu_edge);
        CHECK_FALSE(menu->chat_overlay_visible_);
    }
    static void check_row_bodies() {
        auto menu = std::make_unique<MenuApp>();
        menu->key_enter_ = 13; menu->key_left_ = 37; menu->key_right_ = 39;
        menu->key_escape_ = 27;
        menu->config_ = config::ConfigLoader{}.defaults();
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->config_.skin.source = "native";
        menu->config_.mode.mods = {"dp_flip"};
        menu->skin_settings_controller_.reset("5k");
        bool checked_resolution_value = false;
        bool checked_judge_value = false;
        for (const auto screen : {MenuApp::Screen::ModeSelect, MenuApp::Screen::ModeMods,
             MenuApp::Screen::SettingsAudio, MenuApp::Screen::SettingsGraphics,
             MenuApp::Screen::SettingsSkins, MenuApp::Screen::QuickSetup}) {
            menu->reset_screen(screen);
            menu->publish_snapshot();
            const auto rows = menu->snapshot_.render.generic.rows;
            for (const auto& row : rows) {
                if (!row.adjustable && row.enabled) continue;
                render::MenuClickEvent event;
                event.kind = render::MenuHitTargetKind::SettingsRow;
                event.index = row.row_index;
                // Row bodies only select, even when Resolution and Judge Window
                // expose a separate value control that accepts Activate.
                event.part = render::MenuHitPart::SelectOnly;
                menu->handle_menu_click(event);
                CHECK(menu->current_screen() == screen);
                const auto check_value_unchanged = [&] {
                    const auto& after = menu->snapshot_.render.generic.rows;
                    const auto found = std::find_if(after.begin(), after.end(), [&](const auto& r) { return r.row_index == row.row_index; });
                    REQUIRE(found != after.end());
                    CHECK(found->value == row.value);
                };
                check_value_unchanged();
                CHECK_FALSE(menu->graphics_settings_controller_.editing_resolution());
                event.part = render::MenuHitPart::Activate;
                if (row.value_activatable && screen == MenuApp::Screen::ModeSelect) {
                    REQUIRE(row.row_index == static_cast<int>(menu::settings::ModeSettingId::JudgeWindow));
                    checked_judge_value = true;
                    const auto original_mods = menu->config_.mode.mods;
                    for (const char* judge : {"judge_hard", "judge_ex", ""}) {
                        auto expected_mods = original_mods;
                        if (*judge != '\0') expected_mods.insert(expected_mods.begin(), judge);
                        menu->handle_menu_click(event);
                        CHECK(menu->current_screen() == screen);
                        CHECK(menu->config_.mode.mods == expected_mods);
                    }
                    check_value_unchanged();
                } else if (row.value_activatable) {
                    REQUIRE(screen == MenuApp::Screen::SettingsGraphics);
                    REQUIRE(row.row_index == static_cast<int>(menu::settings::GraphicsSettingId::Resolution));
                    checked_resolution_value = true;
                    const auto resolution = menu->config_.graphics.resolution;
                    const auto display_mode = menu->config_.graphics.display_mode;
                    menu->handle_menu_click(event);
                    CHECK(menu->graphics_settings_controller_.editing_resolution());
                    CHECK(menu->current_screen() == screen);
                    CHECK(menu->config_.graphics.resolution == resolution);
                    CHECK(menu->config_.graphics.display_mode == display_mode);
                    CHECK_FALSE(menu->graphics_settings_controller_.dirty());
                    for (const char digit : std::string("1500")) menu->handle_graphics_settings_input(digit);
                    CHECK(menu->graphics_settings_controller_.resolution_fields()[0] == "1500");
                    CHECK(menu->config_.graphics.resolution == resolution);
                    CHECK_FALSE(menu->graphics_settings_controller_.dirty());
                    menu->handle_graphics_settings_input(menu->key_escape_);
                    CHECK_FALSE(menu->graphics_settings_controller_.editing_resolution());
                    CHECK(menu->config_.graphics.resolution == resolution);
                    check_value_unchanged();
                } else {
                    // Keep the original defensive guard check: forged Activate
                    // on ordinary choices and disabled rows must not adjust.
                    menu->handle_menu_click(event);
                    CHECK(menu->current_screen() == screen);
                    check_value_unchanged();
                }
            }
        }
        CHECK(checked_resolution_value);
        CHECK(checked_judge_value);
        CHECK_FALSE(menu->render_thread_.is_running());
        menu->reset_screen(MenuApp::Screen::SettingsSkins);
        menu->publish_snapshot();
        render::MenuClickEvent event;
        event.kind = render::MenuHitTargetKind::SettingsRow;
        event.index = static_cast<int>(SkinSettingsRowId::ScratchPosition);
        event.part = render::MenuHitPart::Increment;
        menu->handle_menu_click(event);
        CHECK(menu->config_.skin.scratch_position == "left");
        CHECK(menu->snapshot_.render.generic.selected_help.find("7+1") != std::string::npos);
    }
};
}
TEST_CASE("feedback settings pointer row bodies only select and disabled rows cannot adjust") {
    tenriff::app::MenuAppFeedbackTestAccess::check_row_bodies();
}
TEST_CASE("F11 chat preserves Shift gameplay bindings and F8 calibration edge ownership") {
    tenriff::app::MenuAppFeedbackTestAccess::check_gameplay_chat_latency_routing();
}
TEST_CASE("menu Shift capture ignores generic polling event and saves the physical side to the profile") {
    tenriff::app::MenuAppFeedbackTestAccess::check_shift_capture_profile();
}
TEST_CASE("menu key capture moves primary and secondary assignments and persists the emptied slot") {
    tenriff::app::MenuAppFeedbackTestAccess::check_key_transfer_capture_profile();
}
TEST_CASE("result pointer Continue activates without generic settings rows and returns to its lobby") {
    tenriff::app::MenuAppFeedbackTestAccess::check_result_continue();
}
TEST_CASE("result pointer actions cannot bypass the pending presentation") {
    tenriff::app::MenuAppFeedbackTestAccess::check_result_pending();
}
TEST_CASE("result pointer unavailable Replay does not become Continue") {
    tenriff::app::MenuAppFeedbackTestAccess::check_result_unavailable_replay();
}
TEST_CASE("result pointer Retry without a chart does not continue or start devices") {
    tenriff::app::MenuAppFeedbackTestAccess::check_result_retry_without_chart();
}
TEST_CASE("result pointer dispatch ignores unknown rows and selection-only events") {
    tenriff::app::MenuAppFeedbackTestAccess::check_result_invalid_pointer_actions();
}
#endif
