#include "app/MenuApp.h"

#include <algorithm>
#include <iostream>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "config/KeycodeMap.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

constexpr int64_t kKeymapCaptureTimeoutNs = 5'000'000'000LL;
constexpr int64_t kKeymapStatusTimeoutNs = 2'000'000'000LL;
constexpr int kKeymapFooterReservedLines = 6;

bool is_concrete_keymap_mode(std::string_view token) {
    return token == "4k" || token == "5k" || token == "6k" || token == "7k" ||
           token == "8k" || token == "9k" || token == "10k" || token == "12k" ||
           token == "14k" || token == "16k";
}

}

std::string resolve_keymap_edit_mode_for_menu(std::optional<int> selected_chart_key_count,
                                              std::string_view runtime_key_mode) {
    config::KeymapManager manager;
    if (selected_chart_key_count.has_value()) {
        const int key_count = selected_chart_key_count.value();
        if ((key_count >= 4 && key_count <= 10) || key_count == 12 ||
            key_count == 14 || key_count == 16) {
            return manager.normalize_mode_token(std::to_string(key_count) + "k");
        }
    }

    const std::string normalized_runtime = to_lower_ascii(std::string(runtime_key_mode));
    if (is_concrete_keymap_mode(normalized_runtime)) {
        return manager.normalize_mode_token(normalized_runtime);
    }
    return "10k";
}

void MenuApp::open_keymap_screen(Screen return_screen) {
    submenu_return_screen_ = return_screen;
    working_keymap_ = keymap_;
    std::optional<int> selected_chart_key_count;
    if (return_screen == Screen::SongSelect) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            selected_chart_key_count = entry->key_count;
        }
    }
    keymap_edit_mode_ = resolve_keymap_edit_mode_for_menu(selected_chart_key_count, config_.mode.key_mode);
    refresh_keymap_lane_list();
    keymap_cursor_ = 0;
    keymap_dirty_ = false;
    keymap_capture_active_ = false;
    clear_keymap_status_message();
    clear_keymap_pending_state();
    screen_ = Screen::Keymap;
    refresh_menu_input_polling_scope();
}

void MenuApp::populate_keymap_render_data(render::MenuRenderData& render) {
    config::KeymapManager keymap_manager;
    const auto current_bindings = keymap_manager.bindings_for_mode(working_keymap_, keymap_edit_mode_);
    render.generic.footer_reserved_lines = kKeymapFooterReservedLines;
    if (!keymap_status_message_.empty() &&
        timing::HighResClock::now_ns() < keymap_status_deadline_ns_) {
        render.generic.footer_notes.push_back(keymap_status_message_);
    }
    render.generic.footer_notes.push_back(current_input_backend_status_label());
    if (submenu_return_screen_ == Screen::SongSelect) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            const std::string selected_mode =
                resolve_keymap_edit_mode_for_menu(entry->key_count, config_.mode.key_mode);
            if (selected_mode != keymap_edit_mode_) {
                render.generic.footer_notes.push_back(
                    ui_text("Selected chart uses ", "선택한 차트 키 모드: ") +
                    ui_key_mode_label(selected_mode) +
                    ui_text(" / editing ", " / 현재 편집: ") +
                    ui_key_mode_label(keymap_edit_mode_));
            } else {
                render.generic.footer_notes.push_back(
                    ui_text("Selected chart key mode: ", "선택한 차트 키 모드: ") +
                    ui_key_mode_label(selected_mode));
            }
        }
    }
    append_menu_row(render.generic,
                    ui_text("Key Mode", "키 모드"),
                    ui_key_mode_label(keymap_edit_mode_),
                    keymap_cursor_ == 0,
                    render::MenuHitTargetKind::None,
                    0,
                    false,
                    false);
    for (std::size_t i = 0; i < keymap_lanes_.size(); ++i) {
        const std::string& lane = keymap_lanes_[i];
        const auto it = current_bindings.find(lane);
        std::string key_name =
            (it == current_bindings.end() || it->second.empty()) ? ui_text("Unassigned", "미할당") : it->second;
        if (keymap_capture_active_ && static_cast<int>(i) + 1 == keymap_cursor_) {
            key_name += ui_text(" [waiting]", " [대기 중]");
        }
        append_menu_row(render.generic,
                        lane,
                        key_name,
                        static_cast<int>(i) + 1 == keymap_cursor_,
                        render::MenuHitTargetKind::None,
                        static_cast<int>(i) + 1,
                        false,
                        false);
    }
    if (keymap_capture_active_) {
        const int64_t now_ns = timing::HighResClock::now_ns();
        const int64_t remaining_ns = std::max<int64_t>(0, keymap_capture_deadline_ns_ - now_ns);
        const int remaining_ms = static_cast<int>(remaining_ns / 1'000'000);
        render.generic.footer_notes.push_back(ui_text("Capture timeout: ", "입력 대기 시간: ") + std::to_string(remaining_ms) + "ms");
        render.generic.footer_notes.push_back(ui_text("Press any keyboard key. Delete cancels capture.",
                                                      "아무 키나 누르세요. Delete 키로 입력 대기를 취소합니다."));
        render.generic.footer_notes.push_back(ui_text("Duplicate lane bindings are allowed.",
                                                      "같은 키를 여러 레인에 중복으로 배치할 수 있습니다."));
    }
    append_menu_row(render.generic,
                    ui_text("Reset", "초기화"),
                    "",
                    false,
                    render::MenuHitTargetKind::KeymapButton,
                    kKeymapButtonReset,
                    !keymap_capture_active_,
                    false);
    append_menu_row(render.generic,
                    "NKRO Test",
                    "",
                    false,
                    render::MenuHitTargetKind::KeymapButton,
                    kKeymapButtonNkroTest,
                    !keymap_capture_active_,
                    false);
    append_menu_row(render.generic,
                    ui_text("Back", "뒤로"),
                    "",
                    false,
                    render::MenuHitTargetKind::KeymapButton,
                    kKeymapButtonBack,
                    !keymap_capture_active_,
                    false);
    render.generic.footer_notes.push_back(ui_text("Left/Right on Key Mode selects a 4K-10K, 12K, 14K, or 16K layout.",
                                                  "키 모드에서 좌우 키를 누르면 4K~10K, 12K, 14K 또는 16K 레이아웃을 고릅니다."));
    render.generic.footer_notes.push_back(ui_text("Enter binds the selected lane and saves immediately. Reset also saves immediately.",
                                                  "Enter로 선택 레인에 키를 할당하면 즉시 저장됩니다. 초기화도 바로 저장됩니다."));
}

