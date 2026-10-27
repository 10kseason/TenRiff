#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "app/LanePresentationLayout.h"
#include "app/menu/MenuAction.h"
#include "app/MenuAppSkinUtils.h"
#include "app/menu/settings/SkinSettingsController.h"

namespace {

using tenriff::app::SkinSettingsRowId;
using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::SkinBoundaryAction;
using tenriff::app::menu::settings::SkinSettingsController;

const std::vector<std::string> kLr2Names{"LR2 A", "LR2 B"};
const std::vector<std::string> kTenRiffNames{"Native A", "Native B"};

}  // namespace

TEST_CASE("skin percentage controls change by one percent and sliders retain the percent grid") {
    using Skin = tenriff::config::SkinConfig;
    const std::pair<SkinSettingsRowId, double Skin::*> scalars[] = {
        {SkinSettingsRowId::KeyOverlayX, &Skin::key_overlay_x},
        {SkinSettingsRowId::KeyOverlayY, &Skin::key_overlay_y},
        {SkinSettingsRowId::NoteFadeIn, &Skin::note_fade_in},
        {SkinSettingsRowId::NoteFadeOut, &Skin::note_fade_out},
        {SkinSettingsRowId::KeyPulse, &Skin::key_pulse_brightness},
        {SkinSettingsRowId::KeyBackdropOpacity, &Skin::key_backdrop_opacity},
        {SkinSettingsRowId::KeyBackdropBrightness, &Skin::key_backdrop_brightness},
        {SkinSettingsRowId::KeyBackdropHeight, &Skin::key_backdrop_height},
        {SkinSettingsRowId::JudgeLinePosition, &Skin::judgement_line_position},
        {SkinSettingsRowId::ComboY, &Skin::combo_position},
        {SkinSettingsRowId::JudgementY, &Skin::judgement_position},
        {SkinSettingsRowId::ComboFontSize, &Skin::combo_font_scale},
        {SkinSettingsRowId::JudgementFontSize, &Skin::judgement_font_scale},
        {SkinSettingsRowId::TimingTextSize, &Skin::timing_text_scale},
        {SkinSettingsRowId::TimingBarSize, &Skin::timing_bar_scale},
    };
    for (const auto& [id, member] : scalars) {
        tenriff::config::RuntimeConfig runtime;
        runtime.skin.*member = 0.75;
        runtime.skin.key_backdrop_override = true;
        SkinSettingsController controller;
        controller.reset("4k");
        const auto before = tenriff::config::serialize_skin_config(runtime.skin);
        static_cast<void>(controller.select(id, false));
        CHECK(tenriff::config::serialize_skin_config(runtime.skin) == before);
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, id));
        CHECK(runtime.skin.*member == doctest::Approx(0.76));
        static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, id));
        CHECK(runtime.skin.*member == doctest::Approx(0.75));
    }
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("4k");
    static_cast<void>(controller.handle(MenuAction::set_ratio(0.513), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::TimingTextSize));
    CHECK(runtime.skin.timing_text_scale == doctest::Approx(1.27));
    static_cast<void>(controller.handle(MenuAction::set_ratio(0.413), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::KeyBackdropBrightness));
    CHECK(runtime.skin.key_backdrop_brightness == doctest::Approx(0.83));
}

TEST_CASE("skin stable identifiers map across the optional LR2 row") {
    using tenriff::app::menu::settings::skin_setting_id_at;

    CHECK(skin_setting_id_at(2, false) == SkinSettingsRowId::VisualLatency);
    CHECK(skin_setting_id_at(3, true) == SkinSettingsRowId::VisualLatency);
    CHECK(skin_setting_id_at(3, false) == SkinSettingsRowId::ImportSkin);
    CHECK(skin_setting_id_at(4, true) == SkinSettingsRowId::Lr2Resolution);
    CHECK(tenriff::app::skin_settings_category(SkinSettingsRowId::VisualLatency) ==
          tenriff::app::SkinSettingsCategory::Source);
    const auto native_back = tenriff::app::menu::settings::skin_setting_index(SkinSettingsRowId::Back, false);
    const auto lr2_back = tenriff::app::menu::settings::skin_setting_index(SkinSettingsRowId::Back, true);
    REQUIRE(native_back.has_value());
    REQUIRE(lr2_back.has_value());
    CHECK(*skin_setting_id_at(*native_back, false) == SkinSettingsRowId::Back);
    CHECK(*skin_setting_id_at(*lr2_back, true) == SkinSettingsRowId::Back);
    CHECK_FALSE(skin_setting_id_at(*native_back + 1, false).has_value());
    CHECK_FALSE(tenriff::app::menu::settings::skin_setting_index(SkinSettingsRowId::Lr2Resolution, false).has_value());
}

TEST_CASE("skin divider brightness uses one percent steps without changing visibility or geometry") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    runtime.skin.show_lane_dividers = false;
    runtime.skin.lane_divider_width_scale = 1.25;
    CHECK(runtime.skin.lane_divider_brightness == 1.0);
    (void)controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::LaneDividerBrightness);
    CHECK(runtime.skin.lane_divider_brightness == doctest::Approx(0.99));
    (void)controller.handle(MenuAction::set_ratio(0.374), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::LaneDividerBrightness);
    CHECK(runtime.skin.lane_divider_brightness == doctest::Approx(0.37));
    (void)controller.handle(MenuAction::set_ratio(-1), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::LaneDividerBrightness);
    CHECK(runtime.skin.lane_divider_brightness == 0.0);
    (void)controller.handle(MenuAction::set_ratio(2), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::LaneDividerBrightness);
    CHECK(runtime.skin.lane_divider_brightness == 1.0);
    CHECK_FALSE(runtime.skin.show_lane_dividers);
    CHECK(runtime.skin.lane_divider_width_scale == 1.25);
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
}

