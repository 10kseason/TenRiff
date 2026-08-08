#include "doctest/doctest.h"

#include "app/ProfileSetupFlow.h"

using namespace tenriff::app::profile_setup;

TEST_CASE("first-run profile setup keeps its original song-select and title exits") {
    CHECK(row_count(Entry::FirstRun) == 11);
    CHECK(kBackendRow == kNicknameRow - 1);
    CHECK(kNicknameRow == kAvatarRow - 1);
    CHECK(kAvatarRow == kClearAvatarRow - 1);
    CHECK(kClearAvatarRow == kDoneRow - 1);
    CHECK(enter_destination(Entry::FirstRun, kDoneRow) == Destination::SongSelect);
    CHECK(enter_destination(Entry::FirstRun, kFirstRunSkipRow) == Destination::Title);
    CHECK(cancel_destination(Entry::FirstRun) == Destination::Title);
}

TEST_CASE("reopened profile setup returns to the options hub") {
    CHECK(row_count(Entry::Options) == 10);
    CHECK(enter_destination(Entry::Options, kDoneRow) == Destination::OptionsHub);
    CHECK(enter_destination(Entry::Options, 2) == Destination::Stay);
    CHECK(cancel_destination(Entry::Options) == Destination::OptionsHub);
}

TEST_CASE("options hub uses an eight-card four-by-two grid") {
    CHECK(kOptionsHubRowCount == 8);
    CHECK(kOptionsKeyModeRow == 0);
    CHECK(kOptionsProfileSetupRow == 7);
    CHECK(move_options_grid_cursor(0, 1, 0) == 1);
    CHECK(move_options_grid_cursor(3, 1, 0) == 3);
    CHECK(move_options_grid_cursor(2, 0, 1) == 6);
    CHECK(move_options_grid_cursor(6, 0, -1) == 2);
}
