#include "app/MenuApp.h"

#include <cmath>
#include <filesystem>
#include <iterator>
#include <optional>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#endif

#include "app/GraphicsTiming.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSkinUtils.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

const int kPollingOptions[] = {1000, 2000, 4000, 8000};
const int kDebounceMsOptions[] = {0, 2, 4, 6, 8, 10, 12};
const int kCalibrationStepOptions[] = {1, 5, 10, 25};

std::string onnx_model_label(std::string_view model_path, std::string_view empty_label) {
    if (model_path.empty()) {
        return std::string(empty_label);
    }
    try {
        const std::string filename = util::path_from_utf8_lossy(model_path).filename().u8string();
        return util::sanitize_ui_text(filename.empty() ? std::string(model_path) : filename);
    } catch (...) {
        return util::sanitize_ui_text(model_path);
    }
}

#ifdef _WIN32
std::optional<std::string> pick_onnx_model_dialog_utf8() {
    const HRESULT init_hr =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool should_uninitialize = SUCCEEDED(init_hr);

    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog,
                                nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))) ||
        !dialog) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    DWORD options = 0;
    if (FAILED(dialog->GetOptions(&options))) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    const COMDLG_FILTERSPEC filters[] = {
        {L"ONNX model (*.onnx)", L"*.onnx"},
        {L"All files (*.*)", L"*.*"},
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetDefaultExtension(L"onnx");
    dialog->SetTitle(L"Select external ONNX upscaler");

    std::optional<std::string> result;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (SUCCEEDED(dialog->GetResult(&item)) && item) {
            PWSTR raw_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) && raw_path) {
                result = std::filesystem::path(raw_path).u8string();
                CoTaskMemFree(raw_path);
            }
        }
    }
    if (should_uninitialize) {
        CoUninitialize();
    }
    return result;
}
#endif
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
    const int item_count = 12;
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

    if (settings_cursor_ == 0 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.graphics.display_mode = cycle_display_mode(config_.graphics.display_mode, direction);
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 1 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        config_.graphics.resolution = cycle_resolution_preset(config_.graphics.resolution, direction);
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 2 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        int next_value = config_.graphics.refresh_hz + direction * kRefreshHzStep;
        next_value = clamp_int(next_value, kGraphicsRefreshHzMin, kGraphicsRefreshHzMax);
        config_.graphics.refresh_hz = next_value;
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 3 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.graphics.vsync = !config_.graphics.vsync;
        graphics_dirty_ = true;
        apply_runtime_graphics_config();
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 4 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.graphics.performance_overlay = !config_.graphics.performance_overlay;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 5 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.graphics.bga_enabled = !config_.graphics.bga_enabled;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 6 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.skin.black_playfield_enabled = !config_.skin.black_playfield_enabled;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 7 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        if (config::normalize_background_upscale_mode(config_.graphics.background_upscale_mode) == "onnx") {
            config_.graphics.background_upscale_mode = "off";
            graphics_dirty_ = true;
        } else {
#ifdef _WIN32
            if (config_.graphics.background_upscale_model_path.empty()) {
                const auto selected = pick_onnx_model_dialog_utf8();
                if (!selected.has_value()) {
                    publish_snapshot();
                    return;
                }
                config_.graphics.background_upscale_model_path = selected.value();
                graphics_dirty_ = true;
            }
#endif
            onnx_upscaler_confirm_cursor_ = 1;
            screen_ = Screen::OnnxUpscalerConfirm;
        }
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 8 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
#ifdef _WIN32
        if (const auto selected = pick_onnx_model_dialog_utf8(); selected.has_value()) {
            config_.graphics.background_upscale_model_path = selected.value();
            graphics_dirty_ = true;
        }
#endif
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 9 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.graphics.background_upscale_prefer_npu =
            !config_.graphics.background_upscale_prefer_npu;
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    if (settings_cursor_ == 10 &&
        (keycode == key_left_ || keycode == key_right_ || keycode == key_enter_)) {
        config_.ui.language =
            (config::normalize_ui_language_token(config_.ui.language) == "ko") ? "en" : "ko";
        graphics_dirty_ = true;
        publish_snapshot();
        return;
    }
    const bool back_requested = keycode == key_escape_ || keycode == key_backspace_ ||
                                (keycode == key_enter_ && settings_cursor_ == item_count - 1);
    if (back_requested) {
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
    const int item_count = 4;
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
        const bool requested_rawinput = keycode == key_right_;
        const bool backend_changed = config_.input.rawinput != requested_rawinput;
        const bool retry_rawinput = requested_rawinput && input_backend_fallback_policy_.polling_latched();
        if (backend_changed || retry_rawinput) {
            config_.input.rawinput = requested_rawinput;
            config_.input.backend = requested_rawinput ? "rawinput" : "polling";
            input_dirty_ = true;
            input_backend_dirty_ = true;
        }
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 1 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int option_count = static_cast<int>(sizeof(kPollingOptions) / sizeof(kPollingOptions[0]));
        const int next_index = next_option_index(kPollingOptions, option_count, config_.input.polling_hz, direction);
        config_.input.polling_hz = kPollingOptions[next_index];
        input_dirty_ = true;
        publish_snapshot();
        return;
    }

    if (settings_cursor_ == 2 && (keycode == key_left_ || keycode == key_right_)) {
        const int direction = (keycode == key_left_) ? -1 : 1;
        const int option_count = static_cast<int>(sizeof(kDebounceMsOptions) / sizeof(kDebounceMsOptions[0]));
        const int current = static_cast<int>(std::llround(config_.input.debounce_ms));
        const int next_index = next_option_index(kDebounceMsOptions, option_count, current, direction);
        config_.input.debounce_ms = static_cast<double>(kDebounceMsOptions[next_index]);
        input_dirty_ = true;
        publish_snapshot();
        return;
    }

    const bool back_requested =
        keycode == key_escape_ || keycode == key_backspace_ ||
        (keycode == key_enter_ && settings_cursor_ == item_count - 1);
    if (back_requested) {
        screen_ = submenu_return_screen_;
        settings_cursor_ = 0;
        if (input_dirty_) {
            persist_runtime_config();
            restart_input_thread(input_backend_dirty_);
            input_dirty_ = false;
            input_backend_dirty_ = false;
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

void MenuApp::handle_onnx_upscaler_confirm_input(uint32_t keycode) {
    if (keycode == key_up_ || keycode == key_down_ ||
        keycode == key_left_ || keycode == key_right_) {
        onnx_upscaler_confirm_cursor_ = 1 - onnx_upscaler_confirm_cursor_;
        publish_snapshot();
        return;
    }

    if (keycode == key_enter_) {
        if (onnx_upscaler_confirm_cursor_ == 0) {
            config_.graphics.background_upscale_mode = "onnx";
            graphics_dirty_ = true;
        }
        screen_ = Screen::SettingsGraphics;
        settings_cursor_ = 7;
        publish_snapshot();
        return;
    }

    if (keycode == key_escape_ || keycode == key_backspace_) {
        screen_ = Screen::SettingsGraphics;
        settings_cursor_ = 7;
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
    append_menu_row(render.generic, ui_text("BGA", "BGA 표시"), ui_on_off(config_.graphics.bga_enabled), settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow, 5, false, true);
    append_menu_row(render.generic, ui_text("BGA Behind Notes", "기어 뒤 BGA"),
                    config_.skin.black_playfield_enabled ? ui_text("Blocked", "가림") : ui_text("Visible", "표시"),
                    settings_cursor_ == 6, render::MenuHitTargetKind::SettingsRow, 6, false, true);

    append_menu_row(render.generic, ui_text("BGA Upscaler", "BGA 업스케일러"),
                    ui_on_off(config::normalize_background_upscale_mode(
                                  config_.graphics.background_upscale_mode) == "onnx"),
                    settings_cursor_ == 7,
                    render::MenuHitTargetKind::SettingsRow, 7, false, true);
    append_menu_row(render.generic, ui_text("ONNX Model", "ONNX 모델"),
                    onnx_model_label(config_.graphics.background_upscale_model_path,
                                     ui_text("Select...", "선택...")),
                    settings_cursor_ == 8,
                    render::MenuHitTargetKind::SettingsRow, 8, true, false);
    append_menu_row(render.generic, ui_text("Low-Power DirectX (Experimental)", "저전력 DirectX (실험)"),
                    ui_on_off(config_.graphics.background_upscale_prefer_npu),
                    settings_cursor_ == 9,
                    render::MenuHitTargetKind::SettingsRow, 9, false, true);
    append_menu_row(render.generic, ui_text("Language", "언어"), ui_language_label(config_.ui.language), settings_cursor_ == 10,
                    render::MenuHitTargetKind::SettingsRow, 10, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 11,
                    render::MenuHitTargetKind::SettingsRow, 11, true, false);
    if (normalize_display_mode(config_.graphics.display_mode) == "fullscreen") {
        render.generic.notes.push_back(ui_text(
            "Discord's current voice overlay does not work in Exclusive Fullscreen. Switch Display to Borderless or Windowed.",
            "현재 Discord 음성 오버레이는 독점 전체 화면에서 동작하지 않습니다. 표시 모드를 테두리 없음 또는 창 모드로 바꾸세요."));
    } else {
        render.generic.notes.push_back(ui_text(
            "Discord voice overlay: pin Voice at bottom-left and keep Performance HUD off to avoid covering gameplay information.",
            "Discord 음성 오버레이는 Voice를 좌하단에 고정하고 성능 HUD를 끄면 게임 정보와 가장 덜 겹칩니다."));
    }
    render.generic.notes.push_back(ui_text("Performance HUD shows frame graph, AVG ms/FPS, 0.1%/0.01% lows, and max FPS.",
                                           "성능 HUD는 프레임 그래프, 평균 ms/FPS, 0.1%/0.01% low, 최대 FPS를 표시합니다."));
    render.generic.notes.push_back(ui_text(
        "BGA OFF suppresses gameplay image/video backgrounds and disables their decoder/upscaler work. Song Select background previews remain visible.",
        "BGA를 끄면 게임플레이 이미지/영상 배경과 디코더/업스케일러 작업이 비활성화됩니다. 선곡 배경 미리보기는 유지됩니다."));
    render.generic.notes.push_back(ui_text("Resolution cycles 720p, 1080p, QHD, or the current monitor native size. Refresh Hz ranges from 60 to 1050.",
                                           "해상도는 720p, 1080p, QHD, 모니터 기본 크기를 순환합니다. 주사율은 60~1050Hz 범위입니다."));
    render.generic.notes.push_back(ui_text(
        "BGA Behind Notes blocks bright backgrounds only under the playfield while keeping BGA visible outside it.",
        "기어 뒤 BGA를 가리면 바깥 BGA는 유지하면서 노트 영역 아래의 밝은 배경만 차단합니다."));
    render.generic.notes.push_back(ui_text("Menu rendering is capped at 300 Hz. Gameplay uses the configured value up to 1050 Hz.",
                                           "메뉴 렌더링은 300Hz까지 제한되고, 게임플레이는 설정값을 최대 1050Hz까지 사용합니다."));
    render.generic.notes.push_back(ui_text(
        "The ONNX upscaler has no automatic performance benchmark. It is intended for high-spec systems and may cause stutter or heavy accelerator load.",
        "ONNX 업스케일러는 자동 성능 벤치마크 없이 실행됩니다. 고사양 시스템용이며 끊김이나 높은 가속기 부하가 생길 수 있습니다."));
    render.generic.notes.push_back(ui_text(
        "Selecting or dropping an .onnx file changes only the model path. Turn BGA Upscaler ON separately and confirm the warning. Failures keep native scaling.",
        "ONNX 파일 선택·드롭은 모델 경로만 바꿉니다. BGA 업스케일러를 별도로 켜고 경고를 확인하세요. 실패 시 원본 확대를 유지합니다."));
    render.generic.notes.push_back(ui_text(
        "FP32/FP16 model I/O and float-boundary INT8 QDQ metadata are detected automatically. Low-Power DirectX requests DirectXMinPower; it does not select or verify an NPU.",
        "FP32/FP16 모델 입출력과 float 경계 INT8 QDQ 메타데이터를 자동 감지합니다. 저전력 DirectX는 DirectXMinPower 요청이며 NPU를 명시 선택하거나 검증하지 않습니다."));
    render.generic.notes.push_back(ui_text("Language changes the menu UI immediately. Visual Latency is available in Skin Settings.",
                                           "언어는 메뉴 UI에 즉시 반영됩니다. 비주얼 레이턴시는 스킨 설정에서 조정합니다."));
    render.generic.notes.push_back(ui_text("Display, Resolution, Refresh Hz, and VSync apply immediately. Back saves and returns.",
                                           "표시 모드, 해상도, 주사율, VSync는 즉시 적용됩니다. 뒤로 가면 저장 후 돌아갑니다."));
}

void MenuApp::populate_onnx_upscaler_confirm_render_data(render::MenuRenderData& render) {
    render.generic.footer_reserved_lines = 3;
    append_menu_row(render.generic,
                    ui_text("Yes, enable ONNX", "예, ONNX 켜기"),
                    ui_text("High-spec mode", "고사양 모드"),
                    onnx_upscaler_confirm_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    true,
                    false);
    append_menu_row(render.generic,
                    ui_text("No, keep native", "아니오, 원본 유지"),
                    ui_text("Recommended default", "권장 기본값"),
                    onnx_upscaler_confirm_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    true,
                    false);
    render.generic.notes.push_back(ui_text(
        "This feature runs the selected ONNX model without a benchmark cutoff.",
        "이 기능은 선택한 ONNX 모델을 벤치마크 차단 없이 실행합니다."));
    render.generic.notes.push_back(ui_text(
        "A high-spec GPU/accelerator is recommended. Slow models can reduce menu or gameplay smoothness.",
        "고사양 GPU/가속기를 권장합니다. 느린 모델은 메뉴나 플레이 화면을 끊기게 할 수 있습니다."));
    render.generic.notes.push_back(ui_text(
        "Yes applies now. No, Esc, or Backspace leaves the upscaler OFF.",
        "예를 누르면 바로 적용합니다. 아니오, Esc, Backspace는 업스케일러를 끈 상태로 둡니다."));
}

void MenuApp::populate_input_settings_render_data(render::MenuRenderData& render) {
    std::string backend_value = config_.input.rawinput ? "RawInput" : "Polling";
    if (config_.input.rawinput && input_backend_fallback_policy_.polling_latched()) {
        backend_value += " (active: Polling)";
    }
    append_menu_row(render.generic, ui_text("Backend", "입력 백엔드"), backend_value, settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow, 0, false, true);
    append_menu_row(render.generic, ui_text("Polling Hz", "폴링 Hz"), std::to_string(config_.input.polling_hz), settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow, 1, false, true);
    append_menu_row(render.generic, ui_text("Debounce", "디바운스"), format_decimal(config_.input.debounce_ms) + " ms", settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow, 2, false, true);
    append_menu_row(render.generic, ui_text("Back", "뒤로"), "", settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow, 3, true, false);
    render.generic.notes.push_back(ui_text("Backend selects RawInput or Polling for the saved profile.",
                                           "입력 백엔드는 프로필에 저장할 RawInput 또는 Polling을 선택합니다."));
    render.generic.notes.push_back(ui_text("After a confirmed RawInput failure, this app run stays on Polling so menu and note input keep working.",
                                           "RawInput 고장이 확인되면 이번 실행 동안 Polling을 유지해 메뉴와 노트 입력이 계속 작동합니다."));
    render.generic.notes.push_back(ui_text("Left selects Polling. Right selects or retries RawInput and clears the runtime fallback.",
                                           "왼쪽은 Polling을 선택하고, 오른쪽은 런타임 대체를 해제해 RawInput을 선택하거나 다시 시도합니다."));
    render.generic.notes.push_back(ui_text("During gameplay, RawInput also uses Polling Hz as an always-on bound-key backup.",
                                           "플레이 중 RawInput은 폴링 Hz 주기로 노트 키를 항상 보조 감시합니다."));
    render.generic.notes.push_back(ui_text("Debounce filters duplicate switch chatter on a single key before gameplay sees it.",
                                           "디바운스는 플레이에 전달되기 전 한 키의 중복 스위치 채터링을 걸러냅니다."));
    render.generic.notes.push_back(ui_text("Left/Right or click +/- to change. Back saves and returns.",
                                           "좌우 키 또는 +/- 클릭으로 변경합니다. 뒤로 가면 저장됩니다."));
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
    append_menu_row(render.generic, ui_text("Visual Latency", "비주얼 레이턴시"),
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
    render.generic.notes.push_back(ui_text("Step 2: use Visual Latency only for what you see. Positive values draw notes earlier.",
                                           "2단계: 비주얼 레이턴시는 화면만 조정합니다. 양수일수록 노트가 더 일찍 보입니다."));
    render.generic.notes.push_back(ui_text("Use a familiar chart, retry quickly from Result, and compare fast/slow feedback until both feel centered.",
                                           "익숙한 차트를 고른 뒤 결과 화면에서 빠르게 재시작하면서 빠름/느림 피드백이 중앙에 모일 때까지 조정하세요."));
    render.generic.notes.push_back(ui_text("Changes save immediately so the next launch uses the same calibration.",
                                           "변경은 즉시 저장되므로 다음 플레이에도 같은 보정값이 적용됩니다."));
}

}  // namespace tenriff::app
