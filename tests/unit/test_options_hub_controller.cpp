#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include "app/menu/OptionsHubController.h"

namespace {

using tenriff::app::menu::OptionsHubController;
using tenriff::app::menu::OptionsItemId;
using tenriff::app::menu::Screen;
using tenriff::app::menu::options_item_id_at;
using tenriff::app::menu::options_item_index;

TEST_CASE("options hub cursor initializes to a valid stable item") {
    const OptionsHubController default_controller;
    CHECK(default_controller.cursor() == OptionsItemId::KeyMode);

    const OptionsHubController explicit_controller(OptionsItemId::ProfileSetup);
    CHECK(explicit_controller.cursor() == OptionsItemId::ProfileSetup);

    const OptionsHubController invalid_controller(static_cast<OptionsItemId>(255));
    CHECK(invalid_controller.cursor() == OptionsItemId::KeyMode);
}

TEST_CASE("options hub item indexes preserve the existing card order and boundaries") {
    constexpr std::array<OptionsItemId, 10> expected_order{
        OptionsItemId::KeyMode,
        OptionsItemId::Keymap,
        OptionsItemId::Skins,
        OptionsItemId::Graphics,
        OptionsItemId::Audio,
        OptionsItemId::Input,
        OptionsItemId::Calibration,
        OptionsItemId::ProfileSetup,
        OptionsItemId::Mods,
        OptionsItemId::KeyTest,
    };

    for (std::size_t index = 0; index < expected_order.size(); ++index) {
        REQUIRE(options_item_id_at(index).has_value());
        CHECK(*options_item_id_at(index) == expected_order[index]);
        REQUIRE(options_item_index(expected_order[index]).has_value());
        CHECK(*options_item_index(expected_order[index]) == index);
    }

    CHECK_FALSE(options_item_id_at(expected_order.size()).has_value());
    CHECK_FALSE(options_item_index(static_cast<OptionsItemId>(255)).has_value());
}

TEST_CASE("options hub has five columns with distinct top and bottom row edges") {
    OptionsHubController controller;
    controller.move_horizontal(20);
    CHECK(controller.cursor() == OptionsItemId::Audio);
    controller.move_vertical(1);
    CHECK(controller.cursor() == OptionsItemId::KeyTest);
    controller.move_horizontal(-20);
    CHECK(controller.cursor() == OptionsItemId::Input);
    controller.move_vertical(-1);
    CHECK(controller.cursor() == OptionsItemId::KeyMode);
}

TEST_CASE("options hub movement exhaustively preserves the five-by-two grid") {
    constexpr std::array<int, 5> directions{-2, -1, 0, 1, 2};

    for (std::size_t start = 0; start < 10; ++start) {
        REQUIRE(options_item_id_at(start).has_value());
        for (const int direction : directions) {
            OptionsHubController horizontal(*options_item_id_at(start));
            horizontal.move_horizontal(direction);
            REQUIRE(options_item_index(horizontal.cursor()).has_value());
            const int start_column = static_cast<int>(start % 5);
            const int expected_column = std::clamp(start_column + direction, 0, 4);
            const std::size_t expected_horizontal = (start / 5) * 5 +
                static_cast<std::size_t>(expected_column);
            CHECK(*options_item_index(horizontal.cursor()) == expected_horizontal);

            OptionsHubController vertical(*options_item_id_at(start));
            vertical.move_vertical(direction);
            REQUIRE(options_item_index(vertical.cursor()).has_value());
            const int start_row = static_cast<int>(start / 5);
            const int expected_row = std::clamp(start_row + direction, 0, 1);
            const std::size_t expected_vertical =
                static_cast<std::size_t>(expected_row) * 5 + start % 5;
            CHECK(*options_item_index(vertical.cursor()) == expected_vertical);
        }
    }
}

TEST_CASE("options hub activation maps every card to its destination screen") {
    constexpr std::array<std::pair<OptionsItemId, Screen>, 10> routes{{
        {OptionsItemId::KeyMode, Screen::ModeSelect},
        {OptionsItemId::Keymap, Screen::Keymap},
        {OptionsItemId::Skins, Screen::SettingsSkins},
        {OptionsItemId::Graphics, Screen::SettingsGraphics},
        {OptionsItemId::Audio, Screen::SettingsAudio},
        {OptionsItemId::Input, Screen::SettingsInput},
        {OptionsItemId::Calibration, Screen::SettingsCalibration},
        {OptionsItemId::ProfileSetup, Screen::QuickSetup},
        {OptionsItemId::Mods, Screen::ModeMods},
        {OptionsItemId::KeyTest, Screen::KeymapTest},
    }};

    OptionsHubController controller;
    for (const auto& route : routes) {
        REQUIRE(controller.set_cursor(route.first));
        CHECK(controller.cursor() == route.first);
        CHECK(controller.activate() == route.second);
    }
}

TEST_CASE("options hub rejects an invalid pointer item without changing selection") {
    OptionsHubController controller(OptionsItemId::Audio);

    CHECK_FALSE(controller.set_cursor(static_cast<OptionsItemId>(200)));
    CHECK(controller.cursor() == OptionsItemId::Audio);
    CHECK(controller.activate() == Screen::SettingsAudio);
}

}  // namespace
