#pragma once

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
        bool use_korean);

    [[nodiscard]] static OnnxUpscalerConfirmViewModel build_onnx_confirmation(
        const GraphicsSettingsController& controller,
        bool use_korean);
};

}  // namespace tenriff::app::menu::settings