TEST_CASE("skin direct lane selection preserves the active editor row and config while rejecting absent lanes") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("16k");
    static_cast<void>(controller.select(SkinSettingsRowId::LaneWidth, false));
    for (int lane = 1; lane <= 16; ++lane) {
        const auto effects = controller.select_lane(lane);
        CHECK(controller.edit_lane() == lane - 1);
        CHECK(controller.selected_id() == SkinSettingsRowId::LaneWidth);
        CHECK_FALSE(controller.dirty());
        CHECK_FALSE(effects.menu.persist_config);
        CHECK_FALSE(effects.increment_skin_revision);
    }
    CHECK(controller.select_lane(0).empty());
    CHECK(controller.select_lane(17).empty());
    CHECK(controller.edit_lane() == 15);
    controller.reset("4k");
    CHECK(controller.select_lane(5).empty());
    CHECK(controller.select_lane(10).empty());
    CHECK(controller.edit_lane() == 0);
    static_cast<void>(controller.select_lane(4));
    CHECK(controller.edit_lane() == 3);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("skin HUD layout switches both directions and persists through the standard settings path") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    CHECK(runtime.skin.hud_layout == "studio");
    auto effects = controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                     SkinSettingsRowId::HudLayout);
    CHECK(runtime.skin.hud_layout == "classic");
    CHECK(effects.menu.render_changed);
    CHECK(controller.dirty());
    effects = controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                                SkinSettingsRowId::HudLayout);
    CHECK(runtime.skin.hud_layout == "studio");
    static_cast<void>(controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::HudLayout));
    CHECK(runtime.skin.hud_layout == "classic");
    effects = controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames);
    CHECK(effects.menu.persist_config);
    CHECK(effects.menu.navigate_back);
}

TEST_CASE("Studio riff map toggle follows keyboard and pointer settings routing for Native and LR2") {
    for (const auto source : {"native", "lr2"}) {
        tenriff::config::RuntimeConfig runtime;
        runtime.skin.source = source;
        SkinSettingsController controller;
        controller.reset("10k");
        REQUIRE(runtime.skin.hud_riff_map_visible);
        auto effects = controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                         SkinSettingsRowId::HudRiffMapVisible);
        CHECK_FALSE(runtime.skin.hud_riff_map_visible);
        CHECK(effects.menu.render_changed);
        CHECK(controller.dirty());
        static_cast<void>(controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                             SkinSettingsRowId::HudRiffMapVisible));
        CHECK(runtime.skin.hud_riff_map_visible);
        effects = controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames);
        CHECK(effects.menu.persist_config);
        CHECK(effects.menu.navigate_back);
    }
}

TEST_CASE("retired skin controls cannot mutate old presets through stale mouse targets") {
    for (const auto source : {"native", "lr2", "tenriff"}) {
        tenriff::config::RuntimeConfig runtime;
        runtime.skin.source = source;
        runtime.skin.indicator_background_visible = true;
        runtime.skin.show_gear_boundary_line = true;
        runtime.skin.judgement_offset_x = 130;
        runtime.skin.combo_offset_x = -120;
        runtime.skin.timing_text_offset_x = 90;
        runtime.skin.timing_bar_offset_x = -80;
        SkinSettingsController controller;
        controller.reset("10k");
        const auto saved = tenriff::config::serialize_skin_config(runtime.skin);
        for (const auto id : {SkinSettingsRowId::TargetGap, SkinSettingsRowId::VisualPreset,
                              SkinSettingsRowId::IndicatorBackground, SkinSettingsRowId::TargetLane,
                              SkinSettingsRowId::SingleColor, SkinSettingsRowId::NoteBorder,
                              SkinSettingsRowId::ImageAspect, SkinSettingsRowId::VisualOpacity,
                              SkinSettingsRowId::NoteOutlineOpacity, SkinSettingsRowId::LaneBackgroundOpacity,
                              SkinSettingsRowId::UiFont, SkinSettingsRowId::UiFontFile,
                              SkinSettingsRowId::CenterGap, SkinSettingsRowId::DividerWidth,
                              SkinSettingsRowId::KeyMode, SkinSettingsRowId::GearBoundary}) {
            const auto changed = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames, id);
            CHECK(changed.empty());
            CHECK(controller.selected_id() == SkinSettingsRowId::SkinSource);
            CHECK_FALSE(controller.dirty());
        }
        CHECK(tenriff::config::serialize_skin_config(runtime.skin) == saved);
    }
}

TEST_CASE("skin preset actions use the same keyboard and mouse controller path") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    const auto editor = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                          SkinSettingsRowId::OpenSkinEditor);
    CHECK(editor.boundary_action == SkinBoundaryAction::OpenSkinEditor);
    CHECK_FALSE(editor.menu.persist_config);
    const auto imported = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                             SkinSettingsRowId::ImportPreset);
    CHECK(imported.boundary_action == SkinBoundaryAction::ImportPreset);
    CHECK_FALSE(imported.menu.persist_config);
    const auto font = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                         SkinSettingsRowId::UiFontFile);
    CHECK(font.empty());
    CHECK_FALSE(font.menu.persist_config);
    CHECK_FALSE(controller.dirty());
    CHECK(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                             SkinSettingsRowId::UiFontFile).empty());
}

