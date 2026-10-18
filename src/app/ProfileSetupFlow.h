#pragma once

#include <array>
#include <string_view>

#include "config/Config.h"
#include "ui/Localization.h"

namespace tenriff::app::profile_setup {

enum class Entry {
    FirstRun,
    Options,
};

enum class Destination {
    Stay,
    SongSelect,
    Title,
    OptionsHub,
};

inline constexpr int kLanguageRow = 0;
inline constexpr int kMenuFontSizeRow = 1;
inline constexpr int kUiFontRow = 2;
inline constexpr int kUiFontFileRow = 3;
inline constexpr int kGameplayCursorRow = 4;
inline constexpr int kSongsFolderRow = 5;
inline constexpr int kGaugeRow = 6;
inline constexpr int kRateRow = 7;
inline constexpr int kVisualLatencyRow = 8;
inline constexpr int kKeysoundRow = 9;
inline constexpr int kBackendRow = 10;
inline constexpr int kNicknameRow = 11;
inline constexpr int kAvatarRow = 12;
inline constexpr int kClearAvatarRow = 13;
inline constexpr int kExportSettingsRow = 14;
inline constexpr int kImportSettingsRow = 15;
inline constexpr int kSkipStartCountdownRow = 16;
inline constexpr int kInstantRestartKeyRow = 17;
inline constexpr int kHispeedDecreaseKeyRow = 18;
inline constexpr int kHispeedIncreaseKeyRow = 19;
inline constexpr int kFunctionKeysFirstRow = 20;
inline constexpr int kFunctionKeysLastRow = 28;
inline constexpr int kDoneRow = 29;
inline constexpr int kFirstRunSkipRow = 30;

[[nodiscard]] inline constexpr int function_for_row(int row) {
    if (row == kHispeedDecreaseKeyRow) return 5;
    if (row == kHispeedIncreaseKeyRow) return 6;
    if (row < kFunctionKeysFirstRow || row > kFunctionKeysLastRow) return 0;
    const int slot = row - kFunctionKeysFirstRow;
    return slot < 4 ? slot + 1 : slot + 3;
}

[[nodiscard]] inline constexpr Entry entry(bool first_run_profile) {
    return first_run_profile ? Entry::FirstRun : Entry::Options;
}

[[nodiscard]] inline constexpr int row_count(Entry source) {
    return source == Entry::FirstRun ? kFirstRunSkipRow + 1 : kDoneRow + 1;
}

[[nodiscard]] inline constexpr bool supports_hold_adjustment(int row) {
    return row == kRateRow || row == kVisualLatencyRow;
}

// Both first-run and reopened profiles share the same presentation controls.
// Keep this independent from window/input handling so cycling can be checked directly.
[[nodiscard]] inline bool adjust_presentation_setting(config::UiConfig& settings,
                                                       int row,
                                                       int direction) {
    if (direction == 0) return false;
    if (row == kGameplayCursorRow) {
        settings.show_cursor_in_gameplay = !settings.show_cursor_in_gameplay;
        return true;
    }
    if (row == kLanguageRow) {
        settings.language = ui::language_token(
            ui::cycle_language(ui::language_from_token(settings.language), direction));
        return true;
    }
    if (row == kMenuFontSizeRow) {
        constexpr std::array<std::string_view, 3> sizes{"normal", "large", "extra_large"};
        const std::string current = config::normalize_menu_font_size_token(settings.menu_font_size);
        const int index = current == "large" ? 1 : current == "extra_large" ? 2 : 0;
        settings.menu_font_size = sizes[static_cast<std::size_t>((index + (direction < 0 ? 2 : 1)) % 3)];
        return true;
    }
    return false;
}

[[nodiscard]] inline constexpr Destination enter_destination(Entry source, int cursor) {
    if (cursor == kDoneRow) {
        return source == Entry::FirstRun ? Destination::SongSelect : Destination::OptionsHub;
    }
    if (source == Entry::FirstRun && cursor == kFirstRunSkipRow) {
        return Destination::Title;
    }
    return Destination::Stay;
}

[[nodiscard]] inline constexpr Destination cancel_destination(Entry source) {
    return source == Entry::FirstRun ? Destination::Title : Destination::OptionsHub;
}

}  // namespace tenriff::app::profile_setup
