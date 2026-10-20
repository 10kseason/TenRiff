#include "app/MenuApp.h"

#include <algorithm>
#include <iostream>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "app/ProfileSetupFlow.h"
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
    static_cast<void>(keymap_settings_controller_.select(1));
    keymap_common_tab_ = false;
    keymap_mode_tabs_focused_ = false;
    push_screen(Screen::Keymap);
    refresh_menu_input_polling_scope();
}

void MenuApp::populate_keymap_render_data(render::MenuRenderData& render) {
    render.generic.keymap_mode_tabs.push_back({ui_text("Common Keys", "공용 키"), "", keymap_common_tab_});
    for (const auto& mode : config::KeymapManager{}.supported_mode_tokens()) {
        render.generic.keymap_mode_tabs.push_back({key_mode_label(mode), "",
            !keymap_common_tab_ && mode == keymap_settings_controller_.edit_mode()});
    }
    render.generic.keymap_mode_tabs_focused = keymap_mode_tabs_focused_;
    if (keymap_common_tab_) {
        populate_common_key_rows(render);
        append_menu_row(render.generic, ui_text("Back", "뒤로"), "",
            settings_cursor_ == profile_setup::kDoneRow, render::MenuHitTargetKind::SettingsRow,
            profile_setup::kDoneRow, true, false);
        for (auto& row : render.generic.rows) row.selected &= !keymap_mode_tabs_focused_;
        render.generic.notes.push_back(ui_text("These keys apply to every key mode and share the saved Profile bindings.",
            "모든 키 모드에 적용되며 프로필의 저장된 단축키와 같은 설정을 사용합니다."));
        render.generic.notes.push_back(ui_text("Enter captures a key. Esc cancels; Delete clears. Tab selects the mode tabs.",
            "Enter로 키 지정, Esc로 취소, Delete로 해제합니다. Tab으로 모드 탭을 선택합니다."));
        return;
    }
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
        ui_language());
    render.generic.footer_reserved_lines = view.footer_reserved_lines;
    render.generic.keymap_keyboard = true;
    render.generic.keymap_secondary_selected = keymap_settings_controller_.secondary_selected();
    render.generic.keymap_capture_active = keymap_settings_controller_.capture_active();
    const auto colors = config::resolved_skin_lane_colors(config_.skin, keymap_settings_controller_.edit_mode());
    int keymap_row_index = 0;
    for (auto& source : view.rows) {
        render::MenuHitTargetKind target_kind = render::MenuHitTargetKind::SettingsRow;
        int row_index = keymap_row_index++;
        bool activatable = !keymap_settings_controller_.capture_active();
        if (source.action.has_value()) {
            target_kind = render::MenuHitTargetKind::KeymapButton;
            row_index = static_cast<int>(*source.action);
            activatable = !keymap_settings_controller_.capture_active() ||
                *source.action == menu::settings::KeymapActionId::Back;
        }
        append_menu_row(render.generic,
                        std::move(source.label),
                        std::move(source.value),
                        source.selected && !keymap_mode_tabs_focused_,
                        target_kind,
                        row_index,
                        activatable,
                        false);
        render.generic.rows.back().secondary_value = std::move(source.secondary_value);
        const int lane_index = row_index - 1;
        if (target_kind == render::MenuHitTargetKind::SettingsRow && lane_index >= 0 &&
            static_cast<std::size_t>(lane_index) < colors.size())
            render.generic.rows.back().key_color = config::skin_color_rgb(colors[lane_index]);
        if (row_index == 0 && target_kind == render::MenuHitTargetKind::SettingsRow) {
            auto& row = render.generic.rows.back();
            row.adjustable = row.increment_enabled = row.decrement_enabled = activatable;
        }
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
        ui_language());
    render.generic.footer_reserved_lines = view.footer_reserved_lines;
    render.generic.keymap_keyboard = true;
    render.generic.keymap_test = true;
    const auto colors = config::resolved_skin_lane_colors(config_.skin, keymap_settings_controller_.edit_mode());
    std::size_t lane_index = 0;
    for (auto& source : view.rows) {
        const bool is_back = source.action == menu::settings::KeymapActionId::Back;
        append_menu_row(render.generic,
                        std::move(source.label),
                        std::move(source.value),
                        source.selected && !keymap_mode_tabs_focused_,
                        is_back ? render::MenuHitTargetKind::SettingsRow
                                : render::MenuHitTargetKind::None,
                        0,
                        is_back,
                        false);
        render.generic.rows.back().secondary_value = std::move(source.secondary_value);
        if (!is_back && lane_index < colors.size())
            render.generic.rows.back().key_color = config::skin_color_rgb(colors[lane_index++]);
    }
    render.generic.footer_notes = std::move(view.footer_notes);
}

void MenuApp::select_keymap_tab(int index) {
    const auto modes = config::KeymapManager{}.supported_mode_tokens();
    if (index < 0 || index > static_cast<int>(modes.size()) ||
        keymap_settings_controller_.capture_active() || profile_control_key_capture_active_) return;
    keymap_common_tab_ = index == 0;
    keymap_mode_tabs_focused_ = true;
    if (keymap_common_tab_) {
        settings_cursor_ = profile_setup::kInstantRestartKeyRow;
        refresh_menu_input_polling_scope();
        publish_snapshot();
    } else {
        apply_keymap_settings_effects(keymap_settings_controller_.select_mode(modes[index - 1]));
    }
}

