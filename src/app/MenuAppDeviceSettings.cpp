#include "app/MenuApp.h"

#include <cmath>

#include "app/GraphicsTiming.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"

namespace tenriff::app {

namespace {

const int kPollingOptions[] = {1000, 2000, 4000, 8000};
const int kDebounceMsOptions[] = {0, 2, 4, 6, 8, 10, 12};
const int kCalibrationStepOptions[] = {1, 5, 10, 25};

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
    const int item_count = 8;
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
        config_.ui.language =
            (config::normalize_ui_language_token(config_.ui.language) == "ko") ? "en" : "ko";
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 6 && (keycode == key_left_ || keycode == key_right_)) {
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

void MenuApp::handle_calibration_settings_input(uint32_t keycode) {
    const int item_count = 5;
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
        const int option_count = static_cast<int>(sizeof(kCalibrationStepOptions) / sizeof(kCalibrationStepOptions[0]));
        const int next_index =
            next_option_index(kCalibrationStepOptions, option_count, calibration_step_ms_, direction);
        calibration_step_ms_ = kCalibrationStepOptions[next_index];
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.input_offset_ms = clamp_step_value(config_.input_offset_ms +
                                                       direction * static_cast<double>(calibration_step_ms_),
                                                   -250.0, 250.0, static_cast<double>(calibration_step_ms_));
        persist_runtime_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const double direction = (keycode == key_left_) ? -1.0 : 1.0;
        config_.visual_offset_ms = clamp_step_value(config_.visual_offset_ms +
                                                        direction * static_cast<double>(calibration_step_ms_),
                                                    kVisualOffsetMin, kVisualOffsetMax,
                                                    static_cast<double>(calibration_step_ms_));
        persist_runtime_config();
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_) {
        if (settings_cursor_ == 3) {
            config_.input_offset_ms = 0.0;
            config_.visual_offset_ms = 0.0;
            persist_runtime_config();
            publish_snapshot();
            return;
        }
        if (settings_cursor_ == 4) {
            screen_ = submenu_return_screen_;
            settings_cursor_ = 0;
            publish_snapshot();
            return;
        }
    }

    if (keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        publish_snapshot();
    }
}

void MenuApp::populate_graphics_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, ui_text("Display", "표시 모드"), ui_display_mode_label(config_.graphics.display_mode), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, ui_text("Resolution", "해상도"), ui_resolution_label(config_.graphics.resolution), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, ui_text("Refresh Hz", "주사율"), std::to_string(config_.graphics.refresh_hz), settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, "VSync", ui_on_off(config_.graphics.vsync), settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, false, true);
    append_menu_row(render.generic, ui_text("Performance HUD", "성능 HUD"), ui_on_off(config_.graphics.performance_overlay), settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow, 4, false, true);
    append_menu_row(render.generic, ui_text("Language", "언어"), ui_language_label(config_.ui.language), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, ui_text("Display Offset", "표시 오프셋"), format_signed_offset_ms(config_.visual_offset_ms), settings_cursor_ == 6,
                    render::MenuHitTargetKind::SettingsRow, 6, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 7, render::MenuHitTargetKind::SettingsRow, 7, true, false);
    render.generic.notes.push_back(ui_text("Performance HUD shows frame graph, AVG ms/FPS, 0.1%/0.01% lows, and max FPS.",
                                           "성능 HUD는 프레임 그래프, 평균 ms/FPS, 0.1%/0.01% low, 최대 FPS를 표시합니다."));
    render.generic.notes.push_back(ui_text("Resolution cycles 720p, 1080p, QHD, or the current monitor native size. Refresh Hz ranges from 60 to 1050.",
                                           "해상도는 720p, 1080p, QHD, 모니터 기본 크기를 순환합니다. 주사율은 60~1050Hz 범위입니다."));
    render.generic.notes.push_back(ui_text("Menu rendering is capped at 300 Hz. Gameplay uses the configured value up to 1050 Hz.",
                                           "메뉴 렌더링은 300Hz까지 제한되고, 게임플레이는 설정값을 최대 1050Hz까지 사용합니다."));
    render.generic.notes.push_back(ui_text("Language changes the menu UI immediately. Display Offset shifts only visuals from -500ms to +500ms.",
                                           "언어는 메뉴 UI에 즉시 반영됩니다. 표시 오프셋은 시각 요소만 -500ms~+500ms 범위에서 이동합니다."));
    render.generic.notes.push_back(ui_text("Display, Resolution, Refresh Hz, and VSync apply immediately. Back saves and returns.",
                                           "표시 모드, 해상도, 주사율, VSync는 즉시 적용됩니다. 뒤로 가면 저장 후 돌아갑니다."));
}

void MenuApp::populate_input_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, ui_text("Polling Hz", "폴링 Hz"), std::to_string(config_.input.polling_hz), settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, ui_text("Debounce", "디바운스"), format_decimal(config_.input.debounce_ms) + " ms", settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 2, render::MenuHitTargetKind::SettingsRow, 2, true, false);
    render.generic.notes.push_back(ui_text("Debounce filters duplicate switch chatter on a single key before gameplay sees it.",
                                           "디바운스는 게임플레이로 전달되기 전 한 키의 중복 스위치 떨림 입력을 걸러냅니다."));
    render.generic.notes.push_back(ui_text("Left/Right or click +/- to change. Back saves and returns.",
                                           "좌우 키나 +/- 클릭으로 변경합니다. 뒤로 가면 저장 후 돌아갑니다."));
}

void MenuApp::populate_calibration_settings_render_data(render::MenuRenderData& render) {
    append_menu_row(render.generic, ui_text("Adjustment Step", "조정 단위"),
                    std::to_string(calibration_step_ms_) + " ms",
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    false,
                    true);
    append_menu_row(render.generic, ui_text("Input Offset", "입력 오프셋"),
                    format_signed_offset_ms(config_.input_offset_ms),
                    settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    false,
                    true);
    append_menu_row(render.generic, ui_text("Display Offset", "표시 오프셋"),
                    format_signed_offset_ms(config_.visual_offset_ms),
                    settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow,
                    2,
                    false,
                    true);
    append_menu_row(render.generic, ui_text("Reset Offsets", "오프셋 초기화"),
                    "",
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    true,
                    false);
    append_menu_row(render.generic, ui_text("Back", "뒤로"),
                    "",
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    true,
                    false);

    render.generic.notes.push_back(ui_text("Step 1: use Input Offset to match your actual key hit timing to the judgement windows.",
                                           "1단계: 입력 오프셋으로 실제 타건 타이밍을 판정창에 맞추세요."));
    render.generic.notes.push_back(ui_text("Step 2: use Display Offset only for what you see. Positive values draw notes earlier.",
                                           "2단계: 표시 오프셋은 화면만 조정합니다. 양수일수록 노트가 더 일찍 보입니다."));
    render.generic.notes.push_back(ui_text("Use a familiar chart, retry quickly from Result, and compare fast/slow feedback until both feel centered.",
                                           "익숙한 차트를 고른 뒤 결과 화면에서 빠르게 재시작하면서 빠름/느림 피드백이 중앙에 모일 때까지 조정하세요."));
    render.generic.notes.push_back(ui_text("Changes save immediately so the next launch uses the same calibration.",
                                           "변경은 즉시 저장되므로 다음 플레이에도 같은 보정값이 적용됩니다."));
}

}  // namespace tenriff::app
