#include "doctest/doctest.h"

#include <array>
#include <cstddef>

#include "app/menu/MenuScreenDescriptor.h"

namespace {

using tenriff::app::menu::GenericViewKind;
using tenriff::app::menu::Screen;
using tenriff::app::menu::SnapshotViewKind;
using tenriff::app::menu::screen_descriptor;

constexpr std::array<Screen, 20> kScreens{
    Screen::QuickSetup,
    Screen::Title,
    Screen::OptionsHub,
    Screen::Multiplayer,
    Screen::SongSelect,
    Screen::SessionMix,
    Screen::SongBrowser,
    Screen::Gameplay,
    Screen::SettingsAudio,
    Screen::SettingsGraphics,
    Screen::SettingsSkins,
    Screen::SettingsInput,
    Screen::SettingsCalibration,
    Screen::ModeSelect,
    Screen::ModeMods,
    Screen::Keymap,
    Screen::KeymapConfirm,
    Screen::OnnxUpscalerConfirm,
    Screen::KeymapTest,
    Screen::Result,
};

TEST_CASE("every menu screen has a stable descriptor") {
    for (const Screen screen : kScreens) {
        const auto& descriptor = screen_descriptor(screen);
        CHECK(descriptor.screen == screen);
        CHECK_FALSE(descriptor.english_title.empty());
        CHECK_FALSE(descriptor.korean_title.empty());
        if (screen != Screen::Gameplay) {
            CHECK_FALSE(descriptor.background_key.empty());
        }
    }
}

TEST_CASE("settings screens share the settings skin background fallback") {
    constexpr std::array<Screen, 5> settings_screens{
        Screen::SettingsAudio,
        Screen::SettingsGraphics,
        Screen::SettingsSkins,
        Screen::SettingsInput,
        Screen::SettingsCalibration,
    };

    for (const Screen screen : settings_screens) {
        const auto& descriptor = screen_descriptor(screen);
        CHECK(descriptor.snapshot_view == SnapshotViewKind::Generic);
        CHECK(descriptor.background_fallback_key == "settings");
        CHECK(descriptor.options_family);
    }
}

TEST_CASE("generic routing is explicit for nested options screens") {
    CHECK(screen_descriptor(Screen::OptionsHub).generic_view == GenericViewKind::OptionsHub);
    CHECK(screen_descriptor(Screen::SettingsAudio).generic_view == GenericViewKind::AudioSettings);
    CHECK(screen_descriptor(Screen::SettingsGraphics).generic_view == GenericViewKind::GraphicsSettings);
    CHECK(screen_descriptor(Screen::SettingsSkins).generic_view == GenericViewKind::SkinSettings);
    CHECK(screen_descriptor(Screen::SettingsInput).generic_view == GenericViewKind::InputSettings);
    CHECK(screen_descriptor(Screen::SettingsCalibration).generic_view == GenericViewKind::CalibrationSettings);
    CHECK(screen_descriptor(Screen::ModeSelect).generic_view == GenericViewKind::ModeSettings);
    CHECK(screen_descriptor(Screen::ModeMods).generic_view == GenericViewKind::ModManager);
    CHECK(screen_descriptor(Screen::Keymap).generic_view == GenericViewKind::Keymap);
    CHECK(screen_descriptor(Screen::KeymapConfirm).generic_view == GenericViewKind::KeymapConfirm);
    CHECK(screen_descriptor(Screen::OnnxUpscalerConfirm).generic_view == GenericViewKind::OnnxUpscalerConfirm);
    CHECK(screen_descriptor(Screen::KeymapTest).generic_view == GenericViewKind::KeymapTest);
}

TEST_CASE("key input screens own their footer content") {
    CHECK_FALSE(screen_descriptor(Screen::Keymap).shows_input_footer);
    CHECK_FALSE(screen_descriptor(Screen::KeymapTest).shows_input_footer);
    CHECK(screen_descriptor(Screen::KeymapConfirm).shows_input_footer);
    CHECK_FALSE(screen_descriptor(Screen::Gameplay).shows_input_footer);
}

TEST_CASE("an invalid screen value falls back to the title descriptor") {
    const auto invalid = static_cast<Screen>(999);
    CHECK(screen_descriptor(invalid).screen == Screen::Title);
}

}  // namespace