TEST_CASE("skin keyboard navigation opens preset export and import") {
    using tenriff::app::menu::settings::skin_setting_id_at;
    using tenriff::app::menu::settings::skin_setting_index;
    for (const auto source : {"native", "lr2", "tenriff"}) {
        tenriff::config::RuntimeConfig runtime;
        runtime.skin.source = source;
        const bool lr2 = runtime.skin.source == "lr2";
        SkinSettingsController controller;
        controller.reset("10k");
        const bool tenriff_source = runtime.skin.source == "tenriff";
        const auto before_export = tenriff_source ? SkinSettingsRowId::ReloadSkin : SkinSettingsRowId::ImportSkin;
        static_cast<void>(controller.select(before_export, lr2, true, tenriff_source));
        const auto reload_index = skin_setting_index(before_export, lr2, true, tenriff_source);
        REQUIRE(reload_index.has_value());
        CHECK(skin_setting_id_at(*reload_index + 1, lr2, true, tenriff_source) == SkinSettingsRowId::ExportPreset);
        REQUIRE(skin_setting_index(SkinSettingsRowId::ExportPreset, lr2).has_value());
        static_cast<void>(controller.handle(MenuAction::move(1), runtime, kLr2Names, kTenRiffNames));
        CHECK(controller.selected_id() == SkinSettingsRowId::ExportPreset);
        const auto exported = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames);
        CHECK(exported.boundary_action == SkinBoundaryAction::ExportPreset);
        CHECK_FALSE(exported.menu.persist_config);

        static_cast<void>(controller.handle(MenuAction::move(1), runtime, kLr2Names, kTenRiffNames));
        CHECK(controller.selected_id() == SkinSettingsRowId::ImportPreset);
        const auto imported = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames);
        CHECK(imported.boundary_action == SkinBoundaryAction::ImportPreset);
        static_cast<void>(controller.handle(MenuAction::move(1), runtime, kLr2Names, kTenRiffNames));
        CHECK(controller.selected_id() == SkinSettingsRowId::OpenSkinEditor);
        static_cast<void>(controller.handle(MenuAction::move(-1), runtime, kLr2Names, kTenRiffNames));
        CHECK(controller.selected_id() == SkinSettingsRowId::ImportPreset);
        static_cast<void>(controller.handle(MenuAction::move(-1), runtime, kLr2Names, kTenRiffNames));
        CHECK(controller.selected_id() == SkinSettingsRowId::ExportPreset);
        static_cast<void>(controller.handle(MenuAction::move(-1), runtime, kLr2Names, kTenRiffNames));
        CHECK(controller.selected_id() == before_export);
        CHECK_FALSE(controller.dirty());
    }
}

TEST_CASE("skin controller owns edit mode lane and gap selection") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("7k");
    CHECK(controller.edit_mode() == "7k");
    CHECK(controller.edit_lane() == 0);

    auto effects = controller.select_mode("7+1", false);
    CHECK(controller.edit_mode() == "7+1");
    CHECK(effects.menu.render_changed);
    CHECK_FALSE(controller.dirty());

    effects = controller.select_lane(8);
    CHECK(controller.edit_lane() == 7);
    CHECK_FALSE(effects.menu.persist_config);
    CHECK(controller.edit_gap() == 6);
    static_cast<void>(controller.select_lane(3));
    CHECK(controller.edit_gap() == 2);
    const auto original = tenriff::config::resolved_skin_lane_spacing_scales(runtime.skin, "7+1");
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::LaneSpacing));
    const auto adjusted = tenriff::config::resolved_skin_lane_spacing_scales(runtime.skin, "7+1");
    for (std::size_t i = 0; i < original.size(); ++i) {
        CHECK(adjusted[i] == original[i]);
    }
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
    CHECK(runtime.skin.note_border_enabled == old_border);

    const double old_opacity = runtime.skin.visual_opacity;
    static_cast<void>(controller.handle(
        MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::VisualOpacity));
    CHECK(runtime.skin.visual_opacity == old_opacity);

    const double old_offset = runtime.visual_offset_ms;
    static_cast<void>(controller.handle(
        MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::VisualLatency));
    CHECK(runtime.visual_offset_ms == doctest::Approx(old_offset + 1.0));
}

TEST_CASE("note and field size controls reach 10 percent without changing other modes") {
    std::vector<std::string> modes{"7+1"};
    for (int keys = 4; keys <= 16; ++keys) modes.push_back(std::to_string(keys) + "k");
    for (const auto& mode : modes) {
        tenriff::config::RuntimeConfig runtime;
        runtime.skin.note_width_scale = 0.15;
        const std::string other_mode = mode == "4k" ? "16k" : "4k";
        runtime.skin.note_width_scales[other_mode] = 0.80;
        SkinSettingsController controller;
        controller.reset(mode);
        for (int press = 0; press < 6; ++press) {
            static_cast<void>(controller.handle(
                MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                SkinSettingsRowId::NoteWidth));
            const double width = tenriff::config::resolved_skin_note_width_scale(runtime.skin, mode);
            CHECK(width == doctest::Approx(std::max(0.10, 0.14 - press * 0.01)));
            if (press >= 4) CHECK(tenriff::app::format_percent(width) == "10%");
        }
        static_cast<void>(controller.handle(
            MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
            SkinSettingsRowId::NoteWidth));
        CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, mode) == doctest::Approx(0.11));
        CHECK(runtime.skin.note_width_scale == doctest::Approx(0.15));
        CHECK(runtime.skin.note_width_scales.at(other_mode) == doctest::Approx(0.80));
        CHECK(runtime.skin.note_width_scales.size() == 2u);

        runtime.skin.note_width_scales[mode] = 1.35;
        for (int press = 0; press < 6; ++press) {
            static_cast<void>(controller.handle(
                MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                SkinSettingsRowId::NoteWidth));
        }
        CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, mode) == doctest::Approx(1.40));
        CHECK(controller.dirty());
    }
}

