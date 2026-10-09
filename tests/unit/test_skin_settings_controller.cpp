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

TEST_CASE("skin stable identifiers map across the optional LR2 row") {
    using tenriff::app::menu::settings::skin_setting_id_at;

    REQUIRE(skin_setting_id_at(4, false).has_value());
    CHECK(*skin_setting_id_at(4, false) == SkinSettingsRowId::VisualLatency);
    REQUIRE(skin_setting_id_at(4, true).has_value());
    CHECK(*skin_setting_id_at(4, true) == SkinSettingsRowId::VisualLatency);
    CHECK(*skin_setting_id_at(5, false) == SkinSettingsRowId::ImportSkin);
    CHECK(*skin_setting_id_at(5, true) == SkinSettingsRowId::Lr2Resolution);
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

TEST_CASE("skin preset actions use the same keyboard and mouse controller path") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("10k");
    const auto editor = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                          SkinSettingsRowId::OpenSkinEditor);
    CHECK(editor.boundary_action == SkinBoundaryAction::OpenSkinEditor);
    CHECK_FALSE(editor.menu.persist_config);
    const auto exported = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                             SkinSettingsRowId::ExportPreset);
    CHECK(exported.boundary_action == SkinBoundaryAction::ExportPreset);
    const auto imported = controller.handle(MenuAction::activate(), runtime, kLr2Names, kTenRiffNames,
                                             SkinSettingsRowId::ImportPreset);
    CHECK(imported.boundary_action == SkinBoundaryAction::ImportPreset);
    CHECK_FALSE(imported.menu.persist_config);
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

TEST_CASE("note height controls stop at 50 and 400 percent for every supported key count") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    for (int keys = 4; keys <= 16; ++keys) {
        const std::string mode = std::to_string(keys) + "k";
        controller.reset(mode);
        runtime.skin.note_height_scales[mode] = 0.55;
        for (int press = 0; press < 2; ++press) {
            static_cast<void>(controller.handle(
                MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames,
                SkinSettingsRowId::NoteHeight));
        }
        const auto lower = tenriff::config::resolved_skin_note_height_scale(runtime.skin, mode);
        CHECK(lower == doctest::Approx(0.5));
        CHECK(tenriff::app::format_percent(lower) == "50%");
        runtime.skin.note_height_scales[mode] = 3.95;
        for (int press = 0; press < 2; ++press) {
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
                const double expected_width = std::clamp(inherited_width + direction * 0.05,
                    tenriff::config::kNoteWidthScaleMin, tenriff::config::kNoteWidthScaleMax);
                CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, mode) ==
                      doctest::Approx(expected_width));
                CHECK(tenriff::config::resolved_skin_note_height_scale(runtime.skin, mode) ==
                      doctest::Approx(inherited + direction * 0.05));
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
    CHECK(widths[0] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault + 0.05));
    CHECK(widths[1] == doctest::Approx(tenriff::config::kLaneWidthScaleDefault));
    const auto gaps = tenriff::config::resolved_skin_lane_spacing_scales(runtime.skin, "16k");
    REQUIRE(gaps.size() == 15u);
    CHECK(gaps[0] == doctest::Approx(tenriff::config::kLaneSpacingScaleDefault + 0.05));
    CHECK(gaps[1] == doctest::Approx(tenriff::config::kLaneSpacingScaleDefault));
    CHECK(tenriff::config::resolved_skin_lane_divider_width_scale(runtime.skin, "16k") ==
          doctest::Approx(tenriff::config::kLaneDividerWidthScaleDefault + 0.05));
    CHECK(tenriff::config::resolved_skin_lane_center_gap_scale(runtime.skin, "16k") == doctest::Approx(0.85));
    CHECK(tenriff::config::resolved_skin_note_width_scale(runtime.skin, "16k") == doctest::Approx(1.25));
    CHECK(tenriff::config::resolved_skin_note_height_scale(runtime.skin, "16k") == doctest::Approx(2.05));
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
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::KeyMode));
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
    CHECK(tenriff::config::resolved_skin_note_width_scale(loaded.config.skin, "7+1") == doctest::Approx(1.35));
    CHECK(tenriff::config::resolved_skin_note_height_scale(loaded.config.skin, "7+1") == doctest::Approx(1.75));
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
    CHECK(runtime.skin.combo_font_scale == doctest::Approx(1.30));
    static_cast<void>(controller.handle(MenuAction::set_ratio(9.0), runtime, kLr2Names, kTenRiffNames,
                                        SkinSettingsRowId::JudgementFontSize));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(2.0));
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(2.0));
    static_cast<void>(controller.handle(MenuAction::set_ratio(-2.0), runtime, kLr2Names, kTenRiffNames));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(0.5));
    static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames));
    CHECK(runtime.skin.judgement_font_scale == doctest::Approx(0.5));
    CHECK(runtime.skin.combo_font_scale == doctest::Approx(1.30));
    CHECK(controller.handle(MenuAction::back(), runtime, kLr2Names, kTenRiffNames).menu.persist_config);
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
    static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::KeyMode));
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
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.85));
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
    CHECK(runtime.skin.key_backdrop_opacity == doctest::Approx(0.6));
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
    CHECK(runtime.skin.key_backdrop_height == doctest::Approx(0.25));
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

TEST_CASE("note fog controls change separately in five percent steps and save") {
    tenriff::config::RuntimeConfig runtime;
    SkinSettingsController controller;
    controller.reset("7+1");
    for (int n = 0; n < 7; ++n)
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::NoteFadeIn));
    CHECK(runtime.skin.note_fade_in == doctest::Approx(0.35));
    CHECK(runtime.skin.note_fade_out == 0.0);
    for (int n = 0; n < 25; ++n)
        static_cast<void>(controller.handle(MenuAction::adjust(1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::NoteFadeOut));
    CHECK(runtime.skin.note_fade_out == 1.0);
    for (int n = 0; n < 25; ++n)
        static_cast<void>(controller.handle(MenuAction::adjust(-1), runtime, kLr2Names, kTenRiffNames, SkinSettingsRowId::NoteFadeOut));
    CHECK(runtime.skin.note_fade_out == 0.0);
    CHECK(runtime.skin.note_fade_in == doctest::Approx(0.35));
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
