#pragma once

#include "ui/Localization.h"

#include "app/menu/settings/AudioSettingsController.h"
#include "app/menu/settings/SettingsRowModel.h"
#include "config/Config.h"

namespace tenriff::app::menu::settings {

using AudioSettingsRowModel = SettingsRowModel<AudioSettingId>;
using AudioSettingsViewModel = SettingsViewModel<AudioSettingId>;

class AudioSettingsView {
public:
    [[nodiscard]] static AudioSettingsViewModel build(
        const AudioSettingsController& controller,
        const config::RuntimeConfig& runtime,
        ui::Language language);
};

}  // namespace tenriff::app::menu::settings
