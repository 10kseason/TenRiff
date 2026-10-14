#include "doctest/doctest.h"

#include <cstdint>

#include "app/MenuAppSettingsUtils.h"
#include "app/menu/MenuAction.h"
#include "app/menu/settings/ModeSettingsController.h"

namespace {

namespace config = tenriff::config;
namespace ui = tenriff::ui;

using tenriff::app::menu::MenuAction;
using tenriff::app::menu::settings::ModeSettingId;
using tenriff::app::menu::settings::ModeSettingsController;

}  // namespace

TEST_CASE("mode setting identifiers preserve renderer row values") {
    using tenriff::app::menu::settings::kModeSettingOrder;
    using tenriff::app::menu::settings::mode_setting_id_at;
    using tenriff::app::menu::settings::mode_setting_index;

    static_assert(static_cast<std::uint8_t>(ModeSettingId::Indexing) == 0);
    static_assert(static_cast<std::uint8_t>(ModeSettingId::Mods) == 14);
    static_assert(static_cast<std::uint8_t>(ModeSettingId::Back) == 17);
    static_assert(static_cast<std::uint8_t>(ModeSettingId::JudgeWindow) == 18);
    CHECK(*mode_setting_index(ModeSettingId::JudgeWindow) + 1 == *mode_setting_index(ModeSettingId::Mods));
    for (std::size_t index = 0; index < kModeSettingOrder.size(); ++index) {
        REQUIRE(mode_setting_id_at(index).has_value());
        CHECK(*mode_setting_id_at(index) == kModeSettingOrder[index]);
        REQUIRE(mode_setting_index(kModeSettingOrder[index]).has_value());
        CHECK(*mode_setting_index(kModeSettingOrder[index]) == index);
    }
    CHECK_FALSE(mode_setting_id_at(kModeSettingOrder.size()).has_value());
}

TEST_CASE("mode indexing changes defer exactly one library refresh until Back") {
    tenriff::config::RuntimeConfig runtime;
    ModeSettingsController controller;

    auto effects = controller.handle(
        MenuAction::adjust(1), runtime, ModeSettingId::Indexing);
    CHECK(effects.menu.render_changed);
    CHECK_FALSE(effects.refresh_song_library);
    CHECK(controller.dirty());
    CHECK(controller.library_dirty());

    effects = controller.handle(MenuAction::back(), runtime);
    CHECK(effects.menu.persist_config);
    CHECK(effects.refresh_song_library);
    CHECK(effects.menu.navigate_back);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("mode assist choices remain mutually exclusive") {
    tenriff::config::RuntimeConfig runtime;
    ModeSettingsController controller;
    runtime.mode.one_miss_fail_enabled = true;
    runtime.mode.pacemaker_mode = "score";

    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, ModeSettingId::PracticeNoFail));
    CHECK(runtime.mode.practice_no_fail_enabled);
    CHECK_FALSE(runtime.mode.one_miss_fail_enabled);
    CHECK(runtime.mode.pacemaker_mode == "off");

    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, ModeSettingId::SuddenDeath));
    CHECK(runtime.mode.one_miss_fail_enabled);
    CHECK_FALSE(runtime.mode.practice_no_fail_enabled);

    static_cast<void>(controller.handle(
        MenuAction::adjust(1), runtime, ModeSettingId::Pacemaker));
    CHECK(tenriff::config::normalize_pacemaker_mode_token(runtime.mode.pacemaker_mode) != "off");
    CHECK_FALSE(runtime.mode.practice_no_fail_enabled);
    CHECK_FALSE(runtime.mode.one_miss_fail_enabled);
}