TEST_CASE("note height controls stop at 50 and 400 percent for every supported key count") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    for (int keys = 4; keys <= 16; ++keys) {
        const std::string mode = std::to_string(keys) + "k";
        controller.reset(mode);
        runtime.skin.note_height_scales[mode] = 0.55;
        for (int press = 0; press < 6; ++press) {
            static_cast<void>(controller.handle(
                MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                SkinSettingsRowId::NoteHeight));
        }
        const auto lower = tenriff::config::resolved_skin_note_height_scale(runtime.skin, mode);
        CHECK(lower == doctest::Approx(0.5));
        CHECK(tenriff::app::format_percent(lower) == "50%");
        runtime.skin.note_height_scales[mode] = 3.95;
        for (int press = 0; press < 6; ++press) {
            static_cast<void>(controller.handle(
                MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                SkinSettingsRowId::NoteHeight));
        }
        const auto upper = tenriff::config::resolved_skin_note_height_scale(runtime.skin, mode);
        CHECK(upper == doctest::Approx(4.0));
        CHECK(tenriff::app::format_percent(upper) == "400%");
        CHECK(controller.dirty());
    }
    CHECK(tenriff::app::format_percent(1.0) == "100%");
    CHECK(tenriff::app::format_percent(2.5) == "250%");
}

TEST_CASE("skin size first adjustment starts at the displayed inherited value") {
    std::vector<std::string> modes{"7+1"};
    for (int keys = 4; keys <= 16; ++keys) modes.push_back(std::to_string(keys) + "k");
    for (const auto& mode : modes) {
        for (const double inherited : {1.0, 1.8}) {
            for (const int direction : {-1, 1}) {
                tenriff::config::RuntimeConfig runtime;
                const double inherited_width = std::min(inherited, tenriff::config::kNoteWidthScaleMax);
                runtime.skin.note_width_scale = inherited_width;
                runtime.skin.note_height_scale = inherited;
                REQUIRE(runtime.skin.note_width_scales.empty());
                REQUIRE(runtime.skin.note_height_scales.empty());
                CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, mode) ==
                      doctest::Approx(inherited_width));
                SkinSettingsController controller;
                controller.reset(mode);
                static_cast<void>(controller.handle(
                    MenuAction::adjust(direction), runtime, kLr2Names, kTenRiffNames,
                    SkinSettingsRowId::NoteWidth));
                static_cast<void>(controller.handle(
                    MenuAction::adjust(direction), runtime, kLr2Names, kTenRiffNames,
                    SkinSettingsRowId::NoteHeight));
                const double expected_width = std::clamp(inherited_width + direction * 0.01,
                    tenriff::config::kNoteWidthScaleMin, tenriff::config::kNoteWidthScaleMax);
                CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, mode) ==
                      doctest::Approx(expected_width));
                CHECK(tenriff::config::resolved_skin_note_height_scale(runtime.skin, mode) ==
                      doctest::Approx(inherited + direction * 0.01));
                // Only the edited mode receives an override; the legacy global stays intact.
                CHECK(runtime.skin.note_width_scale == doctest::Approx(inherited_width));
                CHECK(runtime.skin.note_height_scale == doctest::Approx(inherited));
                CHECK(runtime.skin.note_width_scales.size() == 1u);
                CHECK(runtime.skin.note_height_scales.size() == 1u);
                CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
            }
        }
    }
}

TEST_CASE("skin geometry first adjustment preserves lane defaults and explicit per-mode values") {
    tenriff::config::RuntimeConfig runtime;
    runtime.skin.note_width_scale = 1.3;
    runtime.skin.note_height_scale = 1.8;
    runtime.skin.note_width_scales["16k"] = 1.2;
    runtime.skin.note_height_scales["16k"] = 2.0;
    runtime.skin.lane_center_gap_scale = 0.8;
    SkinSettingsController controller;
    controller.reset("16k");
    for (const auto id : {SkinSettingsRowId::LaneWidth, SkinSettingsRowId::LaneSpacing,
                          SkinSettingsRowId::DividerWidth, SkinSettingsRowId::CenterGap,
                          SkinSettingsRowId::NoteWidth, SkinSettingsRowId::NoteHeight}) {
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, id));
    }
    const auto widths = tenriff::config::resolved_skin_lane_width_scales(runtime.skin, "16k");
    REQUIRE(widths.size() == 16u);
    CHECK(widths[0] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault + 0.01));
    CHECK(widths[1] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault));
    const auto gaps = tenriff::config::resolved_skin_lane_spacing_scales(runtime.skin, "16k");
    REQUIRE(gaps.size() == 15u);
    CHECK(gaps[0] == doctest::Approx(tenriff::config::kLaneSpacingScaleDefault));
    CHECK(gaps[1] == doctest::Approx(tenriff::config::kLaneSpacingScaleDefault));
    CHECK(tenriff::config::resolved_skin_lane_divider_width_scale(runtime.skin, "16k") ==
          doctest::Approx(tenriff::config::kLaneDividerWidthScaleDefault));
    CHECK(tenriff::config::resolved_skin_lane_center_gap_scale(runtime.skin, "16k") == doctest::Approx(0.8));
    CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, "16k") == doctest::Approx(1.21));
    CHECK(tenriff::config::resolved_skin_note_height_scale(runtime.skin, "16k") == doctest::Approx(2.01));
}

