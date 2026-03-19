#include "app/MenuApp.h"

#include <cmath>

#include "app/GraphicsTiming.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"

namespace tenriff::app {

namespace {

const int kPollingOptions[] = {1000, 2000, 4000, 8000};
const int kDebounceMsOptions[] = {0, 2, 4, 6, 8, 10, 12};

int next_option_index(const int* options, int count, int current, int direction) {
    int index = 0;
    int best_distance = count > 0 ? std::abs(options[0] - current) : 0;
    for (int i = 0; i < count; ++i) {
        if (options[i] == current) {
            index = i;
            best_distance = 0;
            break;
        }
        const int distance = std::abs(options[i] - current);
        if (distance < best_distance) {
            best_distance = distance;
            index = i;
        }
    }
    index += direction;
    if (index < 0) {
        index = count - 1;
    } else if (index >= count) {
        index = 0;
    }
    return index;
}

}  // namespace

void MenuApp::handle_graphics_settings_input(uint32_t keycode) {
    const int item_count = 7;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.graphics.display_mode = cycle_display_mode(config_.graphics.display_mode, direction);
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.graphics.resolution = cycle_resolution_preset(config_.graphics.resolution, direction);
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        int next_value = config_.graphics.refresh_hz + direction * kRefreshHzStep;
        next_value = clamp_int(next_value, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
        config_.graphics.refresh_hz = next_value;
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 3 && (keycode == key_left_ || keycode == key_right_)) {
        config_.graphics.vsync = !config_.graphics.vsync;
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 4 && (keycode == key_left_ || keycode == key_right_)) {
        config_.graphics.performance_overlay = !config_.graphics.performance_overlay;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 5 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.visual_offset_ms = clamp_step_value(
            config_.visual_offset_ms + static_cast<double>(direction) * kVisualOffsetStep,
            kVisualOffsetMin, kVisualOffsetMax, kVisualOffsetStep);
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (graphics_dirty_) {
            persist_runtime_config();
            apply_runtime_graphics_config();
            graphics_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::handle_input_settings_input(uint32_t keycode) {
    const int item_count = 3;
    if (keycode == key_up_) {
        settings_cursor_ = clamp_int(settings_cursor_ - 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }
    if (keycode == key_down_) {
        settings_cursor_ = clamp_int(settings_cursor_ + 1, 0, item_count - 1);
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 0 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int option_count = static_cast<int>(sizeof(kPollingOptions) / sizeof(kPollingOptions[0]));
        const int next_index = next_option_index(kPollingOptions, option_count, config_.input.polling_hz, direction);
        config_.input.polling_hz = kPollingOptions[next_index];
        input_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int option_count = static_cast<int>(sizeof(kDebounceMsOptions) / sizeof(kDebounceMsOptions[0]));
        const int current = static_cast<int>(std::llround(config_.input.debounce_ms));
        const int next_index = next_option_index(kDebounceMsOptions, option_count, current, direction);
        config_.input.debounce_ms = static_cast<double>(kDebounceMsOptions[next_index]);
        input_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_ || keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (input_dirty_) {
            persist_runtime_config();
            restart_input_thread();
            input_dirty_ = false;
        }
        publish_snapshot();
    }
}

void MenuApp::populate_graphics_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, "Display", display_label(config_.graphics.display_mode), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, "Resolution", resolution_label(config_.graphics.resolution), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, "Refresh Hz", std::to_string(config_.graphics.refresh_hz), settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, "VSync", on_off(config_.graphics.vsync), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, "Performance HUD", on_off(config_.graphics.performance_overlay), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, "Display Offset", format_signed_offset_ms(config_.visual_offset_ms), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, "Back", "", settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, true, false);
    render.generic.notes.push_back("Performance HUD shows frame graph, AVG ms/FPS, 0.1%/0.01% lows, and max FPS.");
    render.generic.notes.push_back("Resolution cycles 720p, 1080p, QHD, or the current monitor native size. Refresh Hz ranges from 60 to 1050.");
    render.generic.notes.push_back("Menu rendering is capped at 300 Hz. Gameplay uses the configured value up to 1050 Hz.");
    render.generic.notes.push_back("Display Offset shifts only visuals from -500ms to +500ms. Positive values draw notes earlier.");
    render.generic.notes.push_back("Display, Resolution, Refresh Hz, and VSync apply immediately. Back saves and returns.");
}

void MenuApp::populate_input_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, "Polling Hz", std::to_string(config_.input.polling_hz), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, "Debounce", format_decimal(config_.input.debounce_ms) + " ms", settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, "Back", "", settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, true, false);
    render.generic.notes.push_back("Debounce filters duplicate switch chatter on a single key before gameplay sees it.");
    render.generic.notes.push_back("Left/Right or click +/- to change. Back saves and returns.");
}

}  // namespace tenriff::app
