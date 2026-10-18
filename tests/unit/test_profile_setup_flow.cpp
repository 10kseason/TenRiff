#include "doctest/doctest.h"

#include "app/ProfileSetupFlow.h"
#include "app/GameplayStartPolicy.h"

using namespace tenriff::app::profile_setup;

TEST_CASE("first-run profile setup keeps its original song-select and title exits") {
    CHECK(row_count(Entry::FirstRun) == 31);
    CHECK(kLanguageRow == 0);
    CHECK(kMenuFontSizeRow == 1);
    CHECK(kBackendRow == kNicknameRow - 1);
    CHECK(kNicknameRow == kAvatarRow - 1);
    CHECK(kAvatarRow == kClearAvatarRow - 1);
    CHECK(kClearAvatarRow == kExportSettingsRow - 1);
    CHECK(kImportSettingsRow == kSkipStartCountdownRow - 1);
    CHECK(kSkipStartCountdownRow == kInstantRestartKeyRow - 1);
    CHECK(kInstantRestartKeyRow == kHispeedDecreaseKeyRow - 1);
    CHECK(kHispeedDecreaseKeyRow == kHispeedIncreaseKeyRow - 1);
    CHECK(kHispeedIncreaseKeyRow == kFunctionKeysFirstRow - 1);
    CHECK(kFunctionKeysLastRow == kDoneRow - 1);
    CHECK(enter_destination(Entry::FirstRun, kDoneRow) == Destination::SongSelect);
    CHECK(enter_destination(Entry::FirstRun, kFirstRunSkipRow) == Destination::Title);
    CHECK(cancel_destination(Entry::FirstRun) == Destination::Title);
}

TEST_CASE("reopened profile setup returns to the options hub") {
    CHECK(row_count(Entry::Options) == 30);
    CHECK(enter_destination(Entry::Options, kDoneRow) == Destination::OptionsHub);
    CHECK(enter_destination(Entry::Options, 2) == Destination::Stay);
    CHECK(cancel_destination(Entry::Options) == Destination::OptionsHub);
}

TEST_CASE("profile presentation controls cycle all languages and text sizes immediately") {
    tenriff::config::UiConfig settings;
    CHECK(adjust_presentation_setting(settings, kLanguageRow, -1));
    CHECK(settings.language == "zh-CN");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
    CHECK(settings.language == "en");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
    CHECK(settings.language == "ko");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
    CHECK(settings.language == "ja");

    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
    CHECK(settings.language == "zh-CN");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, -1));
    CHECK(settings.language == "ja");

    CHECK(adjust_presentation_setting(settings, kMenuFontSizeRow, 1));
    CHECK(settings.menu_font_size == "large");
    CHECK(tenriff::config::menu_text_scale(settings.menu_font_size) == doctest::Approx(1.15));
    CHECK(adjust_presentation_setting(settings, kMenuFontSizeRow, 1));
    CHECK(settings.menu_font_size == "extra_large");
    CHECK(tenriff::config::menu_text_scale(settings.menu_font_size) == doctest::Approx(1.30));
    CHECK(adjust_presentation_setting(settings, kMenuFontSizeRow, 1));
    CHECK(settings.menu_font_size == "normal");
    CHECK(adjust_presentation_setting(settings, kMenuFontSizeRow, -1));
    CHECK(settings.menu_font_size == "extra_large");
    CHECK_FALSE(adjust_presentation_setting(settings, kLanguageRow, 0));
    CHECK_FALSE(adjust_presentation_setting(settings, kMenuFontSizeRow, 0));
    CHECK_FALSE(adjust_presentation_setting(settings, kSongsFolderRow, 1));
    CHECK(settings.language == "ja");
    CHECK(settings.menu_font_size == "extra_large");
}

TEST_CASE("profile hold adjustment follows Rate and latency after presentation rows are added") {
    CHECK(supports_hold_adjustment(kRateRow));
    CHECK(supports_hold_adjustment(kVisualLatencyRow));
    CHECK_FALSE(supports_hold_adjustment(kLanguageRow));
    CHECK_FALSE(supports_hold_adjustment(kMenuFontSizeRow));
    CHECK_FALSE(supports_hold_adjustment(kSongsFolderRow));
    CHECK_FALSE(supports_hold_adjustment(kGaugeRow));
    CHECK_FALSE(supports_hold_adjustment(kDoneRow));
}

TEST_CASE("solo start countdown is opt-in skippable while multiplayer always synchronizes") {
    using tenriff::app::gameplay_start_countdown_seconds;
    tenriff::config::UiConfig defaults;
    CHECK_FALSE(defaults.skip_start_countdown);
    CHECK(defaults.instant_restart_key.empty());
    CHECK(gameplay_start_countdown_seconds(false, false, false) == 3);
    CHECK(gameplay_start_countdown_seconds(true, false, false) == 0);
    CHECK(gameplay_start_countdown_seconds(false, false, true) == 0);
    for (bool skip : {false, true}) for (bool instant : {false, true})
        CHECK(gameplay_start_countdown_seconds(skip, true, instant) == 3);
    CHECK_FALSE(supports_hold_adjustment(kSkipStartCountdownRow));
}

