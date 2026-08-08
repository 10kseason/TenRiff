#pragma once

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

inline constexpr int kBackendRow = 5;
inline constexpr int kNicknameRow = 6;
inline constexpr int kAvatarRow = 7;
inline constexpr int kClearAvatarRow = 8;
inline constexpr int kDoneRow = 9;
inline constexpr int kFirstRunSkipRow = 10;
inline constexpr int kOptionsHubRowCount = 8;
inline constexpr int kOptionsKeyModeRow = 0;
inline constexpr int kOptionsKeymapRow = 1;
inline constexpr int kOptionsSkinsRow = 2;
inline constexpr int kOptionsGraphicsRow = 3;
inline constexpr int kOptionsAudioRow = 4;
inline constexpr int kOptionsInputRow = 5;
inline constexpr int kOptionsCalibrationRow = 6;
inline constexpr int kOptionsProfileSetupRow = 7;

[[nodiscard]] inline constexpr int move_options_grid_cursor(int cursor, int dx, int dy) {
    constexpr int columns = 4;
    cursor = cursor < 0 ? 0 : (cursor >= kOptionsHubRowCount ? kOptionsHubRowCount - 1 : cursor);
    int column = cursor % columns;
    int row = cursor / columns;
    column = column + dx < 0 ? 0 : (column + dx >= columns ? columns - 1 : column + dx);
    row = row + dy < 0 ? 0 : (row + dy >= 2 ? 1 : row + dy);
    return row * columns + column;
}

[[nodiscard]] inline constexpr Entry entry(bool first_run_profile) {
    return first_run_profile ? Entry::FirstRun : Entry::Options;
}

[[nodiscard]] inline constexpr int row_count(Entry source) {
    return source == Entry::FirstRun ? 11 : 10;
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
