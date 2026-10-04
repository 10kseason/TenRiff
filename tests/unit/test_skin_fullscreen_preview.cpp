#include "doctest/doctest.h"

#ifdef _WIN32
#include <array>
#include <memory>
#include <string>

#include "app/MenuApp.h"
#include "app/GameSession.h"
#include "config/KeycodeMap.h"

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
    static void check_gameplay_chat_latency_routing() {
        auto menu = std::make_unique<MenuApp>();
        auto session = std::make_unique<GameSession>();
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->key_f8_ = config::KeycodeMap::to_keycode("F8").value();
        menu->key_escape_ = config::KeycodeMap::to_keycode("Esc").value();
        session->f8_keycode_ = menu->key_f8_;
        session->lshift_keycode_ = config::KeycodeMap::to_keycode("LShift").value();
        session->rshift_keycode_ = config::KeycodeMap::to_keycode("RShift").value();
        session->set_control_input_callback([&](const input::InputEvent& event) {
            return menu->queue_gameplay_chat_input(event);
        });
        const auto edge = [&](uint32_t key, input::InputState state) {
            input::InputEvent event{};
            event.keycode = key; event.state = state; event.input_time_ns = 1000000;
            CHECK(session->handle_control_input(event));
        };
        using input::InputState;
        edge(menu->key_f8_, InputState::Pressed);
        CHECK(session->config_.visual_offset_ms == 1.0);
        CHECK(menu->gameplay_chat_control_actions_.empty());
        edge(menu->key_f8_, InputState::Released);
        CHECK_FALSE(session->visual_offset_increase_repeat_.held);
        // Either Shift stays sufficient when the other is released first.
        for (bool release_left_first : {false, true}) {
            edge(session->lshift_keycode_, InputState::Pressed);
            edge(session->rshift_keycode_, InputState::Pressed);
            edge(release_left_first ? session->lshift_keycode_ : session->rshift_keycode_, InputState::Released);
            edge(menu->key_f8_, InputState::Pressed);
            CHECK(session->config_.visual_offset_ms == 1.0);
            REQUIRE(menu->gameplay_chat_control_actions_.size() == 1);
            CHECK(menu->gameplay_chat_control_actions_[0].kind == MenuApp::GameplayOverlayActionKind::ChatVisibility);
            edge(release_left_first ? session->rshift_keycode_ : session->lshift_keycode_, InputState::Released);
            edge(menu->key_f8_, InputState::Released);
            CHECK_FALSE(session->visual_offset_increase_repeat_.held);
            menu->drain_gameplay_chat_input();
            CHECK(menu->chat_overlay_visible_);
            // Bare F8 closes the visible chat instead of calibrating the song.
            edge(menu->key_f8_, InputState::Pressed);
            edge(menu->key_f8_, InputState::Released);
            REQUIRE(menu->gameplay_chat_control_actions_.size() == 1);
            CHECK(menu->gameplay_chat_control_actions_[0].kind == MenuApp::GameplayOverlayActionKind::Input);
            CHECK(menu->gameplay_chat_control_actions_[0].keycode == menu->key_escape_);
            menu->drain_gameplay_chat_input();
            CHECK_FALSE(menu->chat_overlay_visible_);
            CHECK(session->config_.visual_offset_ms == 1.0);
        }
        edge(menu->key_f8_, InputState::Pressed);
        edge(menu->key_f8_, InputState::Released);
        CHECK(session->config_.visual_offset_ms == 2.0);
        CHECK(menu->gameplay_chat_control_actions_.empty());
        menu->ranked_account_overlay_visible_ = true;
        menu->gameplay_overlay_capture_active_.store(true);
        edge(menu->key_f8_, InputState::Pressed);
        menu->drain_gameplay_chat_input();
        edge(menu->key_f8_, InputState::Released);
        CHECK_FALSE(menu->ranked_account_overlay_visible_);
        CHECK_FALSE(menu->chat_overlay_visible_);
        CHECK(session->config_.visual_offset_ms == 2.0);
        // The menu uses its existing unmodified F8 shortcut.
        menu->reset_screen(MenuApp::Screen::Title);
        input::InputEvent menu_f8{};
        menu_f8.keycode = menu->key_f8_; menu_f8.state = InputState::Pressed;
        menu->handle_input_event(menu_f8);
        CHECK(menu->chat_overlay_visible_);
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
TEST_CASE("gameplay chat callback keeps F8 latency and Shift F8 chat ownership separate") {
    tenriff::app::MenuAppFeedbackTestAccess::check_gameplay_chat_latency_routing();
}
#endif