TEST_CASE("skin 7+1 scratch side and adjusted sizes survive profile save and reload") {
    struct ProfileDir {
        std::filesystem::path path;
        ~ProfileDir() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } profile{std::filesystem::temp_directory_path() /
              ("tenriff-skin-first-adjust-" + std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()))};
    REQUIRE(std::filesystem::create_directory(profile.path));
    tenriff::config::RuntimeConfig runtime;
    runtime.skin.note_width_scale = 1.3;
    runtime.skin.note_height_scale = 1.8;
    SkinSettingsController controller;
    controller.reset("7k");
    static_cast<void>(controller.select_mode("7+1", false));
    REQUIRE(controller.edit_mode() == "7+1");
    CHECK(tenriff::app::key_mode_label(std::string(controller.edit_mode())) == "7+1");
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::ScratchPosition));
    REQUIRE(runtime.skin.scratch_position == "right");
    static_cast<void>(controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::NoteWidth));
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::NoteHeight));
    REQUIRE(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
    tenriff::config::ConfigLoader loader;
    REQUIRE(loader.save_profile(profile.path.u8string(), runtime));
    auto loaded = loader.load_profile(profile.path.u8string());
    REQUIRE(loaded.success());
    CHECK(loaded.config.skin.scratch_position == "right");
    CHECK(tenriff::config::resolved_skin_note_width_scale(loaded.config.skin, "7+1") == doctest::Approx(1.31));
    CHECK(tenriff::config::resolved_skin_note_height_scale(loaded.config.skin, "7+1") == doctest::Approx(1.79));
    CHECK(tenriff::config::resolved_skin_note_width_scale(loaded.config.skin, "7k") == doctest::Approx(1.3));
    const int scratch = 1;
    const auto right = tenriff::app::resolve_lane_presentation_layout(
        8, &scratch, 1, loaded.config.skin.scratch_position);
    CHECK(right.visual_lane_for_source(1) == 8);
    CHECK(right.visual_lane_for_source(2) == 1);
    controller.reset("7+1");
    static_cast<void>(controller.handle(MenuAction::adjust(-1), loaded.config, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::ScratchPosition));
    CHECK(loaded.config.skin.scratch_position == "left");
    static_cast<void>(controller.handle(MenuAction::adjust(-1), loaded.config, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::NoteWidth));
    CHECK(tenriff::config::resolved_skin_note_width_scale(loaded.config.skin, "7+1") == doctest::Approx(1.3));
    const auto left = tenriff::app::resolve_lane_presentation_layout(
        8, &scratch, 1, loaded.config.skin.scratch_position);
    CHECK(left.visual_lane_for_source(1) == 1);
    CHECK(left.visual_lane_for_source(2) == 2);
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
    CHECK(effects.empty());
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("skin platform actions remain explicit boundary requests") {
    tenriff::config::RuntimeConfig runtime;
    runtime.skin.source = "tenriff";
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
        SkinSettingsRowId::JudgementLine));
    effects = controller.handle(
        MenuAction::back(), runtime, kLr2Names, kTenRiffNames);
    CHECK(effects.menu.navigate_back);
    CHECK(effects.menu.persist_config);
    CHECK_FALSE(controller.dirty());
    CHECK(controller.selected_id() == SkinSettingsRowId::SkinSource);
}

TEST_CASE("judgement and combo offsets change independently and clamp to visible bounds") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    const double combo_y = runtime.skin.combo_position;
    (void)controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::JudgementY);
    CHECK(runtime.skin.judgement_position > combo_y);
    CHECK(runtime.skin.combo_position == combo_y);
    runtime.skin.judgement_offset_x = 600;
    (void)controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::JudgementX);
    CHECK(runtime.skin.judgement_offset_x == 600);
    (void)controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::ComboX);
    CHECK(runtime.skin.combo_offset_x == -10);
    CHECK(runtime.skin.judgement_offset_x == 600);
}

TEST_CASE("combo and judgement font sizes adjust independently with bounded mouse and keyboard controls") {
    tenriff::config::RuntimeConfig runtime;
    runtime.skin.source = "tenriff";
    SkinSettingsController controller;
    controller.reset("4k");
    static_cast<void>(controller.handle(MenuAction::set_ratio(0.5), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::ComboFontSize));
    CHECK(runtime.skin.combo_font_scale == doctest::Approx(1.25));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(1.0));
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::ComboFontSize));
    CHECK(runtime.skin.combo_font_scale == doctest::Approx(1.26));
    static_cast<void>(controller.handle(MenuAction::set_ratio(9.0), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::JudgementFontSize));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(2.0));
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(2.0));
    static_cast<void>(controller.handle(MenuAction::set_ratio(-2.0), runtime, kLr2Names, kTenRiffNames));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(0.5));
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(0.5));
    CHECK(runtime.skin.combo_font_scale == doctest::Approx(1.26));
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
}

TEST_CASE("timing indicator size controls preserve judgement combo positions and sibling scale") {
    tenriff::config::RuntimeConfig runtime;
    runtime.skin.source = "tenriff";
    runtime.skin.timing_text_offset_x = 120;
    runtime.skin.timing_bar_offset_y = -70;
    SkinSettingsController controller;
    controller.reset("4k");
    (void)controller.handle(MenuAction::set_ratio(0.5), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::TimingTextSize);
    CHECK(runtime.skin.timing_text_scale == doctest::Approx(1.25));
    CHECK(runtime.skin.timing_bar_scale == doctest::Approx(1.0));
    (void)controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::TimingBarSize);
    CHECK(runtime.skin.timing_bar_scale == doctest::Approx(0.99));
    (void)controller.handle(MenuAction::set_ratio(9), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::TimingTextSize);
    CHECK(runtime.skin.timing_text_scale == doctest::Approx(2.0));
    (void)controller.handle(MenuAction::set_ratio(-9), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::TimingBarSize);
    CHECK(runtime.skin.timing_bar_scale == doctest::Approx(0.5));
    CHECK(runtime.skin.combo_font_scale == doctest::Approx(1.0));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(1.0));
    CHECK(runtime.skin.timing_text_offset_x == 120);
    CHECK(runtime.skin.timing_bar_offset_y == -70);
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
}

