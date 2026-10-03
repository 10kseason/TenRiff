#include "doctest/doctest.h"

#include <cstdint>
#include <string>
#include <unordered_set>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/KeymapSettingsController.h"
#include "app/menu/settings/KeymapSettingsView.h"
#include "config/KeycodeMap.h"
#include "config/Keymap.h"

namespace {

using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::KeymapActionId;
using tenriff::app::menu::settings::KeymapSettingsController;
using tenriff::app::menu::settings::KeymapSettingsView;

}  // namespace

TEST_CASE("NKRO layout arrows clamp and use each layout's existing bindings") {
    KeymapSettingsController controller;
    controller.reset(4, "4k");
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    CHECK(controller.adjust_test_key_count(-1).empty());
    CHECK(controller.edit_mode() == "4k");
    const auto modes = manager.supported_mode_tokens();
    for (std::size_t i = 1; i < modes.size(); ++i) {
        const auto effect = controller.adjust_test_key_count(1);
        CHECK(effect.refresh_input_scope);
        CHECK_FALSE(effect.menu.persist_config);
        CHECK(controller.edit_mode() == modes[i]);
        const auto view = KeymapSettingsView::build_nkro_test(controller, keymap, {}, "RawInput", tenriff::ui::Language::Korean);
        CHECK(view.rows.size() == controller.lane_ids().size() + 1);
        const auto bindings = manager.bindings_for_mode(keymap, modes[i]);
        REQUIRE_FALSE(controller.lane_ids().empty());
        CHECK(view.rows.front().value == bindings.at(controller.lane_ids().front()));
    }
    CHECK(controller.edit_mode() == "16k");
    CHECK(controller.adjust_test_key_count(1).empty());
    for (std::size_t i = modes.size() - 1; i > 0; --i) {
        CHECK(controller.adjust_test_key_count(-1).refresh_input_scope);
        CHECK(controller.edit_mode() == modes[i - 1]);
    }
    CHECK(controller.adjust_test_key_count(-1).empty());
}

TEST_CASE("keymap action identifiers remain compatible with renderer hits") {
    static_assert(static_cast<std::uint8_t>(KeymapActionId::Reset) == 0);
    static_assert(static_cast<std::uint8_t>(KeymapActionId::NkroTest) == 1);
    static_assert(static_cast<std::uint8_t>(KeymapActionId::Back) == 2);
}

TEST_CASE("keymap controller resolves initial mode and owns dynamic lane selection") {
    KeymapSettingsController controller;
    controller.reset(7, "4k");
    CHECK(controller.edit_mode() == "7k");
    CHECK(controller.lane_ids().size() == 7);
    CHECK(controller.selected_row() == 0);

    static_cast<void>(controller.handle(MenuAction::move(1), 100));
    CHECK(controller.selected_row() == 1);
    REQUIRE(controller.selected_lane().has_value());
    CHECK(*controller.selected_lane() == "lane1");

    for (int index = 0; index < 20; ++index) {
        static_cast<void>(controller.handle(MenuAction::move(1), 100));
    }
    CHECK(controller.selected_row() == 10);
    static_cast<void>(controller.select(7));
    CHECK(*controller.selected_lane() == "lane7");
}

TEST_CASE("keymap mode cycling refreshes lanes and polling scope") {
    KeymapSettingsController controller;
    controller.reset(std::nullopt, "4k");

    auto effects = controller.handle(MenuAction::adjust(1), 100);
    CHECK(controller.edit_mode() == "5k");
    CHECK(controller.lane_ids().size() == 5);
    CHECK(effects.menu.render_changed);
    CHECK(effects.refresh_input_scope);

    effects = controller.handle(MenuAction::adjust(-1), 100);
    CHECK(controller.edit_mode() == "4k");
    effects = controller.handle(MenuAction::adjust(-1), 100);
    CHECK(controller.edit_mode() == "16k");
    CHECK(controller.lane_ids().size() == 16);
}

