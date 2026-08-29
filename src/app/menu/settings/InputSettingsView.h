#pragma once

#include "app/menu/settings/InputSettingsController.h"
#include "app/menu/settings/SettingsRowModel.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

using InputSettingsRowModel = SettingsRowModel<InputSettingId>;
using InputSettingsViewModel = SettingsViewModel<InputSettingId>;

class InputSettingsView {
public:
    [[nodiscard]] static InputSettingsViewModel build(
        const InputSettingsController& controller,
        const config::RuntimeConfig& runtime,
        bool is_polling_fallback_latched,
        bool use_korean);
};

}  // namespace tenriff::app::menu::settings
