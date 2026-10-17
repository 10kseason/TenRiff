#include "ui/FunctionKeyLabels.h"
#include "app/NativeFileDialog.h"
#include "app/MenuApp.h"

#include <filesystem>
#include <iterator>
#include <optional>
#include <utility>

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
#include "app/menu/settings/CalibrationSettingsView.h"
#include "app/menu/settings/GraphicsSettingsView.h"
#include "app/menu/settings/InputSettingsView.h"
#include "config/KeycodeMap.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

#ifdef _WIN32
std::optional<std::string> pick_onnx_model_dialog_utf8() {
    const NativeFileDialogApartment apartment;
    if (FAILED(apartment.result())) return std::nullopt;

    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog,
                                nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))) ||
        !dialog) {
        return std::nullopt;
    }

    DWORD options = 0;
    if (FAILED(dialog->GetOptions(&options))) {
        return std::nullopt;
    }
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    const COMDLG_FILTERSPEC filters[] = {
        {L"Upscaler model (*.onnx;*.safetensors)", L"*.onnx;*.safetensors"},
        {L"All files (*.*)", L"*.*"},
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetDefaultExtension(L"onnx");
    dialog->SetTitle(L"Select external ONNX / Safetensors upscaler");

    std::optional<std::string> result;
    if (SUCCEEDED(show_native_file_dialog(dialog.Get()))) {
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (SUCCEEDED(dialog->GetResult(&item)) && item) {
            PWSTR raw_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) && raw_path) {
                result = std::filesystem::path(raw_path).u8string();
                CoTaskMemFree(raw_path);
            }
        }
    }
    return result;
}
#endif
}  // namespace

void MenuApp::handle_graphics_settings_input(uint32_t keycode) {
    // Consume a completed native drag before applying a newer explicit choice.
    (void)service_window_size_change();
    if (graphics_settings_controller_.editing_resolution()) {
#ifdef _WIN32
        const auto vk = config::KeycodeMap::polling_vk_for_keycode(keycode);
        if (vk && ((*vk >= '0' && *vk <= '9') || (*vk >= VK_NUMPAD0 && *vk <= VK_NUMPAD9))) {
            const char digit = static_cast<char>(*vk >= VK_NUMPAD0 ? '0' + *vk - VK_NUMPAD0 : *vk);
            apply_graphics_settings_effects(graphics_settings_controller_.resolution_text(std::string(1, digit)));
            return;
        }
#endif
        if (keycode == key_backspace_ || keycode == key_delete_) {
            apply_graphics_settings_effects(graphics_settings_controller_.resolution_erase(keycode == key_delete_));
            return;
        }
        if (keycode == key_tab_) {
            apply_graphics_settings_effects(graphics_settings_controller_.handle(menu::MenuAction::move(1), config_));
            return;
        }
        if (keycode != key_up_ && keycode != key_down_ && keycode != key_left_ &&
            keycode != key_right_ && keycode != key_enter_ && keycode != key_escape_) return;
    }
    if (keycode == key_f5_) {
        refresh_graphics_resolutions();
        publish_snapshot();
        return;
    }
    menu::settings::GraphicsSettingsEffects effects;
    if (keycode == key_up_) {
        effects = graphics_settings_controller_.handle(menu::MenuAction::move(-1), config_);
    } else if (keycode == key_down_) {
        effects = graphics_settings_controller_.handle(menu::MenuAction::move(1), config_);
    } else if (keycode == key_left_) {
        effects = graphics_settings_controller_.handle(menu::MenuAction::adjust(-1), config_);
    } else if (keycode == key_right_) {
        effects = graphics_settings_controller_.handle(menu::MenuAction::adjust(1), config_);
    } else if (keycode == key_enter_) {
        effects = graphics_settings_controller_.handle(menu::MenuAction::activate(), config_);
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = graphics_settings_controller_.handle(menu::MenuAction::back(), config_);
    }
    apply_graphics_settings_effects(effects);
}

void MenuApp::apply_graphics_settings_effects(
    const menu::settings::GraphicsSettingsEffects& requested_effects) {
    if (requested_effects.empty()) {
        return;
    }

    auto effects = requested_effects;
    bool model_selection_allows_confirmation = true;
    if (effects.choose_onnx_model) {
#ifdef _WIN32
        const auto selected = pick_onnx_model_dialog_utf8();
        model_selection_allows_confirmation = selected.has_value();
        if (selected.has_value()) {
            effects.merge(graphics_settings_controller_.set_onnx_model_path(
                config_, selected.value()));
        }
#endif
    }
    if (effects.show_onnx_confirmation && model_selection_allows_confirmation) {
        graphics_settings_controller_.prepare_onnx_confirmation();
        push_screen(Screen::OnnxUpscalerConfirm);
        effects.menu.render_changed = true;
    }
    if (effects.apply_runtime_graphics) {
        apply_runtime_graphics_config();
    }
    if (effects.menu.persist_config) {
        persist_runtime_config();
    }
    if (effects.menu.navigate_back && !pop_screen()) {
        reset_screen(Screen::OptionsHub);
    }
    settings_cursor_ = static_cast<int>(graphics_settings_controller_.selected_id());
    publish_snapshot();
}