TEST_CASE("keymap capture has explicit start cancel timeout and completion effects") {
    using tenriff::app::menu::settings::kKeymapCaptureTimeoutNs;

    KeymapSettingsController controller;
    controller.reset(std::nullopt, "4k");
    static_cast<void>(controller.handle(MenuAction::move(1), 0));

    auto effects = controller.handle(MenuAction::activate(), 1'000);
    CHECK(controller.capture_active());
    CHECK(controller.capture_deadline_ns() == 1'000 + kKeymapCaptureTimeoutNs);
    CHECK(effects.refresh_input_scope);

    effects = controller.update_capture_timeout(
        1'000 + kKeymapCaptureTimeoutNs - 1);
    CHECK(effects.empty());
    effects = controller.update_capture_timeout(
        1'000 + kKeymapCaptureTimeoutNs);
    CHECK_FALSE(controller.capture_active());
    CHECK(effects.menu.render_changed);
    CHECK(effects.refresh_input_scope);

    static_cast<void>(controller.handle(MenuAction::activate(), 2'000));
    effects = controller.finish_capture("Saved lane1 = D", 3'000);
    CHECK_FALSE(controller.capture_active());
    CHECK(controller.status_visible(3'000));
    CHECK(controller.status_message() == "Saved lane1 = D");
    CHECK(effects.refresh_input_scope);
}

TEST_CASE("keymap back cancels capture before clearing transient state and navigating") {
    KeymapSettingsController controller;
    controller.reset(std::nullopt, "4k");
    static_cast<void>(controller.handle(MenuAction::move(1), 0));
    static_cast<void>(controller.handle(MenuAction::activate(), 100));
    controller.show_status("temporary", 100);

    const auto cancelled = controller.handle(MenuAction::back(), 200);
    CHECK_FALSE(cancelled.menu.navigate_back);
    CHECK(cancelled.refresh_input_scope);
    CHECK_FALSE(controller.capture_active());
    const auto effects = controller.handle(MenuAction::back(), 300);
    CHECK(effects.menu.navigate_back);
    CHECK(effects.refresh_input_scope);
    CHECK_FALSE(controller.capture_active());
    CHECK(controller.status_message().empty());
}

TEST_CASE("keymap view keeps main rows actions and capture status value-only") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();
    KeymapSettingsController controller;
    controller.reset(4, "10k");
    static_cast<void>(controller.handle(MenuAction::move(1), 0));
    static_cast<void>(controller.handle(MenuAction::activate(), 1'000));

    const auto view = KeymapSettingsView::build(
        controller,
        keymap,
        4,
        "10k",
        "Input: RawInput",
        2'000, tenriff::ui::Language::English);
    REQUIRE(view.rows.size() == 8);
    CHECK(view.rows[0].label == "Key Mode");
    CHECK(view.rows[0].value == "4K");
    CHECK(view.rows[1].label == "Key 1");
    CHECK(view.rows[1].selected);
    CHECK(view.rows[1].value.find("[waiting]") != std::string::npos);
    REQUIRE(view.rows[5].action.has_value());
    CHECK(*view.rows[5].action == KeymapActionId::Reset);
    REQUIRE(view.rows[6].action.has_value());
    CHECK(*view.rows[6].action == KeymapActionId::NkroTest);
    REQUIRE(view.rows[7].action.has_value());
    CHECK(*view.rows[7].action == KeymapActionId::Back);
    CHECK(view.footer_reserved_lines == 6);
}

TEST_CASE("NKRO view highlights only pressed mapped lanes") {
    tenriff::config::KeymapManager manager;
    const auto keymap = manager.default_keymap();
    KeymapSettingsController controller;
    controller.reset(std::nullopt, "4k");
    const auto bindings = manager.bindings_for_mode(keymap, "4k");
    const auto lane1_keycode =
        tenriff::config::KeycodeMap::to_keycode(bindings.at("lane1"));
    REQUIRE(lane1_keycode.has_value());
    const std::unordered_set<std::uint32_t> pressed{*lane1_keycode};

    const auto view = KeymapSettingsView::build_nkro_test(
        controller, keymap, pressed, "Input: Polling", tenriff::ui::Language::Korean);
    REQUIRE(view.rows.size() == 5);
    CHECK(view.rows[0].selected);
    CHECK(view.rows[0].value.find("[눌림]") != std::string::npos);
    CHECK_FALSE(view.rows[1].selected);
    REQUIRE(view.rows.back().action.has_value());
    CHECK(*view.rows.back().action == KeymapActionId::Back);
    CHECK(view.footer_reserved_lines == 2);
}

TEST_CASE("keymap primary and secondary selection mouse capture and keyboard actions are accessible") {
    KeymapSettingsController controller;
    controller.reset(std::nullopt, "10k");
    CHECK(controller.edit_mode() == "4k");
    static_cast<void>(controller.select(2, true));
    CHECK(controller.secondary_selected());
    CHECK(*controller.selected_lane() == "lane2");
    static_cast<void>(controller.handle(MenuAction::activate(), 10));
    CHECK(controller.capture_active());
    CHECK(controller.select(3).empty());
    CHECK(controller.selected_row() == 2);
    static_cast<void>(controller.cancel_capture());
    static_cast<void>(controller.handle(MenuAction::adjust(-1), 20));
    CHECK_FALSE(controller.secondary_selected());
    static_cast<void>(controller.handle(MenuAction::adjust(1), 20));
    CHECK(controller.secondary_selected());
    static_cast<void>(controller.select(5));
    CHECK(controller.handle(MenuAction::activate(), 30).reset_bindings);
    static_cast<void>(controller.select(6));
    CHECK(controller.handle(MenuAction::activate(), 30).open_nkro_test);
    static_cast<void>(controller.select(7));
    CHECK(controller.handle(MenuAction::activate(), 30).menu.navigate_back);
}

TEST_CASE("secondary assignment appears on its piano key and participates in NKRO feedback") {
    tenriff::config::KeymapManager manager;
    auto keymap = manager.default_keymap();
    keymap.secondary_mode_bindings["5k"]["lane3"] = "Space";
    KeymapSettingsController controller;
    controller.reset(5, "none");
    static_cast<void>(controller.select(3, true));
    static_cast<void>(controller.handle(MenuAction::activate(), 100));
    const auto view = KeymapSettingsView::build(controller, keymap, 5, "none", "Input: RawInput", 200,
                                               tenriff::ui::Language::English);
    REQUIRE(view.rows.size() == 9);
    CHECK(view.rows[3].label == "Key 3");
    CHECK(view.rows[3].value == "K");
    CHECK(view.rows[3].secondary_value.find("Space [waiting]") != std::string::npos);
    const auto nkro = KeymapSettingsView::build_nkro_test(controller, keymap,
        {tenriff::config::KeycodeMap::to_keycode("Space").value()}, "Input: Polling", tenriff::ui::Language::English);
    CHECK(nkro.rows[2].selected);
    CHECK_FALSE(nkro.rows[1].selected);
}
