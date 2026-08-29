#include "doctest/doctest.h"

#include <cstdint>
#include <string>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/GraphicsSettingsController.h"
#include "app/menu/settings/GraphicsSettingsView.h"

namespace {

using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::GraphicsSettingId;
using tenriff::app::menu::settings::GraphicsSettingsController;
using tenriff::app::menu::settings::GraphicsSettingsView;
using tenriff::app::menu::settings::OnnxUpscalerConfirmId;

}  // namespace

TEST_CASE("graphics setting and ONNX confirmation identifiers are stable") {
    using tenriff::app::menu::settings::graphics_setting_id_at;
    using tenriff::app::menu::settings::graphics_setting_index;
    using tenriff::app::menu::settings::kGraphicsSettingOrder;
    using tenriff::app::menu::settings::onnx_upscaler_confirm_id_at;

    static_assert(static_cast<std::uint8_t>(GraphicsSettingId::Display) == 0);
    static_assert(static_cast<std::uint8_t>(GraphicsSettingId::Back) == 11);
    static_assert(static_cast<std::uint8_t>(OnnxUpscalerConfirmId::Enable) == 0);
    static_assert(static_cast<std::uint8_t>(OnnxUpscalerConfirmId::KeepNative) == 1);
    for (std::size_t index = 0; index < kGraphicsSettingOrder.size(); ++index) {
        REQUIRE(graphics_setting_id_at(index).has_value());
        CHECK(*graphics_setting_id_at(index) == kGraphicsSettingOrder[index]);
        REQUIRE(graphics_setting_index(kGraphicsSettingOrder[index]).has_value());
        CHECK(*graphics_setting_index(kGraphicsSettingOrder[index]) == index);
    }
    CHECK_FALSE(graphics_setting_id_at(kGraphicsSettingOrder.size()).has_value());
    REQUIRE(onnx_upscaler_confirm_id_at(1).has_value());
    CHECK(*onnx_upscaler_confirm_id_at(1) == OnnxUpscalerConfirmId::KeepNative);
    CHECK_FALSE(onnx_upscaler_confirm_id_at(2).has_value());
}

TEST_CASE("graphics live rows cycle and request immediate runtime apply") {
    tenriff::config::RuntimeConfig runtime;
    GraphicsSettingsController controller;

    auto effects = controller.handle(MenuAction::adjust(1), runtime);
    CHECK(runtime.graphics.display_mode == "windowed");
    CHECK(effects.menu.render_changed);
    CHECK(effects.apply_runtime_graphics);

    effects = controller.handle(
        MenuAction::adjust(-1), runtime, GraphicsSettingId::Resolution);
    CHECK(runtime.graphics.resolution == "qhd");
    CHECK(effects.apply_runtime_graphics);

    runtime.graphics.refresh_hz = -1;
    effects = controller.handle(
        MenuAction::adjust(1), runtime, GraphicsSettingId::RefreshHz);
    CHECK(runtime.graphics.refresh_hz == 0);
    CHECK(effects.apply_runtime_graphics);

    const bool original_vsync = runtime.graphics.vsync;
    effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::VSync);
    CHECK(runtime.graphics.vsync != original_vsync);
    CHECK(effects.apply_runtime_graphics);
    CHECK(controller.dirty());
}

TEST_CASE("graphics deferred rows mutate config without live apply") {
    tenriff::config::RuntimeConfig runtime;
    GraphicsSettingsController controller;

    auto effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::PerformanceHud);
    CHECK(runtime.graphics.performance_overlay);
    CHECK_FALSE(effects.apply_runtime_graphics);

    effects = controller.handle(MenuAction::activate(), runtime, GraphicsSettingId::Bga);
    CHECK_FALSE(runtime.graphics.bga_enabled);
    effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::BgaBehindNotes);
    CHECK_FALSE(runtime.skin.black_playfield_enabled);
    effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::PreferLowPowerDirectX);
    CHECK(runtime.graphics.background_upscale_prefer_npu);
    effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::Language);
    CHECK(runtime.ui.language == "ko");
    CHECK(controller.dirty());
}

TEST_CASE("graphics back saves and reapplies once only when dirty") {
    tenriff::config::RuntimeConfig runtime;
    GraphicsSettingsController controller;

    const auto clean = controller.handle(MenuAction::back(), runtime);
    CHECK(clean.menu.navigate_back);
    CHECK_FALSE(clean.menu.persist_config);
    CHECK_FALSE(clean.apply_runtime_graphics);

    static_cast<void>(controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::PerformanceHud));
    const auto dirty = controller.handle(MenuAction::back(), runtime);
    CHECK(dirty.menu.navigate_back);
    CHECK(dirty.menu.persist_config);
    CHECK(dirty.apply_runtime_graphics);
    CHECK_FALSE(controller.dirty());
    CHECK(controller.selected_id() == GraphicsSettingId::Display);
}

