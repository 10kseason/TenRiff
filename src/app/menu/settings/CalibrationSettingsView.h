#pragma once

#include "app/menu/settings/CalibrationSettingsController.h"
#include "app/menu/settings/SettingsRowModel.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

using CalibrationSettingsRowModel = SettingsRowModel<CalibrationSettingId>;
using CalibrationSettingsViewModel = SettingsViewModel<CalibrationSettingId>;

class CalibrationSettingsView {
public:
    [[nodiscard]] static CalibrationSettingsViewModel build(
        const CalibrationSettingsController& controller,
        const config::RuntimeConfig& runtime,
        bool use_korean);
};

}  // namespace tenriff::app::menu::settings
