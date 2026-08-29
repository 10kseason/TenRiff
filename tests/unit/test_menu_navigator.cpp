#include "doctest/doctest.h"

#include <array>
#include <cstddef>

#include "app/menu/MenuNavigator.h"

namespace {

using tenriff::app::menu::MenuNavigator;
using tenriff::app::menu::Screen;

TEST_CASE("menu screen preserves the existing exhaustive screen order") {
    constexpr std::array<Screen, 20> screens{
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

    for (std::size_t index = 0; index < screens.size(); ++index) {
        CHECK(static_cast<std::size_t>(screens[index]) == index);
    }
}

TEST_CASE("reset starts a new root flow and clears previous history") {
    MenuNavigator navigator(Screen::Title);
    navigator.push(Screen::OptionsHub);
    navigator.push(Screen::SettingsAudio);

    navigator.reset(Screen::SongSelect);

    CHECK(navigator.current() == Screen::SongSelect);
    CHECK(navigator.depth() == 1);
    CHECK_FALSE(navigator.can_back());
    CHECK_FALSE(navigator.back());
    CHECK(navigator.current() == Screen::SongSelect);
}

TEST_CASE("options audio navigation returns one level at a time to each origin") {
    constexpr std::array<Screen, 3> origins{
        Screen::Title,
        Screen::SongSelect,
        Screen::Multiplayer,
    };

    for (const Screen origin : origins) {
        MenuNavigator navigator(origin);

        navigator.push(Screen::OptionsHub);
        navigator.push(Screen::SettingsAudio);

        CHECK(navigator.current() == Screen::SettingsAudio);
        CHECK(navigator.root() == origin);
        REQUIRE(navigator.parent().has_value());
        CHECK(navigator.parent().value() == Screen::OptionsHub);
        CHECK(navigator.depth() == 3);
        CHECK(navigator.can_back());

        REQUIRE(navigator.back());
        CHECK(navigator.current() == Screen::OptionsHub);
        CHECK(navigator.depth() == 2);

        REQUIRE(navigator.back());
        CHECK(navigator.current() == origin);
        CHECK(navigator.depth() == 1);
        CHECK_FALSE(navigator.can_back());
    }
}

TEST_CASE("keymap confirmation and test screens return through keymap to options") {
    MenuNavigator navigator(Screen::Title);
    navigator.push(Screen::OptionsHub);
    navigator.push(Screen::Keymap);

    navigator.push(Screen::KeymapConfirm);
    REQUIRE(navigator.back());
    CHECK(navigator.current() == Screen::Keymap);

    navigator.push(Screen::KeymapTest);
    REQUIRE(navigator.back());
    CHECK(navigator.current() == Screen::Keymap);

    REQUIRE(navigator.back());
    CHECK(navigator.current() == Screen::OptionsHub);
    REQUIRE(navigator.back());
    CHECK(navigator.current() == Screen::Title);
}

TEST_CASE("gameplay result replacement does not create stale back history") {
    MenuNavigator navigator(Screen::Gameplay);

    navigator.replace(Screen::Result);

    CHECK(navigator.current() == Screen::Result);
    CHECK(navigator.root() == Screen::Result);
    CHECK(navigator.depth() == 1);
    CHECK_FALSE(navigator.can_back());
    CHECK_FALSE(navigator.back());
}

TEST_CASE("back at a root reports failure without changing the screen") {
    MenuNavigator navigator(Screen::Multiplayer);

    CHECK_FALSE(navigator.can_back());
    CHECK(navigator.root() == Screen::Multiplayer);
    CHECK_FALSE(navigator.parent().has_value());
    CHECK_FALSE(navigator.back());
    CHECK(navigator.current() == Screen::Multiplayer);
    CHECK(navigator.depth() == 1);
}

}  // namespace
