#include "doctest/doctest.h"

#include <array>
#include <cstdint>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/InputSettingsController.h"
#include "app/menu/settings/InputSettingsView.h"

namespace {

using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::InputSettingId;
using tenriff::app::menu::settings::InputSettingsController;
using tenriff::app::menu::settings::InputSettingsView;

}  // namespace

TEST_CASE("input setting identifiers preserve renderer hit order") {
    using tenriff::app::menu::settings::input_setting_id_at;
    using tenriff::app::menu::settings::input_setting_index;
    using tenriff::app::menu::settings::kInputSettingOrder;

    static_assert(static_cast<std::uint8_t>(InputSettingId::Backend) == 0);
    static_assert(static_cast<std::uint8_t>(InputSettingId::Back) == 3);
    for (std::size_t index = 0; index < kInputSettingOrder.size(); ++index) {
        REQUIRE(input_setting_id_at(index).has_value());
        CHECK(*input_setting_id_at(index) == kInputSettingOrder[index]);
        REQUIRE(input_setting_index(kInputSettingOrder[index]).has_value());
        CHECK(*input_setting_index(kInputSettingOrder[index]) == index);
    }
    CHECK_FALSE(input_setting_id_at(kInputSettingOrder.size()).has_value());
    CHECK_FALSE(input_setting_index(static_cast<InputSettingId>(255)).has_value());
}

TEST_CASE("input settings cursor movement clamps and rejects invalid pointer ids") {
    tenriff::config::RuntimeConfig runtime;
    InputSettingsController controller;

    CHECK(controller.handle(MenuAction::move(-1), runtime, false).empty());
    CHECK(controller.selected_id() == InputSettingId::Backend);
    CHECK(controller.handle(MenuAction::move(1), runtime, false).render_changed);
    CHECK(controller.selected_id() == InputSettingId::PollingHz);
    static_cast<void>(controller.handle(MenuAction::move(8), runtime, false));
    CHECK(controller.selected_id() == InputSettingId::Debounce);
    CHECK(controller.handle(
        MenuAction::activate(), runtime, false, static_cast<InputSettingId>(250)).empty());
    CHECK(controller.selected_id() == InputSettingId::Debounce);
}

TEST_CASE("input backend selection and fallback retry request a backend reinitialization") {
    tenriff::config::RuntimeConfig runtime;
    InputSettingsController controller;

    runtime.input.rawinput = true;
    runtime.input.backend = "rawinput";
    const auto polling = controller.handle(MenuAction::adjust(-1), runtime, false);
    CHECK(polling.render_changed);
    CHECK_FALSE(runtime.input.rawinput);
    CHECK(runtime.input.backend == "polling");
    CHECK(controller.dirty());
    CHECK(controller.backend_dirty());

    controller.reset();
    runtime.input.rawinput = true;
    runtime.input.backend = "rawinput";
    const auto retry = controller.handle(MenuAction::adjust(1), runtime, true);
    CHECK(retry.render_changed);
    CHECK(runtime.input.rawinput);
    CHECK(controller.dirty());
    CHECK(controller.backend_dirty());

    controller.reset();
    CHECK(controller.handle(MenuAction::adjust(1), runtime, false).empty());
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("input polling and debounce options cycle in both directions") {
    tenriff::config::RuntimeConfig runtime;
    InputSettingsController controller;

    static_cast<void>(controller.select(InputSettingId::PollingHz));
    runtime.input.polling_hz = 1000;
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, false));
    CHECK(runtime.input.polling_hz == 8000);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, false));
    CHECK(runtime.input.polling_hz == 1000);

    static_cast<void>(controller.select(InputSettingId::Debounce));
    runtime.input.debounce_ms = 0.0;
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, false));
    CHECK(runtime.input.debounce_ms == doctest::Approx(12.0));
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, false));
    CHECK(runtime.input.debounce_ms == doctest::Approx(0.0));
}

TEST_CASE("input pointer target and keyboard adjustment share one mutation path") {
    tenriff::config::RuntimeConfig keyboard_runtime;
    tenriff::config::RuntimeConfig pointer_runtime;
    InputSettingsController keyboard;
    InputSettingsController pointer;

    static_cast<void>(keyboard.select(InputSettingId::PollingHz));
    static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime, false));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(1), pointer_runtime, false, InputSettingId::PollingHz));
    CHECK(keyboard_runtime.input.polling_hz == pointer_runtime.input.polling_hz);

    static_cast<void>(keyboard.select(InputSettingId::Debounce));
    static_cast<void>(keyboard.handle(MenuAction::adjust(-1), keyboard_runtime, false));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(-1), pointer_runtime, false, InputSettingId::Debounce));
    CHECK(keyboard_runtime.input.debounce_ms ==
          doctest::Approx(pointer_runtime.input.debounce_ms));
}

TEST_CASE("input Back requests persistence and one restart only for dirty visits") {
    tenriff::config::RuntimeConfig runtime;
    InputSettingsController controller;

    const auto clean = controller.handle(MenuAction::back(), runtime, false);
    CHECK(clean.render_changed);
    CHECK(clean.navigate_back);
    CHECK_FALSE(clean.persist_config);
    CHECK_FALSE(clean.restart_input);

    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, false, InputSettingId::PollingHz));
    const auto timing_dirty = controller.handle(MenuAction::back(), runtime, false);
    CHECK(timing_dirty.persist_config);
    CHECK(timing_dirty.restart_input);
    CHECK_FALSE(timing_dirty.reinitialize_input_backend);

    static_cast<void>(controller.handle(
        MenuAction::adjust(-1), runtime, false, InputSettingId::Backend));
    const auto backend_dirty = controller.handle(MenuAction::back(), runtime, false);
    CHECK(backend_dirty.persist_config);
    CHECK(backend_dirty.restart_input);
    CHECK(backend_dirty.reinitialize_input_backend);

    const auto second = controller.handle(MenuAction::back(), runtime, false);
    CHECK_FALSE(second.persist_config);
    CHECK_FALSE(second.restart_input);
}

TEST_CASE("input settings view preserves values fallback detail and localization") {
    tenriff::config::RuntimeConfig runtime;
    InputSettingsController controller;
    static_cast<void>(controller.select(InputSettingId::Debounce));

    const auto english = InputSettingsView::build(controller, runtime, true, false);
    REQUIRE(english.rows.size() == 4);
    REQUIRE(english.notes.size() == 6);
    CHECK(english.rows[0].value == "RawInput (active: Polling)");
    CHECK(english.rows[1].value == "1000");
    CHECK(english.rows[2].value == "8 ms");
    CHECK(english.rows[2].selected);
    CHECK(english.rows[3].activatable);

    const auto korean = InputSettingsView::build(controller, runtime, false, true);
    CHECK(korean.rows[0].label == "입력 백엔드");
    CHECK(korean.rows[1].label == "폴링 Hz");
    CHECK(korean.rows[2].label == "디바운스");
    CHECK(korean.rows[3].label == "뒤로");
}
