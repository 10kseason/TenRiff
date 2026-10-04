#include "doctest/doctest.h"

#include "app/ProfileSetupFlow.h"

using namespace tenriff::app::profile_setup;

TEST_CASE("first-run profile setup keeps its original song-select and title exits") {
    CHECK(row_count(Entry::FirstRun) == 15);
    CHECK(kLanguageRow == 0);
    CHECK(kMenuFontSizeRow == 1);
    CHECK(kBackendRow == kNicknameRow - 1);
    CHECK(kNicknameRow == kAvatarRow - 1);
    CHECK(kAvatarRow == kClearAvatarRow - 1);
    CHECK(kClearAvatarRow == kExportSettingsRow - 1);
    CHECK(kImportSettingsRow == kDoneRow - 1);
    CHECK(enter_destination(Entry::FirstRun, kDoneRow) == Destination::SongSelect);
    CHECK(enter_destination(Entry::FirstRun, kFirstRunSkipRow) == Destination::Title);
    CHECK(cancel_destination(Entry::FirstRun) == Destination::Title);
}

TEST_CASE("reopened profile setup returns to the options hub") {
    CHECK(row_count(Entry::Options) == 14);
    CHECK(enter_destination(Entry::Options, kDoneRow) == Destination::OptionsHub);
    CHECK(enter_destination(Entry::Options, 2) == Destination::Stay);
    CHECK(cancel_destination(Entry::Options) == Destination::OptionsHub);
}

TEST_CASE("profile presentation controls cycle all languages and text sizes immediately") {
    tenriff::config::UiConfig settings;
    CHECK(adjust_presentation_setting(settings, kLanguageRow, -1));
    CHECK(settings.language == "ja");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
    CHECK(settings.language == "en");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
    CHECK(settings.language == "ko");
    CHECK(adjust_presentation_setting(settings, kLanguageRow, 1));
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