TEST_CASE("simple rectangle is a distinct selectable note shape and retains rectangle compatibility") {
    tenriff::config::RuntimeConfig runtime;
    CHECK(runtime.skin.note_shape == "simple_rect");
    runtime.skin.note_shape = "rect";
    SkinSettingsController controller;
    controller.reset("4k");
    (void)controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::NoteShape);
    CHECK(runtime.skin.note_shape == "simple_rect");
    (void)controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames);
    CHECK(runtime.skin.note_shape == "rect");
}

TEST_CASE("skin editing starts at four keys and preserves a later edit choice without changing gameplay") {
    SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    runtime.mode.key_mode = "10k";
    CHECK(controller.edit_mode() == "4k");
    CHECK(tenriff::app::normalize_skin_edit_mode("unsupported") == "4k");
    CHECK(tenriff::app::normalize_skin_edit_mode("") == "4k");
    CHECK(tenriff::app::normalize_skin_edit_mode(" keys_10 ") == "10k");
    CHECK(tenriff::app::normalize_skin_edit_mode("7+1 SP") == "7+1");
    for (int keys = 4; keys <= 16; ++keys) {
        CHECK(tenriff::app::normalize_skin_edit_mode(std::to_string(keys) + "-Key") ==
              std::to_string(keys) + "k");
        CHECK(tenriff::app::key_mode_label(std::to_string(keys) + "k") == std::to_string(keys) + "K");
        CHECK(tenriff::app::lane_count_for_skin_mode(std::to_string(keys) + "k") == keys);
    }
    controller.reset(controller.edit_mode());
    CHECK(controller.edit_mode() == "4k");
    static_cast<void>(controller.select_mode("5k", false));
    CHECK(controller.edit_mode() == "5k");
    controller.reset(controller.edit_mode());
    CHECK(controller.edit_mode() == "5k");
    CHECK(runtime.mode.key_mode == "10k");
}

TEST_CASE("skin key backdrop toggle and opacity remain independent of hit bursts") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    const double burst = runtime.skin.key_pulse_brightness;
    runtime.skin.key_backdrop_enabled = true;
    runtime.skin.key_backdrop_opacity = 0.35;
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyBackdrop));
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.35));
    static_cast<void>(controller.handle(MenuAction::set_ratio(0.83), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyBackdropOpacity));
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.83));
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyBackdrop));
    CHECK(runtime.skin.key_backdrop_enabled);
    for (int press = 0; press < 25; ++press)
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyBackdropOpacity));
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(1.0));
    static_cast<void>(controller.handle(MenuAction::set_ratio(-2.0), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyBackdropOpacity));
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.0));
    CHECK(runtime.skin.key_pulse_brightness == doctest::Approx(burst));
    CHECK(runtime.skin.key_backdrop_override);
    CHECK(controller.dirty());
}

TEST_CASE("first backdrop edit starts at imported skin defaults and then owns the profile override") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.set_backdrop_defaults(false, 0.65f);
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropOpacity));
    CHECK(runtime.skin.key_backdrop_override);
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.64));
    controller.set_backdrop_defaults(true, 0.1f);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropOpacity));
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.65));
    runtime.skin.key_backdrop_override = false;
    controller.set_backdrop_defaults(false, 0.3f);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdrop));
    CHECK(runtime.skin.key_backdrop_enabled);
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.3));
    static_cast<void>(controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdrop));
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
}

TEST_CASE("skin related groups stay contiguous while stable targets roundtrip their display positions") {
    for (const bool lr2 : {false, true}) {
        std::vector<tenriff::app::SkinSettingsCategory> completed;
        auto current = tenriff::app::SkinSettingsCategory::Source;
        const tenriff::app::SkinSettingsRows rows{lr2};
        for (int index = 0; index < rows.count(); ++index) {
            const auto id = tenriff::app::menu::settings::skin_setting_id_at(index, lr2);
            REQUIRE(id.has_value());
            CHECK(rows.index_of(*id) == index);
            const auto category = tenriff::app::skin_settings_category(*id);
            if (category != current) {
                for (const auto prior : completed) CHECK(category != prior);
                completed.push_back(current);
                current = category;
            }
        }
    }
}