TEST_CASE("mode locked dependent rows are non-mutating") {
    tenriff::config::RuntimeConfig runtime;
    ModeSettingsController controller;
    runtime.mode.pacemaker_mode = "off";
    const double old_accuracy = runtime.mode.pacemaker_target_accuracy;
    auto effects = controller.handle(
        MenuAction::adjust(1), runtime, ModeSettingId::PacemakerTarget);
    CHECK(runtime.mode.pacemaker_target_accuracy == doctest::Approx(old_accuracy));
    CHECK_FALSE(controller.dirty());
    CHECK(effects.menu.render_changed);

    runtime.mode.key_conversion_algorithm = "krrcream";
    const std::string old_preset = runtime.mode.key_conversion_nk2_preset;
    effects = controller.handle(
        MenuAction::adjust(1), runtime, ModeSettingId::Nk2Preset);
    CHECK(runtime.mode.key_conversion_nk2_preset == old_preset);
    CHECK_FALSE(controller.dirty());
}

TEST_CASE("mode numeric rows clamp and use keyboard pointer parity") {
    tenriff::config::RuntimeConfig keyboard_runtime;
    tenriff::config::RuntimeConfig pointer_runtime;
    ModeSettingsController keyboard;
    ModeSettingsController pointer;

    static_cast<void>(keyboard.select(ModeSettingId::Rate));
    static_cast<void>(keyboard.handle(MenuAction::adjust(1), keyboard_runtime));
    static_cast<void>(pointer.handle(
        MenuAction::adjust(1), pointer_runtime, ModeSettingId::Rate));
    CHECK(keyboard_runtime.speed.rate == doctest::Approx(pointer_runtime.speed.rate));

    keyboard_runtime.mode.random_seed = tenriff::app::kSeedMax;
    static_cast<void>(keyboard.handle(
        MenuAction::adjust(1), keyboard_runtime, ModeSettingId::RandomSeed));
    CHECK(keyboard_runtime.mode.random_seed == tenriff::app::kSeedMax);
}

TEST_CASE("mode Mod Manager is nested and keeps dirty state for parent save") {
    tenriff::config::RuntimeConfig runtime;
    ModeSettingsController controller;

    auto effects = controller.handle(
        MenuAction::activate(), runtime, ModeSettingId::Mods);
    CHECK(effects.show_mod_manager);
    CHECK(controller.selected_mod_category() == 0);

    effects = controller.handle_mod_manager(MenuAction::adjust(1), runtime);
    CHECK(controller.dirty());
    CHECK_FALSE(runtime.mode.mods.empty());
    CHECK_FALSE(effects.menu.navigate_back);

    effects = controller.handle_mod_manager(MenuAction::back(), runtime);
    CHECK(effects.menu.navigate_back);
    CHECK(controller.selected_id() == ModeSettingId::Mods);
    CHECK(controller.dirty());

    effects = controller.handle(MenuAction::back(), runtime);
    CHECK(effects.menu.persist_config);
    CHECK(effects.menu.navigate_back);
}

TEST_CASE("mode Enter preserves the legacy leave behavior on existing non-Mods rows") {
    tenriff::config::RuntimeConfig runtime;
    ModeSettingsController controller;

    const auto effects = controller.handle(
        MenuAction::activate(), runtime, ModeSettingId::Autoplay);
    CHECK(effects.menu.navigate_back);
    CHECK_FALSE(runtime.mode.autoplay_enabled);
}

