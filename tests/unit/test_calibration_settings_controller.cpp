#include "doctest/doctest.h"

#include <cstdint>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/CalibrationSettingsController.h"
#include "app/menu/settings/CalibrationSettingsView.h"

namespace {

using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::CalibrationSettingId;
using tenriff::app::menu::settings::CalibrationSettingsController;
using tenriff::app::menu::settings::CalibrationSettingsView;

}  // namespace

TEST_CASE("calibration setting identifiers and ranges are stable") {
    using tenriff::app::menu::settings::calibration_setting_id_at;
    using tenriff::app::menu::settings::calibration_setting_index;
    using tenriff::app::menu::settings::calibration_setting_numeric_range;
    using tenriff::app::menu::settings::kCalibrationSettingOrder;

    static_assert(static_cast<std::uint8_t>(CalibrationSettingId::AdjustmentStep) == 0);
    static_assert(static_cast<std::uint8_t>(CalibrationSettingId::Back) == 5);
    for (std::size_t index = 0; index < kCalibrationSettingOrder.size(); ++index) {
        REQUIRE(calibration_setting_id_at(index).has_value());
        CHECK(*calibration_setting_id_at(index) == kCalibrationSettingOrder[index]);
        REQUIRE(calibration_setting_index(kCalibrationSettingOrder[index]).has_value());
        CHECK(*calibration_setting_index(kCalibrationSettingOrder[index]) == index);
    }
    CHECK_FALSE(calibration_setting_id_at(kCalibrationSettingOrder.size()).has_value());

    const auto input = calibration_setting_numeric_range(CalibrationSettingId::InputOffset);
    REQUIRE(input.has_value());
    CHECK(input->minimum == doctest::Approx(-250.0));
    CHECK(input->maximum == doctest::Approx(250.0));
    CHECK_FALSE(calibration_setting_numeric_range(CalibrationSettingId::ResetOffsets).has_value());
}

TEST_CASE("calibration adjustment step cycles and survives screen visits") {
    tenriff::config::RuntimeConfig runtime;
    CalibrationSettingsController controller;

    CHECK(controller.adjustment_step_ms() == 1);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(controller.adjustment_step_ms() == 5);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime));
    CHECK(controller.adjustment_step_ms() == 10);
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime));
    CHECK(controller.adjustment_step_ms() == 5);
    controller.reset(CalibrationSettingId::Back);
    CHECK(controller.adjustment_step_ms() == 5);
    CHECK(controller.selected_id() == CalibrationSettingId::Back);
}

TEST_CASE("calibration offsets use the selected step and clamp at their bounds") {
    tenriff::config::RuntimeConfig runtime;
    CalibrationSettingsController controller;

    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime));
    REQUIRE(controller.adjustment_step_ms() == 5);
    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, CalibrationSettingId::InputOffset));
    CHECK(runtime.input_offset_ms == doctest::Approx(5.0));
    static_cast<void>(controller.handle(
        MenuAction::adjust(-1), runtime, CalibrationSettingId::VisualOffset));
    CHECK(runtime.visual_offset_ms == doctest::Approx(-5.0));
    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, CalibrationSettingId::SoundOffset));
    CHECK(runtime.sound_offset_ms == doctest::Approx(5.0));

    runtime.input_offset_ms = 250.0;
    const auto no_op = controller.handle(
        MenuAction::adjust(1), runtime, CalibrationSettingId::InputOffset);
    CHECK(no_op.render_changed);
    CHECK_FALSE(no_op.persist_config);
    CHECK(runtime.input_offset_ms == doctest::Approx(250.0));
}

TEST_CASE("calibration changed adjustments persist immediately") {
    tenriff::config::RuntimeConfig runtime;
    CalibrationSettingsController controller;

    const auto changed = controller.handle(
        MenuAction::adjust(1), runtime, CalibrationSettingId::InputOffset);
    CHECK(changed.render_changed);
    CHECK(changed.persist_config);
    CHECK_FALSE(changed.navigate_back);

    const auto clean_back = controller.handle(MenuAction::back(), runtime);
    CHECK(clean_back.render_changed);
    CHECK(clean_back.navigate_back);
    CHECK_FALSE(clean_back.persist_config);
}

TEST_CASE("calibration reset clears all offsets and persists only when needed") {
    tenriff::config::RuntimeConfig runtime;
    CalibrationSettingsController controller;
    runtime.input_offset_ms = 10.0;
    runtime.visual_offset_ms = -20.0;
    runtime.sound_offset_ms = 30.0;

    const auto reset = controller.handle(
        MenuAction::activate(), runtime, CalibrationSettingId::ResetOffsets);
    CHECK(reset.render_changed);
    CHECK(reset.persist_config);
    CHECK(runtime.input_offset_ms == doctest::Approx(0.0));
    CHECK(runtime.visual_offset_ms == doctest::Approx(0.0));
    CHECK(runtime.sound_offset_ms == doctest::Approx(0.0));

    CHECK(controller.handle(
        MenuAction::activate(), runtime, CalibrationSettingId::ResetOffsets).empty());
}

TEST_CASE("calibration pointer target and keyboard adjustment have parity") {
    tenriff::config::RuntimeConfig keyboard_runtime;
    tenriff::config::RuntimeConfig pointer_runtime;
    CalibrationSettingsController keyboard;
    CalibrationSettingsController pointer;

    static_cast<void>(keyboard.select(CalibrationSettingId::SoundOffset));
    static_cast<void>(keyboard.handle(MenuAction::adjust(-1), keyboard_runtime));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(-1), pointer_runtime, CalibrationSettingId::SoundOffset));
    CHECK(keyboard_runtime.sound_offset_ms ==
          doctest::Approx(pointer_runtime.sound_offset_ms));
}

TEST_CASE("calibration settings view preserves row values and localization") {
    tenriff::config::RuntimeConfig runtime;
    CalibrationSettingsController controller;
    static_cast<void>(controller.select(CalibrationSettingId::VisualOffset));
    runtime.input_offset_ms = 2.0;
    runtime.visual_offset_ms = -3.0;
    runtime.sound_offset_ms = 4.0;

    const auto english = CalibrationSettingsView::build(controller, runtime, false);
    REQUIRE(english.rows.size() == 6);
    REQUIRE(english.notes.size() == 5);
    CHECK(english.rows[0].value == "1 ms");
    CHECK(english.rows[1].value == "+2.0 ms");
    CHECK(english.rows[2].value == "-3.0 ms");
    CHECK(english.rows[3].value == "+4.0 ms");
    CHECK(english.rows[2].selected);
    CHECK(english.rows[4].activatable);
    CHECK(english.rows[5].activatable);

    const auto korean = CalibrationSettingsView::build(controller, runtime, true);
    CHECK(korean.rows[0].label == "조정 단위");
    CHECK(korean.rows[1].label == "입력 오프셋");
    CHECK(korean.rows[2].label == "비주얼 레이턴시");
    CHECK(korean.rows[5].label == "뒤로");
}