TEST_CASE("backdrop height edits adopt imported brightness without changing opacity or enabling the effect") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    const double pulse = runtime.skin.key_pulse_brightness;
    controller.set_backdrop_defaults(false, 0.65f, 0.4f, 0.8f);
    static_cast<void>(controller.handle(MenuAction::set_ratio(0.3), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropHeight));
    CHECK(runtime.skin.key_backdrop_override);
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.65));
    CHECK(runtime.skin.key_backdrop_brightness == doctest::Approx(0.4));
    CHECK(runtime.skin.key_backdrop_height == doctest::Approx(0.3));
    controller.set_backdrop_defaults(true, 0.1f, 0.1f, 0.1f);
    static_cast<void>(controller.handle(MenuAction::set_ratio(0.85), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropBrightness));
    CHECK(runtime.skin.key_backdrop_brightness == doctest::Approx(1.7));
    CHECK(runtime.skin.key_backdrop_height == doctest::Approx(0.3));
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.65));
    CHECK_FALSE(runtime.skin.key_backdrop_enabled);
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropHeight));
    CHECK(runtime.skin.key_backdrop_height == doctest::Approx(0.29));
    static_cast<void>(controller.handle(MenuAction::set_ratio(4.0), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropBrightness));
    CHECK(runtime.skin.key_backdrop_brightness == doctest::Approx(2.0));
    static_cast<void>(controller.handle(MenuAction::set_ratio(-4.0), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::KeyBackdropHeight));
    CHECK(runtime.skin.key_backdrop_height == doctest::Approx(0.0));
    CHECK(runtime.skin.key_pulse_brightness == doctest::Approx(pulse));
}

TEST_CASE("timing controls adopt manifest switches independently and persist on exit") {
    SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    controller.reset("10k");
    controller.set_timing_defaults(false, true);
    auto change = [&](SkinSettingsRowId id, int direction = 1) {
        return controller.handle(MenuAction::adjust(direction), runtime, kLr2Names, kTenRiffNames, id);
    };
    CHECK(change(SkinSettingsRowId::TimingFeedback).menu.render_changed);
    CHECK(runtime.skin.show_timing_feedback);
    CHECK(runtime.skin.show_timing_bar);
    CHECK(runtime.skin.timing_feedback_override);
    change(SkinSettingsRowId::TimingBar);
    CHECK(runtime.skin.show_timing_feedback);
    CHECK_FALSE(runtime.skin.show_timing_bar);
    // Saved user choices beat a different imported skin on reload.
    const auto visible = tenriff::app::resolve_timing_feedback_visibility(runtime.skin, false, true);
    CHECK(visible.text);
    CHECK_FALSE(visible.bar);
    change(SkinSettingsRowId::TimingTextX);
    change(SkinSettingsRowId::TimingTextY, -1);
    change(SkinSettingsRowId::TimingBarX, -1);
    change(SkinSettingsRowId::TimingBarY);
    CHECK(runtime.skin.timing_text_offset_x == 10);
    CHECK(runtime.skin.timing_text_offset_y == -10);
    CHECK(runtime.skin.timing_bar_offset_x == -10);
    CHECK(runtime.skin.timing_bar_offset_y == 10);
    CHECK(runtime.skin.judgement_offset_x == 0);
    runtime.skin.timing_bar_offset_y = 400;
    change(SkinSettingsRowId::TimingBarY);
    CHECK(runtime.skin.timing_bar_offset_y == 400);
    const auto back = controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames);
    CHECK(back.menu.persist_config);
    CHECK(back.menu.navigate_back);
}

TEST_CASE("note fog controls change separately in one percent steps and save") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("7+1");
    for (int n = 0; n < 7; ++n)
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::NoteFadeIn));
    CHECK(runtime.skin.note_fade_in == doctest::Approx(0.07));
    CHECK(runtime.skin.note_fade_out == 0.0);
    for (int n = 0; n < 101; ++n)
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::NoteFadeOut));
    CHECK(runtime.skin.note_fade_out == 1.0);
    for (int n = 0; n < 101; ++n)
        static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::NoteFadeOut));
    CHECK(runtime.skin.note_fade_out == 0.0);
    CHECK(runtime.skin.note_fade_in == doctest::Approx(0.07));
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
}

TEST_CASE("timing bar display mode adopts skin defaults without changing its visibility switch") {
    SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    controller.reset("10k");
    controller.set_timing_defaults(false, false, true);
    const auto before = tenriff::app::resolve_timing_feedback_visibility(runtime.skin, false, false, true);
    CHECK(before.always_visible);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::TimingBarMode));
    CHECK(runtime.skin.timing_feedback_override);
    CHECK_FALSE(runtime.skin.show_timing_feedback);
    CHECK_FALSE(runtime.skin.show_timing_bar);
    CHECK_FALSE(runtime.skin.timing_bar_always_visible);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                       SkinSettingsRowId::TimingBarMode));
    CHECK(runtime.skin.timing_bar_always_visible);
    CHECK_FALSE(runtime.skin.show_timing_bar);
}

TEST_CASE("indicator shape selection changes preview settings and persists on exit") {
    SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    controller.reset("10k");
    CHECK(runtime.skin.timing_marker_style == "dot");
    const auto effects = controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                          SkinSettingsRowId::TimingMarkerStyle);
    CHECK(runtime.skin.timing_marker_style == "barcode");
    CHECK(effects.menu.render_changed);
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
}


TEST_CASE("judgement line type cycles by keyboard and pointer and saves independently of visibility") {
    SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    controller.reset("10k");
    const auto id = SkinSettingsRowId::JudgementLineType;
    CHECK(runtime.skin.judgement_line_type == 1);
    auto effects = controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, id);
    CHECK(effects.menu.render_changed);
    CHECK(runtime.skin.judgement_line_type == 2);
    CHECK(runtime.skin.show_judgement_line);
    effects = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames, id);
    CHECK(effects.menu.render_changed);
    CHECK(runtime.skin.judgement_line_type == 1);
    runtime.skin.show_judgement_line = false;
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, id));
    CHECK(runtime.skin.judgement_line_type == 2);
    CHECK_FALSE(runtime.skin.show_judgement_line);
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
}


