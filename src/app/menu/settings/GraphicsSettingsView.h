#pragma once

#include "ui/Localization.h"

#include "app/menu/settings/GraphicsSettingsController.h"
#include "app/menu/settings/SettingsRowModel.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

using GraphicsSettingsRowModel = SettingsRowModel<GraphicsSettingId>;
using GraphicsSettingsViewModel = SettingsViewModel<GraphicsSettingId>;
using OnnxUpscalerConfirmRowModel = SettingsRowModel<OnnxUpscalerConfirmId>;
using OnnxUpscalerConfirmViewModel = SettingsViewModel<OnnxUpscalerConfirmId>;

class GraphicsSettingsView {
public:
    [[nodiscard]] static GraphicsSettingsViewModel build(
        const GraphicsSettingsController& controller,
        const config::RuntimeConfig& runtime,
        ui::Language language);

    [[nodiscard]] static OnnxUpscalerConfirmViewModel build_onnx_confirmation(
        const GraphicsSettingsController& controller,
        ui::Language language);
};

}  // namespace tenriff::app::menu::settings
