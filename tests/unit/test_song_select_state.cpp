#include "doctest/doctest.h"

#include <unordered_map>
#include <vector>

#include "app/SongIndex.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/SongSelectState.h"

using tenriff::app::SongEntry;
using tenriff::app::SongSelectState;
using tenriff::app::menu_song_select::build_song_collection_membership_lookup;
using tenriff::app::menu_song_select::build_song_membership_set;
using tenriff::app::menu_song_select::count_song_membership_matches;
using tenriff::app::menu_song_select::song_collection_membership_contains;
using tenriff::app::resolve_selected_song_index;
using tenriff::app::sync_song_select_state;

TEST_CASE("song select state falls back to song view when sources disappear") {
    SongSelectState state;
    state.selected_song = 7;
    state.selected_source = 4;
    state.showing_sources = true;

    sync_song_select_state(state, 3, 0);

    CHECK(state.selected_song == 2);
    CHECK(state.selected_source == 0);
    CHECK_FALSE(state.showing_sources);
}

TEST_CASE("song select state clamps song and source indices into current bounds") {
    SongSelectState state;
    state.selected_song = -4;
    state.selected_source = 99;
    state.showing_sources = false;

    sync_song_select_state(state, 5, 3);

    CHECK(state.selected_song == 0);
    CHECK(state.selected_source == 2);
    CHECK_FALSE(state.showing_sources);
}

TEST_CASE("song select preserved path wins when rebuilding visible list") {
    std::vector<SongEntry> songs(3);
    songs[0].path = "one.bms";
    songs[1].path = "two.bms";
    songs[2].path = "three.bms";

    const std::string preserved = "three.bms";
    CHECK(resolve_selected_song_index(songs, 0, &preserved) == 2);
}

TEST_CASE("song select preserved path missing falls back to clamped selection") {
    std::vector<SongEntry> songs(2);
    songs[0].path = "one.bms";
    songs[1].path = "two.bms";

    const std::string preserved = "missing.bms";
    CHECK(resolve_selected_song_index(songs, 5, &preserved) == 1);
    CHECK(resolve_selected_song_index(songs, -3, &preserved) == 0);
}

TEST_CASE("song membership helpers recompute favorite counts after toggles and indexed-song changes") {
    auto favorites = build_song_membership_set({"songA", "songB"});
    std::vector<std::string> indexed_song_keys = {"songA", "songC"};

    CHECK(count_song_membership_matches(indexed_song_keys, favorites) == 1);

    favorites = build_song_membership_set({"songA", "songB", "songC"});
    CHECK(count_song_membership_matches(indexed_song_keys, favorites) == 2);

    indexed_song_keys = {"songB", "songC", "songD"};
    CHECK(count_song_membership_matches(indexed_song_keys, favorites) == 2);
}

TEST_CASE("song collection membership helpers reflect collection membership and filter changes") {
    std::unordered_map<std::string, std::vector<std::string>> collections = {
        {"Practice", {"songA"}},
        {"Favorites+", {"songB", "songC"}},
    };

    auto lookup = build_song_collection_membership_lookup(collections);
    CHECK(song_collection_membership_contains(lookup, "Practice", "songA"));
    CHECK_FALSE(song_collection_membership_contains(lookup, "Practice", "songB"));
    CHECK(song_collection_membership_contains(lookup, "Favorites+", "songC"));

    collections["Practice"].push_back("songB");
    lookup = build_song_collection_membership_lookup(collections);
    CHECK(song_collection_membership_contains(lookup, "Practice", "songB"));

    collections.erase("Practice");
    lookup = build_song_collection_membership_lookup(collections);
    CHECK_FALSE(song_collection_membership_contains(lookup, "Practice", "songA"));
}
