#include "app/MenuApp.h"

#include <algorithm>
#include <iostream>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "app/menu/settings/KeymapSettingsView.h"
#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

void MenuApp::open_keymap_screen(Screen return_screen) {
    if (current_screen() != return_screen) {
        return;
    }
    working_keymap_ = keymap_;
    std::optional<int> selected_chart_key_count;
    if (return_screen == Screen::SongSelect) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            selected_chart_key_count = entry->key_count;
        }
    }
    keymap_settings_controller_.reset(selected_chart_key_count, config_.mode.key_mode);
    push_screen(Screen::Keymap);
    refresh_menu_input_polling_scope();
}

void MenuApp::populate_keymap_render_data(render::MenuRenderData& render) {
    std::optional<int> selected_chart_key_count;
    if (menu_navigator_.parent().value_or(Screen::Title) == Screen::SongSelect) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            selected_chart_key_count = entry->key_count;
        }
    }
    auto view = menu::settings::KeymapSettingsView::build(
        keymap_settings_controller_,
        working_keymap_,
        selected_chart_key_count,
        config_.mode.key_mode,
        current_input_backend_status_label(),
        timing::HighResClock::now_ns(),
        ui_uses_korean());
    render.generic.footer_reserved_lines = view.footer_reserved_lines;
    for (auto& source : view.rows) {
        render::MenuHitTargetKind target_kind = render::MenuHitTargetKind::None;
        int row_index = 0;
        bool activatable = false;
        if (source.action.has_value()) {
            target_kind = render::MenuHitTargetKind::KeymapButton;
            row_index = static_cast<int>(*source.action);
            activatable = !keymap_settings_controller_.capture_active();
        }
        append_menu_row(render.generic,
                        std::move(source.label),
                        std::move(source.value),
                        source.selected,
                        target_kind,
                        row_index,
                        activatable,
                        false);
    }
    render.generic.footer_notes = std::move(view.footer_notes);
}

void MenuApp::populate_keymap_confirm_render_data(render::MenuRenderData& render) {
    populate_keymap_render_data(render);
}

void MenuApp::populate_keymap_test_render_data(render::MenuRenderData& render) {
    auto view = menu::settings::KeymapSettingsView::build_nkro_test(
        keymap_settings_controller_,
        working_keymap_,
        pressed_keys_,
        current_input_backend_status_label(),
        ui_uses_korean());
    render.generic.footer_reserved_lines = view.footer_reserved_lines;
    for (auto& source : view.rows) {
        const bool is_back = source.action == menu::settings::KeymapActionId::Back;
        append_menu_row(render.generic,
                        std::move(source.label),
                        std::move(source.value),
                        source.selected,
                        is_back ? render::MenuHitTargetKind::SettingsRow
                                : render::MenuHitTargetKind::None,
                        0,
                        is_back,
                        false);
    }
    render.generic.footer_notes = std::move(view.footer_notes);
}

void MenuApp::handle_keymap_input(uint32_t keycode) {
    menu::settings::KeymapSettingsEffects effects;
    if (keycode == key_up_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::move(-1), timing::HighResClock::now_ns());
    } else if (keycode == key_down_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::move(1), timing::HighResClock::now_ns());
    } else if (keycode == key_left_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::adjust(-1), timing::HighResClock::now_ns());
    } else if (keycode == key_right_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::adjust(1), timing::HighResClock::now_ns());
    } else if (keycode == key_enter_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::activate(), timing::HighResClock::now_ns());
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::back(), timing::HighResClock::now_ns());
    }
    apply_keymap_settings_effects(effects);
}

void MenuApp::handle_keymap_confirm_input(uint32_t keycode) {
    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        if (!pop_screen()) {
            reset_screen(Screen::Keymap);
        }
        publish_snapshot();
    }
}

void MenuApp::handle_keymap_test_input(uint32_t keycode) {
    if (keycode == key_escape_ || keycode == key_backspace_) {
        if (!pop_screen()) {
            reset_screen(Screen::Keymap);
        }
        publish_snapshot();
    }
}

void MenuApp::update_keymap_capture_timeout() {
    if (current_screen() != Screen::Keymap ||
        !keymap_settings_controller_.capture_active()) {
        return;
    }
    apply_keymap_settings_effects(
        keymap_settings_controller_.update_capture_timeout(
            timing::HighResClock::now_ns()));
}

void MenuApp::apply_keymap_capture(uint32_t keycode) {
    const auto selected_lane = keymap_settings_controller_.selected_lane();
    if (!selected_lane.has_value()) {
        apply_keymap_settings_effects(keymap_settings_controller_.cancel_capture());
        return;
    }

    const std::string key_name = config::KeycodeMap::to_name(keycode);
    if ((key_delete_ != 0 && keycode == key_delete_) || key_name == "Delete") {
        apply_keymap_settings_effects(keymap_settings_controller_.cancel_capture());
        return;
    }
    if (key_name == "Unknown") {
        apply_keymap_settings_effects(keymap_settings_controller_.cancel_capture());
        return;
    }

    const std::string lane(*selected_lane);
    const std::string edit_mode(keymap_settings_controller_.edit_mode());
    config::KeymapManager manager;
    config::Keymap pending = working_keymap_;
    pending.mode_bindings[edit_mode][lane] = key_name;
    if (edit_mode == "10k") {
        pending.bindings = pending.mode_bindings[edit_mode];
    }

    std::string error;
    if (!manager.save_profile(profile_dir_, pending, &error)) {
        std::cerr << "[error] " << error << std::endl;
        apply_keymap_settings_effects(keymap_settings_controller_.finish_capture(
            ui_text("Failed to save keymap.", "키 설정 저장에 실패했습니다."),
            timing::HighResClock::now_ns()));
        return;
    }

    working_keymap_ = pending;
    keymap_ = pending;
    apply_keymap_settings_effects(keymap_settings_controller_.finish_capture(
        ui_text("Saved ", "저장됨: ") + lane + " = " + key_name,
        timing::HighResClock::now_ns()));
}

void MenuApp::apply_keymap_reset() {
    config::KeymapManager manager;
    config::Keymap pending = working_keymap_;
    manager.reset_mode_bindings(
        pending, std::string(keymap_settings_controller_.edit_mode()));
    std::string error;
    if (!manager.save_profile(profile_dir_, pending, &error)) {
        std::cerr << "[error] " << error << std::endl;
        keymap_settings_controller_.show_status(
            ui_text("Failed to save keymap.", "키 설정 저장에 실패했습니다."),
            timing::HighResClock::now_ns());
        return;
    }
    working_keymap_ = pending;
    keymap_ = pending;
    keymap_settings_controller_.show_status(
        ui_text("Key mode reset and saved.", "키 모드 초기화 후 저장되었습니다."),
        timing::HighResClock::now_ns());
    refresh_menu_input_polling_scope();
}

void MenuApp::exit_keymap_screen() {
    apply_keymap_settings_effects(keymap_settings_controller_.handle(
        menu::MenuAction::back(), timing::HighResClock::now_ns()));
}

void MenuApp::apply_keymap_settings_effects(
    const menu::settings::KeymapSettingsEffects& effects) {
    if (effects.empty()) {
        return;
    }
    if (effects.menu.navigate_back && !pop_screen()) {
        reset_screen(Screen::OptionsHub);
    }
    if (effects.refresh_input_scope) {
        refresh_menu_input_polling_scope();
    }
    publish_snapshot();
}

} // namespace tenriff::app
