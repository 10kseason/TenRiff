#pragma once

#include <string_view>

#include "app/menu/MenuScreen.h"

namespace tenriff::app::menu {

enum class SnapshotViewKind {
    QuickSetup,
    Title,
    SongSelect,
    SongBrowser,
    Gameplay,
    Result,
    Generic,
};

enum class GenericViewKind {
    None,
    SessionMix,
    OptionsHub,
    Multiplayer,
    AudioSettings,
    GraphicsSettings,
    SkinSettings,
    InputSettings,
    CalibrationSettings,
    ModeSettings,
    ModManager,
    Keymap,
    KeymapConfirm,
    OnnxUpscalerConfirm,
    KeymapTest,
};

struct MenuScreenDescriptor {
    Screen screen = Screen::Title;
    std::string_view english_title;
    std::string_view korean_title;
    std::string_view background_key;
    std::string_view background_fallback_key;
    SnapshotViewKind snapshot_view = SnapshotViewKind::Generic;
    GenericViewKind generic_view = GenericViewKind::None;
    bool options_family = false;
    bool shows_input_footer = true;
};

[[nodiscard]] const MenuScreenDescriptor& screen_descriptor(Screen screen) noexcept;

}  // namespace tenriff::app::menu