void MenuApp::handle_keymap_input(uint32_t keycode) {
    if (profile_control_key_capture_active_) {
        handle_common_key_capture(keycode);
        return;
    }
    if (keycode == key_tab_) {
        keymap_mode_tabs_focused_ = !keymap_mode_tabs_focused_;
        publish_snapshot();
        return;
    }
    if (keymap_mode_tabs_focused_) {
        if (keycode == key_left_ || keycode == key_right_) {
            const auto modes = config::KeymapManager{}.supported_mode_tokens();
            const auto found = std::find(modes.begin(), modes.end(), keymap_settings_controller_.edit_mode());
            const int index = keymap_common_tab_ ? 0 : 1 + static_cast<int>(found - modes.begin());
            const int count = static_cast<int>(modes.size()) + 1;
            select_keymap_tab((index + (keycode == key_left_ ? count - 1 : 1)) % count);
        } else if (keycode == key_down_ || keycode == key_enter_) {
            keymap_mode_tabs_focused_ = false;
            if (!keymap_common_tab_) static_cast<void>(keymap_settings_controller_.select(1));
            publish_snapshot();
        } else if (keycode == key_escape_) exit_keymap_screen();
        return;
    }
    const bool first_row = keymap_common_tab_ ? settings_cursor_ == profile_setup::kInstantRestartKeyRow
                                             : keymap_settings_controller_.selected_row() <= 1;
    if (keycode == key_up_ && first_row) {
        keymap_mode_tabs_focused_ = true;
        publish_snapshot();
        return;
    }
    if (keymap_common_tab_) {
        if (keycode == key_escape_ || (keycode == key_enter_ && settings_cursor_ == profile_setup::kDoneRow)) {
            exit_keymap_screen();
        } else if (keycode == key_up_ || keycode == key_down_) {
            settings_cursor_ = std::clamp(settings_cursor_ + (keycode == key_up_ ? -1 : 1),
                profile_setup::kInstantRestartKeyRow, profile_setup::kDoneRow);
            publish_snapshot();
        } else if (keycode == key_enter_) handle_common_key_capture(keycode);
        return;
    }
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
    } else if (keycode == key_escape_) {
        effects = keymap_settings_controller_.handle(
            menu::MenuAction::back(), timing::HighResClock::now_ns());
    }
    apply_keymap_settings_effects(effects);
}

void MenuApp::handle_keymap_confirm_input(uint32_t keycode) {
    if (keycode == key_enter_ || keycode == key_escape_) {
        if (!pop_screen()) {
            reset_screen(Screen::Keymap);
        }
        publish_snapshot();
    }
}

void MenuApp::handle_keymap_test_input(uint32_t keycode) {
    if (keycode == key_left_ || keycode == key_right_) {
        apply_keymap_settings_effects(keymap_settings_controller_.adjust_test_key_count(
            keycode == key_left_ ? -1 : 1));
        return;
    }
    if (keycode == key_escape_) {
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
    if (keycode == 0x1Bu || (key_escape_ != 0 && keycode == key_escape_)) {
        apply_keymap_settings_effects(keymap_settings_controller_.cancel_capture());
        return;
    }
#ifdef _WIN32
    // Polling reports VK_SHIFT before LShift/RShift. Keep capture open for the
    // following side-specific event instead of saving an ambiguous VK_10.
    if (keycode == 0x10u) {
        return;
    }
#endif
    const auto selected_lane = keymap_settings_controller_.selected_lane();
    if (!selected_lane.has_value()) {
        apply_keymap_settings_effects(keymap_settings_controller_.cancel_capture());
        return;
    }

    const std::string key_name = config::KeycodeMap::to_name(keycode);
    const bool secondary_slot = keymap_settings_controller_.secondary_selected();
    const bool clear_secondary = secondary_slot &&
        ((key_delete_ != 0 && keycode == key_delete_) || key_name == "Delete");
    if (!secondary_slot && ((key_delete_ != 0 && keycode == key_delete_) || key_name == "Delete")) {
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
    if (clear_secondary) {
        pending.secondary_mode_bindings[edit_mode].erase(lane);
    } else if (!manager.assign_binding(pending, edit_mode, lane, key_name, secondary_slot)) {
        apply_keymap_settings_effects(keymap_settings_controller_.cancel_capture());
        return;
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
        ui_text("Saved ", "저장됨: ") + "Key " + lane.substr(4) +
            (secondary_slot ? ui_text(" secondary = ", " 보조 = ") : " = ") +
            (clear_secondary ? ui_text("Unassigned", "미할당") : key_name),
        timing::HighResClock::now_ns(), !clear_secondary));
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
    if (effects.reset_bindings) apply_keymap_reset();
    if (effects.open_nkro_test) push_screen(Screen::KeymapTest);
    if (effects.refresh_input_scope) {
        refresh_menu_input_polling_scope();
    }
    publish_snapshot();
}

} // namespace tenriff::app
