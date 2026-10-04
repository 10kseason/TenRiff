#include "doctest/doctest.h"

#ifdef _WIN32
#include <array>
#include <memory>
#include <string>

#include "app/MenuApp.h"

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
