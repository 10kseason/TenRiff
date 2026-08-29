#pragma once

namespace tenriff::app::menu {

// Keep this list exhaustive and in sync with the application's screen dispatcher.
enum class Screen {
    QuickSetup,
    Title,
    OptionsHub,
    Multiplayer,
    SongSelect,
    SessionMix,
    SongBrowser,
    Gameplay,
    SettingsAudio,
    SettingsGraphics,
    SettingsSkins,
    SettingsInput,
    SettingsCalibration,
    ModeSelect,
    ModeMods,
    Keymap,
    KeymapConfirm,
    OnnxUpscalerConfirm,
    KeymapTest,
    Result,
};

}  // namespace tenriff::app::menu