void MenuApp::handle_input_settings_input(uint32_t keycode) {
    menu::MenuEffectFlags effects;
    if (keycode == key_up_) {
        effects = input_settings_controller_.handle(
            menu::MenuAction::move(-1), config_, input_backend_fallback_policy_.polling_latched());
    } else if (keycode == key_down_) {
        effects = input_settings_controller_.handle(
            menu::MenuAction::move(1), config_, input_backend_fallback_policy_.polling_latched());
    } else if (keycode == key_left_) {
        effects = input_settings_controller_.handle(
            menu::MenuAction::adjust(-1), config_, input_backend_fallback_policy_.polling_latched());
    } else if (keycode == key_right_) {
        effects = input_settings_controller_.handle(
            menu::MenuAction::adjust(1), config_, input_backend_fallback_policy_.polling_latched());
    } else if (keycode == key_enter_) {
        effects = input_settings_controller_.handle(
            menu::MenuAction::activate(), config_, input_backend_fallback_policy_.polling_latched());
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = input_settings_controller_.handle(
            menu::MenuAction::back(), config_, input_backend_fallback_policy_.polling_latched());
    }
    apply_input_settings_effects(effects);
}

void MenuApp::handle_calibration_settings_input(uint32_t keycode) {
    menu::MenuEffectFlags effects;
    if (keycode == key_up_) {
        effects = calibration_settings_controller_.handle(menu::MenuAction::move(-1), config_);
    } else if (keycode == key_down_) {
        effects = calibration_settings_controller_.handle(menu::MenuAction::move(1), config_);
    } else if (keycode == key_left_) {
        effects = calibration_settings_controller_.handle(menu::MenuAction::adjust(-1), config_);
    } else if (keycode == key_right_) {
        effects = calibration_settings_controller_.handle(menu::MenuAction::adjust(1), config_);
    } else if (keycode == key_enter_) {
        effects = calibration_settings_controller_.handle(menu::MenuAction::activate(), config_);
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = calibration_settings_controller_.handle(menu::MenuAction::back(), config_);
    }
    apply_calibration_settings_effects(effects);
}

void MenuApp::apply_input_settings_effects(const menu::MenuEffectFlags& effects) {
    if (effects.empty()) {
        return;
    }
    if (effects.persist_config) {
        persist_runtime_config();
    }
    if (effects.restart_input) {
        restart_input_thread(effects.reinitialize_input_backend);
    }
    if (effects.navigate_back && !pop_screen()) {
        reset_screen(Screen::OptionsHub);
    }
    settings_cursor_ = static_cast<int>(input_settings_controller_.selected_id());
    publish_snapshot();
}

void MenuApp::apply_calibration_settings_effects(const menu::MenuEffectFlags& effects) {
    if (effects.empty()) {
        return;
    }
    if (effects.persist_config) {
        persist_runtime_config();
    }
    if (effects.navigate_back && !pop_screen()) {
        reset_screen(Screen::OptionsHub);
    }
    settings_cursor_ = static_cast<int>(calibration_settings_controller_.selected_id());
    publish_snapshot();
}

void MenuApp::handle_onnx_upscaler_confirm_input(uint32_t keycode) {
    menu::settings::GraphicsSettingsEffects effects;
    if (keycode == key_up_ || keycode == key_down_ ||
        keycode == key_left_ || keycode == key_right_) {
        effects = graphics_settings_controller_.handle_confirmation(
            menu::MenuAction::move(1), config_);
    } else if (keycode == key_enter_) {
        effects = graphics_settings_controller_.handle_confirmation(
            menu::MenuAction::activate(), config_);
    } else if (keycode == key_escape_ || keycode == key_backspace_) {
        effects = graphics_settings_controller_.handle_confirmation(
            menu::MenuAction::back(), config_);
    }
    apply_graphics_settings_effects(effects);
}