void MenuApp::populate_keymap_confirm_render_data(render::MenuRenderData& render) {
    populate_keymap_render_data(render);
}

void MenuApp::populate_keymap_test_render_data(render::MenuRenderData& render) {
    render.generic.footer_reserved_lines = 2;
    render.generic.footer_notes.push_back(ui_text("NKRO Test (press multiple keys)", "NKRO 테스트 (여러 키를 동시에 눌러보세요)"));
    render.generic.footer_notes.push_back(current_input_backend_status_label());
    config::KeymapManager keymap_manager;
    const auto current_bindings = keymap_manager.bindings_for_mode(working_keymap_, keymap_edit_mode_);
    for (std::size_t i = 0; i < keymap_lanes_.size(); ++i) {
        const std::string& lane = keymap_lanes_[i];
        const auto it = current_bindings.find(lane);
        const std::string key_name =
            (it == current_bindings.end() || it->second.empty()) ? ui_text("Unassigned", "미할당") : it->second;
        bool down = false;
        if (!key_name.empty()) {
            const auto keycode = config::KeycodeMap::to_keycode(key_name);
            if (keycode.has_value()) {
                down = (pressed_keys_.find(keycode.value()) != pressed_keys_.end());
            }
        }
        append_menu_row(render.generic,
                        lane,
                        key_name + (down ? ui_text(" [DOWN]", " [눌림]") : ""),
                        down,
                        render::MenuHitTargetKind::None,
                        static_cast<int>(i),
                        false,
                        false);
    }
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", true, render::MenuHitTargetKind::SettingsRow, 0, true, false);
}

void MenuApp::handle_keymap_input(uint32_t keycode) {
    const int row_count = static_cast<int>(keymap_lanes_.size()) + 1;
    if (keycode == key_up_) {
        keymap_cursor_ = clamp_int(keymap_cursor_ - 1, 0, row_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        keymap_cursor_ = clamp_int(keymap_cursor_ + 1, 0, row_count - 1);
        publish_snapshot();
        return;
    }
    if (keymap_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config::KeymapManager manager;
        const auto modes = manager.supported_mode_tokens();
        int current_index = 0;
        for (std::size_t i = 0; i < modes.size(); ++i) {
            if (modes[i] == manager.normalize_mode_token(keymap_edit_mode_)) {
                current_index = static_cast<int>(i);
                break;
            }
        }
        current_index += (keycode == key_left_) ? -1 : 1;
        if (current_index < 0) {
            current_index = static_cast<int>(modes.size() - 1);
        } else if (current_index >= static_cast<int>(modes.size())) {
            current_index = 0;
        }
        keymap_edit_mode_ = modes[static_cast<std::size_t>(current_index)];
        refresh_keymap_lane_list();
        refresh_menu_input_polling_scope();
        publish_snapshot();
        return;
    }
    if (keycode == key_enter_) {
        start_keymap_capture();
        publish_snapshot();
        return;
    }
    if (keycode == key_escape_ || keycode == key_backspace_) {
        exit_keymap_screen();
    }
}

void MenuApp::handle_keymap_confirm_input(uint32_t keycode) {
    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::Keymap;
        publish_snapshot();
    }
}

void MenuApp::handle_keymap_test_input(uint32_t keycode) {
    if (keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::Keymap;
        publish_snapshot();
    }
}