TEST_CASE("judge window has keyboard pointer and Enter parity across only Normal Hard and EX") {
    using namespace tenriff::app;
    config::RuntimeConfig keyboard_runtime;
    config::RuntimeConfig pointer_runtime;
    keyboard_runtime.mode.mods = pointer_runtime.mode.mods = {"dp_flip"};
    ModeSettingsController keyboard, pointer;
    (void)keyboard.select(ModeSettingId::RandomSeed);
    (void)keyboard.handle(MenuAction::move(1), keyboard_runtime);
    CHECK(keyboard.selected_id() == ModeSettingId::JudgeWindow);
    for (const char* expected : {"Hard", "EX", "Normal"}) {
        const auto keyed = keyboard.handle(MenuAction::adjust(1), keyboard_runtime);
        const auto clicked = pointer.handle(MenuAction::activate(), pointer_runtime, ModeSettingId::JudgeWindow);
        CHECK_FALSE(keyed.menu.navigate_back);
        CHECK_FALSE(clicked.menu.navigate_back);
        CHECK(mode_judge_window_label(keyboard_runtime.mode.mods, ui::Language::English) == expected);
        CHECK(keyboard_runtime.mode.mods == pointer_runtime.mode.mods);
        CHECK(std::find(keyboard_runtime.mode.mods.begin(), keyboard_runtime.mode.mods.end(), "dp_flip") != keyboard_runtime.mode.mods.end());
        CHECK(std::find(keyboard_runtime.mode.mods.begin(), keyboard_runtime.mode.mods.end(), "judge_easy") == keyboard_runtime.mode.mods.end());
    }
    (void)keyboard.handle(MenuAction::adjust(-1), keyboard_runtime);
    CHECK(mode_judge_window_label(keyboard_runtime.mode.mods, ui::Language::English) == "EX");
    (void)keyboard.handle(MenuAction::move(1), keyboard_runtime);
    CHECK(keyboard.selected_id() == ModeSettingId::Mods);
    CHECK(keyboard.handle(MenuAction::back(), keyboard_runtime).menu.persist_config);
    keyboard_runtime.mode.mods = {"judge_easy", "dp_flip"};
    (void)keyboard.handle(MenuAction::adjust(1), keyboard_runtime, ModeSettingId::JudgeWindow);
    CHECK(mode_judge_window_label(keyboard_runtime.mode.mods, ui::Language::Korean) == "Hard");
    CHECK(std::find(keyboard_runtime.mode.mods.begin(), keyboard_runtime.mode.mods.end(), "judge_easy") == keyboard_runtime.mode.mods.end());
}

TEST_CASE("nested mods filter judge without changing the registry or scratch row indices") {
    using namespace tenriff::app;
    const auto& categories = visible_mode_mod_categories();
    REQUIRE(categories.size() == 5);
    CHECK(categories.front().token == "dp_layout");
    CHECK(categories.back().token == "scratch_assist");
    for (const auto& category : categories) {
        CHECK(category.token != "judge_window");
        CHECK(mode_mod_category_label(category, ui::Language::Korean) != category.label);
        for (const auto* mod : category.mods)
            CHECK(mode_mod_value_label(*mod, ui::Language::Korean) != mod->label);
    }
    CHECK(std::any_of(mode_mod_categories().begin(), mode_mod_categories().end(),
        [](const auto& category) { return category.token == "judge_window"; }));
    config::RuntimeConfig runtime;
    runtime.mode.mods = {"judge_ex"};
    ModeSettingsController controller;
    (void)controller.handle_mod_manager(MenuAction::adjust(1), runtime, 0);
    CHECK(mode_mod_category_value("dp_layout", runtime.mode.mods) == "DP Flip");
    CHECK(mode_judge_window_label(runtime.mode.mods, ui::Language::English) == "EX");
    (void)controller.handle_mod_manager(MenuAction::adjust(1), runtime, 4);
    REQUIRE(mode_mod_auto_scratch(runtime.mode.mods));
    const bool before = runtime.mode.auto_scratch_hide_lanes;
    (void)controller.handle_mod_manager(MenuAction::adjust(1), runtime, 5);
    CHECK(runtime.mode.auto_scratch_hide_lanes != before);
    CHECK(controller.handle_mod_manager(MenuAction::activate(), runtime, 6).menu.navigate_back);
    CHECK(mode_mod_category_label(categories[0], ui::Language::Korean) == "더블 플레이 배치");
    CHECK(mode_mod_category_ui_value("dp_layout", runtime.mode.mods, ui::Language::Korean) == "좌우 필드 교환");
    CHECK(mode_mod_category_ui_value("scratch_assist", runtime.mode.mods, ui::Language::Korean) == "자동 스크래치 (연습)");
    CHECK(mode_mod_category_ui_value("note_add", runtime.mode.mods, ui::Language::Korean) == "끔");
}
