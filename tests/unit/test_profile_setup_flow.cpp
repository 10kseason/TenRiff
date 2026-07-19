#include "doctest/doctest.h"

#include "app/ProfileSetupFlow.h"

using namespace tenriff::app::profile_setup;

TEST_CASE("first-run profile setup keeps its original song-select and title exits") {
    CHECK(row_count(Entry::FirstRun) == 8);
    CHECK(kBackendRow == kDoneRow - 1);
    CHECK(enter_destination(Entry::FirstRun, kDoneRow) == Destination::SongSelect);
    CHECK(enter_destination(Entry::FirstRun, kFirstRunSkipRow) == Destination::Title);
    CHECK(cancel_destination(Entry::FirstRun) == Destination::Title);
}

TEST_CASE("reopened profile setup returns to the options hub") {
    CHECK(row_count(Entry::Options) == 7);
    CHECK(enter_destination(Entry::Options, kDoneRow) == Destination::OptionsHub);
    CHECK(enter_destination(Entry::Options, 2) == Destination::Stay);
    CHECK(cancel_destination(Entry::Options) == Destination::OptionsHub);
}

TEST_CASE("options hub profile row remains before the final back row") {
    CHECK(kOptionsProfileSetupRow == 7);
    CHECK(kOptionsBackRow == kOptionsHubRowCount - 1);
}