void MenuApp::refresh_keymap_lane_list() {
    config::KeymapManager manager;
    keymap_edit_mode_ = manager.normalize_mode_token(keymap_edit_mode_);
    keymap_lanes_ = manager.lane_ids_for_mode(keymap_edit_mode_);
    const int max_cursor = static_cast<int>(keymap_lanes_.size());
    keymap_cursor_ = clamp_int(keymap_cursor_, 0, max_cursor);
}

void MenuApp::update_keymap_capture_timeout() {
    if (screen_ != Screen::Keymap || !keymap_capture_active_) {
        return;
    }
    const int64_t now_ns = timing::HighResClock::now_ns();
    if (now_ns >= keymap_capture_deadline_ns_) {
        keymap_capture_active_ = false;
        refresh_menu_input_polling_scope();
        publish_snapshot();
    }
}

void MenuApp::start_keymap_capture() {
    const int lane_index = keymap_cursor_ - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(keymap_lanes_.size())) {
        return;
    }
    keymap_capture_active_ = true;
    keymap_capture_deadline_ns_ = timing::HighResClock::now_ns() + kKeymapCaptureTimeoutNs;
    refresh_menu_input_polling_scope();
}

void MenuApp::apply_keymap_capture(uint32_t keycode) {
    const int lane_index = keymap_cursor_ - 1;
    if (lane_index < 0 || lane_index >= static_cast<int>(keymap_lanes_.size())) {
        keymap_capture_active_ = false;
        return;
    }

    const std::string key_name = config::KeycodeMap::to_name(keycode);
    if ((key_delete_ != 0 && keycode == key_delete_) || key_name == "Delete") {
        keymap_capture_active_ = false;
        refresh_menu_input_polling_scope();
        publish_snapshot();
        return;
    }
    if (key_name == "Unknown") {
        keymap_capture_active_ = false;
        refresh_menu_input_polling_scope();
        publish_snapshot();
        return;
    }

    const std::string& lane = keymap_lanes_[static_cast<std::size_t>(lane_index)];
    config::KeymapManager manager;
    config::Keymap pending = working_keymap_;
    pending.mode_bindings[keymap_edit_mode_][lane] = key_name;
    if (keymap_edit_mode_ == "10k") {
        pending.bindings = pending.mode_bindings[keymap_edit_mode_];
    }

    std::string error;
    if (!manager.save_profile(profile_dir_, pending, &error)) {
        std::cerr << "[error] " << error << std::endl;
        show_keymap_status_message(ui_text("Failed to save keymap.", "키 설정 저장에 실패했습니다."));
        keymap_capture_active_ = false;
        refresh_menu_input_polling_scope();
        publish_snapshot();
        return;
    }

    working_keymap_ = pending;
    keymap_ = pending;
    keymap_dirty_ = false;
    keymap_capture_active_ = false;
    show_keymap_status_message(ui_text("Saved ", "저장됨: ") + lane + " = " + key_name);
    clear_keymap_pending_state();
    refresh_menu_input_polling_scope();
    publish_snapshot();
}

void MenuApp::apply_keymap_reset() {
    config::KeymapManager manager;
    config::Keymap pending = working_keymap_;
    manager.reset_mode_bindings(pending, keymap_edit_mode_);
    std::string error;
    if (!manager.save_profile(profile_dir_, pending, &error)) {
        std::cerr << "[error] " << error << std::endl;
        show_keymap_status_message(ui_text("Failed to save keymap.", "키 설정 저장에 실패했습니다."));
        return;
    }
    working_keymap_ = pending;
    keymap_ = pending;
    keymap_dirty_ = false;
    show_keymap_status_message(ui_text("Key mode reset and saved.", "키 모드 초기화 후 저장되었습니다."));
    refresh_menu_input_polling_scope();
}

void MenuApp::apply_keymap_save() {
    config::KeymapManager manager;
    std::string error;
    if (!manager.save_profile(profile_dir_, working_keymap_, &error)) {
        std::cerr << "[error] " << error << std::endl;
        show_keymap_status_message(ui_text("Failed to save keymap.", "키 설정 저장에 실패했습니다."));
        return;
    }
    keymap_ = working_keymap_;
    keymap_dirty_ = false;
    show_keymap_status_message(ui_text("Keymap saved.", "키 설정이 저장되었습니다."));
}

void MenuApp::clear_keymap_status_message() {
    keymap_status_message_.clear();
    keymap_status_deadline_ns_ = 0;
}

void MenuApp::show_keymap_status_message(std::string message) {
    keymap_status_message_ = std::move(message);
    keymap_status_deadline_ns_ = timing::HighResClock::now_ns() + kKeymapStatusTimeoutNs;
}

void MenuApp::clear_keymap_pending_state() {
    keymap_pending_lane_.clear();
    keymap_pending_key_.clear();
    keymap_duplicate_lane_.clear();
}

void MenuApp::exit_keymap_screen() {
    keymap_dirty_ = false;
    keymap_capture_active_ = false;
    clear_keymap_status_message();
    clear_keymap_pending_state();
    screen_ = submenu_return_screen_;
    refresh_menu_input_polling_scope();
    publish_snapshot();
}

} // namespace tenriff::app
