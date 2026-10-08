#include "doctest/doctest.h"

#ifdef _WIN32
#include <array>
#include <memory>
#include <string>

#include "app/MenuApp.h"
#include "app/GameSession.h"
#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

struct MenuAppSkinPreviewTestAccess {
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
        menu->config_.skin.note_width_scales["7k"] = 1.25;
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
        CHECK(menu.config_.skin.note_width_scales.at("7k") == doctest::Approx(1.25));
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
            tap(*menu, menu->key_f6_);
            REQUIRE(menu->skin_preview_fullscreen_);
            CHECK(menu->current_screen() == MenuApp::Screen::SettingsSkins);
            CHECK(menu->menu_navigator_.depth() == depth);
            CHECK(menu->snapshot_.render.kind == render::MenuScreenKind::GameplayHud);
            CHECK(menu->snapshot_.render.generic.skin_preview.fullscreen);
            CHECK(menu->snapshot_.render.gameplay.active);
            CHECK(menu->snapshot_.render.gameplay.lane_count == 7);
            CHECK(menu->snapshot_.render.gameplay.note_width_scale == doctest::Approx(1.25));
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
#endif

#ifdef _WIN32
namespace tenriff::app {
struct MenuAppFeedbackTestAccess {
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
        menu->config_ = config::ConfigLoader{}.defaults();
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->config_.skin.source = "native";
        menu->skin_settings_controller_.reset("5k");
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
                event.part = render::MenuHitPart::Activate;
                menu->handle_menu_click(event);
                CHECK(menu->current_screen() == screen);
                const auto& after = menu->snapshot_.render.generic.rows;
                auto found = std::find_if(after.begin(), after.end(), [&](const auto& r) { return r.row_index == row.row_index; });
                REQUIRE(found != after.end());
                CHECK(found->value == row.value);
            }
        }
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