TEST_CASE("instant restart rejects reserved controls and active lane conflicts") {
    using tenriff::app::resolve_instant_restart_key;
    CHECK(resolve_instant_restart_key("", {}) == 0);
    CHECK(resolve_instant_restart_key("UnknownKey", {}) == 0);
    CHECK(resolve_instant_restart_key("Enter", {}) == 0);
    const auto letter = tenriff::config::KeycodeMap::to_keycode("R");
    REQUIRE(letter.has_value());
    CHECK(resolve_instant_restart_key("R", {}) == *letter);
    CHECK(resolve_instant_restart_key("R", {{*letter, 1}}) == 0);
#ifdef _WIN32
    CHECK(resolve_instant_restart_key("F9", {}) == 0);
    CHECK(resolve_instant_restart_key("LShift", {}) == 0);
    CHECK(resolve_instant_restart_key("F12", {}) != 0);
#endif
}

TEST_CASE("profile coarse speed keys reject lanes reserved keys and restart conflicts") {
    using tenriff::app::resolve_hispeed_key;
    const auto f5 = tenriff::config::KeycodeMap::to_keycode("F5").value_or(0);
    const auto f6 = tenriff::config::KeycodeMap::to_keycode("F6").value_or(0);
    const auto letter = tenriff::config::KeycodeMap::to_keycode("R").value_or(0);
    REQUIRE(f5 != 0); REQUIRE(f6 != 0); REQUIRE(letter != 0);
    CHECK(resolve_hispeed_key("F5", {}, 0) == f5);
    CHECK(resolve_hispeed_key("F6", {}, 0) == f6);
    CHECK(resolve_hispeed_key("R", {}, 0) == letter);
    CHECK(resolve_hispeed_key("R", {{letter, 1}}, 0) == 0);
    CHECK(resolve_hispeed_key("R", {}, letter) == 0);
    CHECK(resolve_hispeed_key("F9", {}, 0) != 0); // A freed function key is assignable.
    CHECK(resolve_hispeed_key("UnknownKey", {}, 0) == 0);
    CHECK(resolve_hispeed_key("", {}, 0) == 0);
}


#include "ui/FunctionKeyLabels.h"
TEST_CASE("function shortcuts resolve collisions and remap authored hints without cascading") {
    using namespace tenriff;
    config::UiConfig ui;
    config::function_key(ui, 1) = "F12";
    config::function_key(ui, 9) = "Q";
    auto resolved = app::resolve_function_keys(ui);
    CHECK(resolved[0] == config::KeycodeMap::to_keycode("F12").value());
    CHECK(resolved[8] == config::KeycodeMap::to_keycode("Q").value());
    CHECK(ui::function_key_labels("F1 / F9 / F10 / F11; F1로; AF1 F12", config::function_key_labels(ui)) ==
          "F12 / Q / F10 / F11; F12로; AF1 F12");
    config::function_key(ui, 2) = "F1";
    CHECK(ui::function_key_labels("F1 F2", config::function_key_labels(ui)) == "F12 F1");
    config::function_key(ui, 3).clear();
    CHECK(ui::function_key_labels("F3", config::function_key_labels(ui)) == "--");
    config::function_key(ui, 2) = "Q";
    resolved = app::resolve_function_keys(ui);
    CHECK(resolved[1] == 0); CHECK(resolved[8] == 0);
    config::function_key(ui, 2) = "Escape";
    CHECK(app::resolve_function_keys(ui)[1] == 0);
    CHECK(app::resolve_function_keys(ui, {{config::KeycodeMap::to_keycode("F12").value(), 1}})[0] == 0);
    ui.instant_restart_key = "F12";
    CHECK(app::resolve_function_keys(ui)[0] == 0);
    CHECK(app::profile_setup::function_for_row(app::profile_setup::kHispeedDecreaseKeyRow) == 5);
    CHECK(app::profile_setup::function_for_row(app::profile_setup::kFunctionKeysLastRow) == 11);
}


TEST_CASE("profile gameplay cursor toggles without altering unrelated UI settings") {
    tenriff::config::UiConfig ui;
    const auto language = ui.language;
    const bool before = ui.show_cursor_in_gameplay;
    CHECK(adjust_presentation_setting(ui, kGameplayCursorRow, 1));
    CHECK(ui.show_cursor_in_gameplay != before);
    CHECK_FALSE(adjust_presentation_setting(ui, kGameplayCursorRow, 0));
    CHECK(adjust_presentation_setting(ui, kGameplayCursorRow, -1));
    CHECK(ui.show_cursor_in_gameplay == before);
    CHECK(ui.language == language);
    CHECK_FALSE(supports_hold_adjustment(kGameplayCursorRow));
}
