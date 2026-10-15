#include "doctest/doctest.h"

#include <array>
#include <cstdint>
#include <limits>

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

TEST_CASE("input polling cycles while debounce advances one millisecond within zero to twenty") {
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
    controller.reset(InputSettingId::Debounce);
    CHECK(controller.handle(MenuAction::adjust(-1), runtime, false).empty());
    CHECK_FALSE(controller.dirty());
    for (int value = 1; value <= 20; ++value) {
        CHECK(controller.handle(MenuAction::adjust(1), runtime, false).render_changed);
        CHECK(runtime.input.debounce_ms == doctest::Approx(value));
    }
    controller.reset(InputSettingId::Debounce);
    CHECK(controller.handle(MenuAction::adjust(1), runtime, false).empty());
    CHECK_FALSE(controller.dirty());
    for (int value = 19; value >= 0; --value) {
        CHECK(controller.handle(MenuAction::adjust(-1), runtime, false).render_changed);
        CHECK(runtime.input.debounce_ms == doctest::Approx(value));
    }
    CHECK(runtime.input.debounce_ms == doctest::Approx(0.0));
    CHECK_FALSE(controller.backend_dirty());
    const auto leave = controller.handle(MenuAction::back(), runtime, false);
    CHECK(leave.persist_config);
    CHECK(leave.restart_input);
    CHECK_FALSE(leave.reinitialize_input_backend);
}

TEST_CASE("input debounce keeps saved fractions until adjustment and handles invalid runtime values") {
    tenriff::config::RuntimeConfig runtime;
    InputSettingsController controller;
    controller.reset(InputSettingId::Debounce);
    runtime.input.debounce_ms = 2.5;
    CHECK(controller.handle(MenuAction::activate(), runtime, false).empty());
    CHECK(runtime.input.debounce_ms == doctest::Approx(2.5));
    CHECK(controller.handle(MenuAction::adjust(1), runtime, false).render_changed);
    CHECK(runtime.input.debounce_ms == doctest::Approx(4.0));
    CHECK(controller.handle(MenuAction::adjust(1), runtime, false).render_changed);
    CHECK(runtime.input.debounce_ms == doctest::Approx(5.0));
    runtime.input.debounce_ms = std::numeric_limits<double>::quiet_NaN();
    CHECK(controller.handle(MenuAction::adjust(1), runtime, false).render_changed);
    CHECK(runtime.input.debounce_ms == doctest::Approx(1.0));
    runtime.input.debounce_ms = std::numeric_limits<double>::infinity();
    CHECK(controller.handle(MenuAction::adjust(-1), runtime, false).render_changed);
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
    static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime, false));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(1), pointer_runtime, false, InputSettingId::Debounce));
    CHECK(keyboard_runtime.input.debounce_ms ==
          doctest::Approx(pointer_runtime.input.debounce_ms));
    CHECK(keyboard_runtime.input.debounce_ms == doctest::Approx(1.0));
    static_cast<void>(keyboard.handle(MenuAction::adjust(-1), keyboard_runtime, false));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(-1), pointer_runtime, false, InputSettingId::Debounce));
    CHECK(keyboard_runtime.input.debounce_ms == doctest::Approx(0.0));
    CHECK(pointer_runtime.input.debounce_ms == doctest::Approx(0.0));
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

    const auto english = InputSettingsView::build(controller, runtime, true, tenriff::ui::Language::English);
    REQUIRE(english.rows.size() == 4);
    REQUIRE(english.notes.size() == 6);
    CHECK(english.rows[0].value == "RawInput (active: Polling)");
    CHECK(english.rows[1].value == "1000");
    CHECK(english.rows[2].value == "0 ms");
    CHECK(english.rows[2].kind == tenriff::app::menu::settings::SettingsRowKind::Numeric);
    REQUIRE(english.rows[2].numeric_range.has_value());
    CHECK(english.rows[2].numeric_range->minimum == doctest::Approx(0.0));
    CHECK(english.rows[2].numeric_range->maximum == doctest::Approx(20.0));
    CHECK(english.rows[2].numeric_range->step == doctest::Approx(1.0));
    CHECK(english.notes[4].find("0-20 ms in 1 ms steps") != std::string::npos);
    CHECK(english.notes[4].find("keyboard") != std::string::npos);
    CHECK(english.notes[4].find("Release stays immediate") != std::string::npos);
    CHECK(english.rows[2].selected);
    CHECK(english.rows[3].activatable);

    const auto korean = InputSettingsView::build(controller, runtime, false, tenriff::ui::Language::Korean);
    CHECK(korean.rows[0].label == "입력 백엔드");
    CHECK(korean.rows[1].label == "폴링 Hz");
    CHECK(korean.rows[2].label == "디바운스");
    CHECK(korean.rows[3].label == "뒤로");

    runtime.input.debounce_ms = 8.0;
    const auto saved = InputSettingsView::build(controller, runtime, false, tenriff::ui::Language::English);
    CHECK(saved.rows[2].value == "8 ms");
    CHECK(runtime.input.debounce_ms == doctest::Approx(8.0));
}