void MenuApp::populate_graphics_settings_render_data(render::MenuRenderData& render) {
    auto view = menu::settings::GraphicsSettingsView::build(
        graphics_settings_controller_, config_, ui_language());
    render.generic.rows.reserve(render.generic.rows.size() + view.rows.size());
    for (auto& source : view.rows) {
        render::MenuRowData row;
        row.category = std::move(source.category);
        row.label = std::move(source.label);
        row.value = std::move(source.value);
        row.selected = source.selected;
        row.activatable = source.activatable;
        row.value_activatable = source.id == menu::settings::GraphicsSettingId::Resolution;
        row.adjustable = source.adjustable;
        row.increment_enabled = source.adjustable;
        row.decrement_enabled = source.adjustable;
        row.target_kind = render::MenuHitTargetKind::SettingsRow;
        row.row_index = static_cast<int>(source.id);
        render.generic.rows.push_back(std::move(row));
    }
    render.generic.notes = std::move(view.notes);
    for (auto& note : render.generic.notes) note = ui::function_key_labels(note, config::function_key_labels(config_.ui));
    if (!graphics_resolution_status_.empty()) render.generic.notes.insert(render.generic.notes.begin(), graphics_resolution_status_);
    if (graphics_settings_controller_.editing_resolution()) {
        render.generic.footer_notes.push_back(ui_text(
            graphics_settings_controller_.resolution_error() ? "Invalid size: enter whole numbers from 320 to 8192 for both dimensions."
                : "Type Width / Height. Tab or arrows switch fields; Enter applies; Esc cancels; Delete clears.",
            graphics_settings_controller_.resolution_error() ? "잘못된 크기입니다. 가로·세로 모두 320~8192 사이 정수를 입력하세요."
                : "가로·세로 입력: Tab 또는 방향키로 항목 전환, Enter 적용, Esc 취소, Delete 지우기."));
    }
}

void MenuApp::populate_onnx_upscaler_confirm_render_data(render::MenuRenderData& render) {
    render.generic.footer_reserved_lines = 3;
    auto view = menu::settings::GraphicsSettingsView::build_onnx_confirmation(
        graphics_settings_controller_, ui_language());
    render.generic.rows.reserve(render.generic.rows.size() + view.rows.size());
    for (auto& source : view.rows) {
        render::MenuRowData row;
        row.category = std::move(source.category);
        row.label = std::move(source.label);
        row.value = std::move(source.value);
        row.selected = source.selected;
        row.activatable = source.activatable;
        row.target_kind = render::MenuHitTargetKind::SettingsRow;
        row.row_index = static_cast<int>(source.id);
        render.generic.rows.push_back(std::move(row));
    }
    render.generic.notes = std::move(view.notes);
    for (auto& note : render.generic.notes) note = ui::function_key_labels(note, config::function_key_labels(config_.ui));
}

void MenuApp::populate_input_settings_render_data(render::MenuRenderData& render) {
    auto view = menu::settings::InputSettingsView::build(
        input_settings_controller_,
        config_,
        input_backend_fallback_policy_.polling_latched(),
        ui_language());
    render.generic.rows.reserve(render.generic.rows.size() + view.rows.size());
    for (auto& source : view.rows) {
        render::MenuRowData row;
        row.category = std::move(source.category);
        row.label = std::move(source.label);
        row.value = std::move(source.value);
        row.selected = source.selected;
        row.activatable = source.activatable;
        row.adjustable = source.adjustable;
        row.increment_enabled = source.adjustable;
        row.decrement_enabled = source.adjustable;
        row.target_kind = render::MenuHitTargetKind::SettingsRow;
        row.row_index = static_cast<int>(source.id);
        render.generic.rows.push_back(std::move(row));
    }
    render.generic.notes = std::move(view.notes);
    for (auto& note : render.generic.notes) note = ui::function_key_labels(note, config::function_key_labels(config_.ui));
}

void MenuApp::populate_calibration_settings_render_data(render::MenuRenderData& render) {
    auto view = menu::settings::CalibrationSettingsView::build(
        calibration_settings_controller_, config_, ui_language());
    render.generic.rows.reserve(render.generic.rows.size() + view.rows.size());
    for (auto& source : view.rows) {
        render::MenuRowData row;
        row.category = std::move(source.category);
        row.label = std::move(source.label);
        row.value = std::move(source.value);
        row.selected = source.selected;
        row.activatable = source.activatable;
        row.adjustable = source.adjustable;
        row.increment_enabled = source.adjustable;
        row.decrement_enabled = source.adjustable;
        row.target_kind = render::MenuHitTargetKind::SettingsRow;
        row.row_index = static_cast<int>(source.id);
        render.generic.rows.push_back(std::move(row));
    }
    render.generic.notes = std::move(view.notes);
    for (auto& note : render.generic.notes) note = ui::function_key_labels(note, config::function_key_labels(config_.ui));
}

}  // namespace tenriff::app
