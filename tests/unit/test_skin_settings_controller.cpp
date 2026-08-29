#include "doctest/doctest.h"

#include <string>
#include <vector>

#include "app/menu/MenuAction.h"
#include "app/menu/settings/SkinSettingsController.h"

namespace {

using tenriff::app::SkinSettingsRowId;
using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::SkinBoundaryAction;
using tenriff::app::menu::settings::SkinSettingsController;

const std::vector<std::string> kLr2Names{"LR2 A", "LR2 B"};
const std::vector<std::string> kTenRiffNames{"Native A", "Native B"};

}  // namespace

TEST_CASE("skin stable identifiers map across the optional LR2 row") {
    using tenriff::app::menu::settings::skin_setting_id_at;

    REQUIRE(skin_setting_id_at(4, false).has_value());
    CHECK(*skin_setting_id_at(4, false) == SkinSettingsRowId::ImportSkin);
    REQUIRE(skin_setting_id_at(4, true).has_value());
    CHECK(*skin_setting_id_at(4, true) == SkinSettingsRowId::Lr2Resolution);
    REQUIRE(skin_setting_id_at(44, false).has_value());
    CHECK(*skin_setting_id_at(44, false) == SkinSettingsRowId::Back);
    REQUIRE(skin_setting_id_at(45, true).has_value());
    CHECK(*skin_setting_id_at(45, true) == SkinSettingsRowId::Back);
    CHECK_FALSE(skin_setting_id_at(45, false).has_value());
}

TEST_CASE("skin controller owns edit mode lane and gap selection") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("7k");
    CHECK(controller.edit_mode() == "7k");
    CHECK(controller.edit_lane() == 0);

    auto effects = controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::KeyMode);
    CHECK(controller.edit_mode() == "7+1");
    CHECK(effects.menu.render_changed);
    CHECK(controller.dirty());

    effects = controller.handle(
        MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::TargetLane);
    CHECK(controller.edit_lane() == 7);
    CHECK_FALSE(effects.menu.persist_config);
    effects = controller.handle(
        MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::TargetGap);
    CHECK(controller.edit_gap() == 6);
}

TEST_CASE("skin source changes request refreshes and preserve typed row identity") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    runtime.skin.source = "native";

    const auto effects = controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::SkinSource);
    CHECK(runtime.skin.source == "tenriff");
    CHECK(controller.selected_id() == SkinSettingsRowId::SkinSource);
    CHECK(effects.refresh_lr2_skins);
    CHECK(effects.refresh_tenriff_skins);
    CHECK(effects.increment_skin_revision);
}

TEST_CASE("skin imported name cycles within the active source") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    runtime.skin.source = "tenriff";
    runtime.skin.tenriff_skin_name = "Native A";

    const auto effects = controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::ImportedSkin);
    CHECK(runtime.skin.tenriff_skin_name == "Native B");
    CHECK(effects.refresh_tenriff_skins);
    CHECK(effects.increment_skin_revision);
    CHECK(controller.dirty());
}

TEST_CASE("skin appearance rows mutate through one typed action path") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");

    const bool old_border = runtime.skin.note_border_enabled;
    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::NoteBorder));
    CHECK(runtime.skin.note_border_enabled != old_border);

    const double old_opacity = runtime.skin.visual_opacity;
    static_cast<void>(controller.handle(
        MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::VisualOpacity));
    CHECK(runtime.skin.visual_opacity < old_opacity);

    const double old_offset = runtime.visual_offset_ms;
    static_cast<void>(controller.handle(
        MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::VisualLatency));
    CHECK(runtime.visual_offset_ms == doctest::Approx(old_offset + 1.0));
}

TEST_CASE("skin unavailable adjustments are no-ops") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");

    auto effects = controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::ScratchPosition);
    CHECK(effects.menu.render_changed);
    CHECK_FALSE(controller.dirty());

    effects = controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::CenterGap);
    CHECK(effects.menu.render_changed);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("skin platform actions remain explicit boundary requests") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");

    auto effects = controller.handle(
        MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::ImportSkin);
    CHECK(effects.boundary_action == SkinBoundaryAction::ImportSkin);
    CHECK_FALSE(controller.dirty());
    effects = controller.handle(
        MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::CreateSkin);
    CHECK(effects.boundary_action == SkinBoundaryAction::CreateSkin);
    effects = controller.request_reload();
    CHECK(effects.boundary_action == SkinBoundaryAction::ReloadSkin);
}

TEST_CASE("skin back persists once only after config mutation") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");

    auto effects = controller.handle(
        MenuAction::back(), runtime, kLr2Names, kTenRiffNames);
    CHECK(effects.menu.navigate_back);
    CHECK_FALSE(effects.menu.persist_config);

    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::GameplayCursor));
    effects = controller.handle(
        MenuAction::back(), runtime, kLr2Names, kTenRiffNames);
    CHECK(effects.menu.navigate_back);
    CHECK(effects.menu.persist_config);
    CHECK_FALSE(controller.dirty());
    CHECK(controller.selected_id() == SkinSettingsRowId::KeyMode);
}
