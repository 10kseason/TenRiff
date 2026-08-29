#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "app/menu/MenuScreen.h"

namespace tenriff::app::menu {

// Explicit values are part of the renderer-hit contract. Append new identifiers
// instead of reordering existing ones.
enum class OptionsItemId : std::uint8_t {
    KeyMode = 0,
    Keymap = 1,
    Skins = 2,
    Graphics = 3,
    Audio = 4,
    Input = 5,
    Calibration = 6,
    ProfileSetup = 7,
};

struct OptionsItemRoute {
    OptionsItemId id;
    Screen destination;
};

// The order is also the existing four-column, two-row visual layout.
inline constexpr std::array<OptionsItemRoute, 8> kOptionsItemRoutes{{
    {OptionsItemId::KeyMode, Screen::ModeSelect},
    {OptionsItemId::Keymap, Screen::Keymap},
    {OptionsItemId::Skins, Screen::SettingsSkins},
    {OptionsItemId::Graphics, Screen::SettingsGraphics},
    {OptionsItemId::Audio, Screen::SettingsAudio},
    {OptionsItemId::Input, Screen::SettingsInput},
    {OptionsItemId::Calibration, Screen::SettingsCalibration},
    {OptionsItemId::ProfileSetup, Screen::QuickSetup},
}};

[[nodiscard]] std::optional<std::size_t> options_item_index(OptionsItemId id) noexcept;
[[nodiscard]] std::optional<OptionsItemId> options_item_id_at(std::size_t index) noexcept;

class OptionsHubController {
public:
    explicit OptionsHubController(OptionsItemId initial_cursor = OptionsItemId::KeyMode) noexcept;

    // Movement preserves the existing four-column, two-row clamped grid.
    void move_horizontal(int direction) noexcept;
    void move_vertical(int direction) noexcept;

    // Returns false for an invalid enum value and preserves the current cursor.
    bool set_cursor(OptionsItemId cursor) noexcept;

    [[nodiscard]] OptionsItemId cursor() const noexcept;

    // Reports the selected screen only. MenuApp owns navigation and any screen-
    // entry preparation such as profile, keymap, or skin setup.
    [[nodiscard]] Screen activate() const noexcept;

private:
    std::size_t cursor_index_ = 0;
};

}  // namespace tenriff::app::menu