TEST_CASE("skin cleanup hides legacy controls and rejects stale pointer targets") {
    using namespace tenriff::app;
    tenriff::config::RuntimeConfig runtime;
    runtime.skin.show_hold_tail = true;
    runtime.skin.hold_body_opacity = 0.35;
    runtime.skin.hold_body_width_scale = 0.7;
    SkinSettingsController controller;
    controller.reset("auto");
    const auto saved = tenriff::config::serialize_skin_config(runtime.skin);
    for (auto id : {SkinSettingsRowId::ShowHoldTail, SkinSettingsRowId::LnTailTaper,
                   SkinSettingsRowId::LnBodyWidth, SkinSettingsRowId::LnBodyOpacity,
                   SkinSettingsRowId::BlackPlayfield, SkinSettingsRowId::LaneSpacing,
                   SkinSettingsRowId::KeyLabelPosition, SkinSettingsRowId::GameplayCursor}) {
        CHECK(SkinSettingsRows{}.index_of(id) < 0);
        CHECK(controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames, id).empty());
    }
    CHECK(tenriff::config::serialize_skin_config(runtime.skin) == saved);
    for (bool lr2 : {false, true}) for (bool original : {false, true}) {
        const SkinSettingsRows rows{lr2, original};
        CHECK((rows.index_of(SkinSettingsRowId::ScratchPosition) >= 0) == original);
        for (int i = 0; i < rows.count(); ++i) {
            auto id = menu::settings::skin_setting_id_at(i, lr2, original);
            REQUIRE(id.has_value());
            CHECK(rows.index_of(*id) == i);
        }
    }
    controller.reset("7+1");
    runtime.mode.key_mode = "auto";
    CHECK(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::ScratchPosition).menu.persist_config == false);
    CHECK(runtime.skin.scratch_position == "right");
    runtime.mode.key_mode = "8k";
    CHECK(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                            SkinSettingsRowId::ScratchPosition).empty());
    CHECK(runtime.skin.scratch_position == "right");
    CHECK(controller.selected_id() == SkinSettingsRowId::SkinSource);
}

TEST_CASE("field judgement counts toggle independently from timing visibility") {
    SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    controller.reset("5k");
    runtime.skin.show_timing_bar = false;
    runtime.skin.show_timing_feedback = false;
    auto result = controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::JudgementCounts);
    CHECK(result.menu.render_changed);
    CHECK(runtime.skin.show_judgement_counts);
    CHECK_FALSE(runtime.skin.show_timing_bar);
    CHECK_FALSE(runtime.skin.show_timing_feedback);
    CHECK(controller.dirty());
    CHECK(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
        SkinSettingsRowId::JudgementCounts).menu.render_changed);
    CHECK_FALSE(runtime.skin.show_judgement_counts);
}

TEST_CASE("skin source visibility matches keyboard and stale action dispatch") {
    for (const auto source : {"native", "lr2", "tenriff"}) {
        tenriff::config::RuntimeConfig runtime;
        runtime.skin.source = source;
        SkinSettingsController controller;
        controller.reset("auto");
        const bool tenriff_source = runtime.skin.source == "tenriff";
        const bool lr2 = runtime.skin.source == "lr2";
        const tenriff::app::SkinSettingsRows rows{lr2, true, tenriff_source};
        CHECK((rows.index_of(SkinSettingsRowId::ImportedSkin) >= 0) == (lr2 || tenriff_source));
        for (const auto id : {SkinSettingsRowId::CreateSkin, SkinSettingsRowId::OpenSkinFolder, SkinSettingsRowId::ReloadSkin}) {
            CHECK((rows.index_of(id) >= 0) == tenriff_source);
            const auto result = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames, id);
            CHECK((result.boundary_action != SkinBoundaryAction::None) == tenriff_source);
        }
    }
}

TEST_CASE("skin mode tabs retain per-mode edits and filtered navigation cannot edit hidden rows") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("16k");
    static_cast<void>(controller.select_lane(16));
    const auto original = tenriff::config::serialize_skin_config(runtime.skin);
    for (const auto mode : tenriff::app::kSkinEditModes) {
        CHECK(controller.select_mode(mode, false).menu.render_changed);
        CHECK(controller.edit_mode() == mode);
        CHECK(controller.edit_lane() < tenriff::app::lane_count_for_skin_mode(std::string(mode)));
        CHECK_FALSE(controller.dirty());
    }
    CHECK(tenriff::config::serialize_skin_config(runtime.skin) == original);
    CHECK(controller.select_mode("17k", false).empty());
    controller.set_visible_rows({SkinSettingsRowId::NoteWidth, SkinSettingsRowId::NoteHeight});
    CHECK(controller.selected_id() == SkinSettingsRowId::NoteWidth);
    static_cast<void>(controller.handle(MenuAction::move(1), runtime, kLr2Names, kTenRiffNames));
    CHECK(controller.selected_id() == SkinSettingsRowId::NoteHeight);
    controller.set_visible_rows({});
    CHECK(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames).empty());
    CHECK(tenriff::config::serialize_skin_config(runtime.skin) == original);
}

TEST_CASE("key overlay plugin toggle and position bounds use real skin settings actions") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("6k"); // Unsupported play mode must still retain user settings.
    CHECK_FALSE(runtime.skin.key_overlay_enabled);
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyOverlayEnabled));
    CHECK(runtime.skin.key_overlay_enabled);
    runtime.skin.key_overlay_x = 1.0;
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyOverlayX));
    CHECK(runtime.skin.key_overlay_x == 1.0);
    runtime.skin.key_overlay_y = 0;
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyOverlayY));
    CHECK(runtime.skin.key_overlay_y == 0.0);
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyOverlayEnabled));
    CHECK_FALSE(runtime.skin.key_overlay_enabled);
}
