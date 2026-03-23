#include "doctest/doctest.h"

#include <algorithm>
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
using tenriff::app::menu_song_select::song_entry_less_by_artist_asc;
using tenriff::app::menu_song_select::song_entry_matches_search;
using tenriff::app::menu_song_select::song_group_folder_label;
using tenriff::app::menu_song_select::song_group_level_key;
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

TEST_CASE("song search matches artist names directly") {
    SongEntry entry;
    entry.title = "Blue Archive";
    entry.artist = "Mitsukiyo";
    entry.path = "Songs/blue_archive/chart.bms";

    CHECK(song_entry_matches_search(entry, "mitsu"));
    CHECK(song_entry_matches_search(entry, "MITSUKIYO"));
    CHECK_FALSE(song_entry_matches_search(entry, "xi"));
}

TEST_CASE("artist sort orders by artist then title") {
    SongEntry beta_first;
    beta_first.artist = "Beta";
    beta_first.title = "A Song";
    beta_first.path = "beta_a.bms";

    SongEntry beta_second;
    beta_second.artist = "Beta";
    beta_second.title = "B Song";
    beta_second.path = "beta_b.bms";

    SongEntry alpha;
    alpha.artist = "Alpha";
    alpha.title = "Z Song";
    alpha.path = "alpha_z.bms";

    std::vector<SongEntry> entries = {beta_second, alpha, beta_first};
    std::stable_sort(entries.begin(), entries.end(), song_entry_less_by_artist_asc);

    CHECK(entries[0].artist == "Alpha");
    CHECK(entries[0].title == "Z Song");
    CHECK(entries[1].artist == "Beta");
    CHECK(entries[1].title == "A Song");
    CHECK(entries[2].artist == "Beta");
    CHECK(entries[2].title == "B Song");
}

TEST_CASE("song folder group label uses the parent directory name") {
    SongEntry entry;
    entry.path = "Songs/Pack Alpha/Sub Folder/chart.bms";

    CHECK(song_group_folder_label(entry) == "Sub Folder");
}

TEST_CASE("song level group key keeps numeric ordering stable") {
    SongEntry low;
    low.level = 2;

    SongEntry high;
    high.level = 10;

    SongEntry unknown;
    unknown.level = 0;

    CHECK(song_group_level_key(low) < song_group_level_key(high));
    CHECK(song_group_level_key(high) < song_group_level_key(unknown));
}
