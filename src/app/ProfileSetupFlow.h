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
inline constexpr int kOptionsHubRowCount = 9;
inline constexpr int kOptionsProfileSetupRow = 7;
inline constexpr int kOptionsBackRow = 8;

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