TEST_CASE("graphics ONNX flow keeps file selection and confirmation as boundary effects") {
    tenriff::config::RuntimeConfig runtime;
    GraphicsSettingsController controller;

    auto effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::BgaUpscaler);
    CHECK(effects.choose_onnx_model);
    CHECK(effects.show_onnx_confirmation);
    CHECK_FALSE(controller.dirty());

    effects = controller.set_onnx_model_path(runtime, "models/upscale.onnx");
    CHECK(effects.menu.render_changed);
    CHECK(controller.dirty());
    CHECK(runtime.graphics.background_upscale_model_path == "models/upscale.onnx");

    effects = controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::BgaUpscaler);
    CHECK_FALSE(effects.choose_onnx_model);
    CHECK(effects.show_onnx_confirmation);
    CHECK(controller.selected_confirmation_id() == OnnxUpscalerConfirmId::KeepNative);
}

TEST_CASE("ONNX confirmation enables only on Yes and returns to the upscaler row") {
    tenriff::config::RuntimeConfig runtime;
    GraphicsSettingsController controller;
    runtime.graphics.background_upscale_model_path = "upscale.onnx";
    static_cast<void>(controller.handle(
        MenuAction::activate(), runtime, GraphicsSettingId::BgaUpscaler));

    static_cast<void>(controller.select_confirmation(OnnxUpscalerConfirmId::Enable));
    auto effects = controller.handle_confirmation(MenuAction::activate(), runtime);
    CHECK(runtime.graphics.background_upscale_mode == "onnx");
    CHECK(effects.menu.navigate_back);
    CHECK(controller.dirty());
    CHECK(controller.selected_id() == GraphicsSettingId::BgaUpscaler);
    CHECK(controller.selected_confirmation_id() == OnnxUpscalerConfirmId::KeepNative);

    runtime.graphics.background_upscale_mode = "off";
    controller.prepare_onnx_confirmation();
    effects = controller.handle_confirmation(MenuAction::activate(), runtime);
    CHECK(runtime.graphics.background_upscale_mode == "off");
    CHECK(effects.menu.navigate_back);

    controller.prepare_onnx_confirmation();
    effects = controller.handle_confirmation(MenuAction::move(1), runtime);
    CHECK(effects.menu.render_changed);
    CHECK(controller.selected_confirmation_id() == OnnxUpscalerConfirmId::Enable);
    effects = controller.handle_confirmation(MenuAction::back(), runtime);
    CHECK(runtime.graphics.background_upscale_mode == "off");
    CHECK(effects.menu.navigate_back);
}

TEST_CASE("graphics keyboard and pointer adjustments have parity") {
    tenriff::config::RuntimeConfig keyboard_runtime;
    tenriff::config::RuntimeConfig pointer_runtime;
    GraphicsSettingsController keyboard;
    GraphicsSettingsController pointer;

    static_cast<void>(keyboard.select(GraphicsSettingId::Resolution));
    static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(1), pointer_runtime, GraphicsSettingId::Resolution));
    CHECK(keyboard_runtime.graphics.resolution == pointer_runtime.graphics.resolution);
}

TEST_CASE("graphics settings views preserve values localization and safe defaults") {
    tenriff::config::RuntimeConfig runtime;
    GraphicsSettingsController controller;
    runtime.graphics.refresh_hz = 0;
    runtime.graphics.background_upscale_model_path = "folder/model.onnx";
    runtime.ui.language = "ko";
    static_cast<void>(controller.select(GraphicsSettingId::OnnxModel));

    const auto korean = GraphicsSettingsView::build(controller, runtime, true);
    REQUIRE(korean.rows.size() == 12);
    REQUIRE(korean.notes.size() == 11);
    CHECK(korean.rows[2].value == "무제한 (최대 1500 FPS)");
    CHECK(korean.rows[8].value == "model.onnx");
    CHECK(korean.rows[8].selected);
    CHECK(korean.rows[10].value == "한국어");
    CHECK(korean.rows[11].label == "뒤로");

    controller.prepare_onnx_confirmation();
    const auto confirmation =
        GraphicsSettingsView::build_onnx_confirmation(controller, false);
    REQUIRE(confirmation.rows.size() == 2);
    REQUIRE(confirmation.notes.size() == 3);
    CHECK_FALSE(confirmation.rows[0].selected);
    CHECK(confirmation.rows[1].selected);
    CHECK(confirmation.rows[0].label == "Yes, enable ONNX");
}
