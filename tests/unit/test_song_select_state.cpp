#include "doctest/doctest.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <vector>

#include "app/SongIndex.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/SongSelectState.h"
#include "config/Config.h"

using tenriff::app::SongEntry;
using tenriff::app::SongSelectState;
using tenriff::app::menu_song_select::build_song_collection_membership_lookup;
using tenriff::app::menu_song_select::build_song_membership_set;
using tenriff::app::menu_song_select::count_song_membership_matches;
using tenriff::app::menu_song_select::song_collection_membership_contains;
using tenriff::app::menu_song_select::song_difficulty_label;
using tenriff::app::menu_song_select::song_difficulty_is_aery_five_key;
using tenriff::app::menu_song_select::song_difficulty_prefix_key_count;
using tenriff::app::menu_song_select::song_entry_less_by_difficulty_asc;
using tenriff::app::menu_song_select::song_entry_less_by_difficulty_desc;
using tenriff::app::menu_song_select::song_entry_matches_level_filter;
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

TEST_CASE("song search lv expressions use inclusive native level boundaries and ignore case") {
    SongEntry entry;
    entry.format = "bms";
    for (int level = 15; level <= 21; ++level) {
        entry.level = entry.native_level = level;
        CHECK(song_entry_matches_search(entry, "lv:16to20") == (level >= 16 && level <= 20));
        CHECK(song_entry_matches_search(entry, "LV:16TO20") == (level >= 16 && level <= 20));
        CHECK(song_entry_matches_search(entry, "Lv:16") == (level == 16));
        CHECK(song_entry_matches_search(entry, "lv:19-") == (level <= 19));
        CHECK_FALSE(song_entry_matches_search(entry, "lV:18+"));
    }
    entry.level = 16;
    CHECK(song_entry_matches_search(entry, "lv:016"));
    CHECK(song_entry_matches_search(entry, "lv:16to16"));
    entry.level = std::numeric_limits<int>::max();
    CHECK(song_entry_matches_search(entry, "lv:2147483647"));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:2147483647+"));
    CHECK(song_entry_matches_search(entry, "lv:2147483647-"));
}

TEST_CASE("song search lv expressions combine with all existing text terms and bounds") {
    SongEntry entry;
    entry.title = "Blue Archive";
    entry.artist = "Mitsukiyo";
    entry.path = "Songs/blue_archive/chart.bms";
    entry.chart_name = "Another";
    entry.format = "bms";
    entry.key_count = 7;
    entry.level = 18;

    CHECK(song_entry_matches_search(entry, "  BLUE\tLv:16tO20 mitsu "));
    CHECK(song_entry_matches_search(entry, "lv:16to20 lv:20-"));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:18+ lv:17-"));
    CHECK_FALSE(song_entry_matches_search(entry, "blue lv:19"));
    CHECK_FALSE(song_entry_matches_search(entry, "xi lv:18"));
    CHECK_FALSE(song_entry_matches_search(entry, "blue mitsu 7k another bms lv18"));
    CHECK_FALSE(song_entry_matches_search(entry, "blue mitsu 5k"));
    CHECK(song_entry_matches_search(entry, " \t "));

    // Search adds an intersection; existing sidebar bounds still apply.
    CHECK(song_entry_matches_search(entry, "lv:16to20"));
    CHECK(song_entry_matches_level_filter(entry, 18, 19));
    CHECK_FALSE(song_entry_matches_level_filter(entry, 19, 20));
}

TEST_CASE("song search lv expressions use the selected table without native fallback") {
    SongEntry entry;
    entry.level = entry.native_level = 50;
    entry.difficulty_table_name = "Selected Table";
    entry.difficulty_table_symbol = "st";
    for (const std::string label : {"18", "018", "18+", "st18", "sl18+", u8"★18", "  LV 18  "}) {
        entry.difficulty_table_level = label;
        CHECK(song_entry_matches_search(entry, "lv:16to20", true));
        CHECK(song_entry_matches_search(entry, "Lv:18", true) == (label.find('+') == std::string::npos));
        CHECK(song_entry_matches_search(entry, "lv:19-", true));
        CHECK(song_entry_matches_search(entry, "lv:18+", true) == (label.find('+') != std::string::npos));
        CHECK_FALSE(song_entry_matches_search(entry, "lv:50", true));
        CHECK_FALSE(song_entry_matches_search(entry, "lv:16to20", false));
        CHECK(song_entry_matches_search(entry, "lv:50", false));
    }
    for (const std::string label : {"", "?", "EX", "18-19", "18/19", "18.5", "18A", "-18", u8"−18", "99999999999999"}) {
        entry.difficulty_table_level = label;
        CHECK_FALSE(song_entry_matches_search(entry, "lv:0+", true));
        CHECK_FALSE(song_entry_matches_search(entry, "lv:50", true));
        CHECK(song_entry_matches_search(entry, "lv:50", false));
    }
    entry.difficulty_table_level = "19";
    CHECK_FALSE(song_entry_matches_search(entry, "lv:18", true));
    CHECK(song_entry_matches_search(entry, "lv:19", true));
}

TEST_CASE("song search lv zero matches authored table zero but excludes unknown native levels") {
    SongEntry entry;
    entry.level = 0;
    entry.difficulty_table_level = "st0";
    CHECK(song_entry_matches_search(entry, "lv:0", true));
    CHECK(song_entry_matches_search(entry, "lv:0-", true));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:0+", true));
    CHECK(song_entry_matches_search(entry, "lv:0to1", true));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:1+", true));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:0", false));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:0+", false));
    entry.difficulty_table_level = "1";
    CHECK_FALSE(song_entry_matches_search(entry, "lv:0", true));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:0-", true));
    entry.level = -1;
    CHECK_FALSE(song_entry_matches_search(entry, "lv:0+", false));
}

TEST_CASE("song search malformed lv expressions retain conservative literal matching") {
    SongEntry entry;
    entry.level = 18;
    for (const std::string query : {"lv:", "lv:abc", "lv:-18", "lv:+18", "lv:18.5", "lv:18++",
                                    "lv:20to16", "lv:18to", "lv:to18", "lv:16to20to24", "lv:18+tail",
                                    "lv:2147483648", "lv:0to2147483648", "lv:999999999999999999999+"}) {
        entry.title = "Unrelated Song";
        CHECK_FALSE(song_entry_matches_search(entry, query));
        entry.title = "Literal " + query;
        CHECK(song_entry_matches_search(entry, query));
    }
    entry.title = "prefix-lv:18";
    entry.level = 20;
    CHECK(song_entry_matches_search(entry, "prefix-lv:18"));
    CHECK_FALSE(song_entry_matches_search(entry, "lv:18"));
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

TEST_CASE("difficulty-table metadata drives labels groups sorting and numeric filters") {
    SongEntry table_low;
    table_low.title = "Table Low";
    table_low.level = 18;
    table_low.difficulty_table_name = "Local Satellite";
    table_low.difficulty_table_symbol = "sl";
    table_low.difficulty_table_level = "2+";
    table_low.difficulty_table_order = 1;

    SongEntry table_high = table_low;
    table_high.title = "Table High";
    table_high.difficulty_table_level = "10";
    table_high.difficulty_table_order = 4;

    SongEntry native;
    native.title = "Native";
    native.level = 1;

    CHECK(song_difficulty_label(table_low) == "sl2+");
    CHECK(song_group_level_key(table_low) < song_group_level_key(native));
    CHECK(song_entry_matches_level_filter(table_low, 2, 2, true));
    CHECK_FALSE(song_entry_matches_level_filter(table_low, 3, 50, true));

    std::vector<SongEntry> ascending = {native, table_high, table_low};
    std::stable_sort(ascending.begin(), ascending.end(), song_entry_less_by_difficulty_asc);
    CHECK(ascending[0].title == "Table Low");
    CHECK(ascending[1].title == "Table High");
    CHECK(ascending[2].title == "Native");

    std::stable_sort(ascending.begin(), ascending.end(), song_entry_less_by_difficulty_desc);
    CHECK(ascending[0].title == "Table High");
    CHECK(ascending[1].title == "Table Low");
    CHECK(ascending[2].title == "Native");
}

TEST_CASE("only the built-in 5K Aery table opts into a mode-colored circled-five prefix") {
    constexpr std::string_view aery_url = "https://asumatoki.kr/table/aery/header.json";
    SongEntry entry;
    entry.key_count = 5;
    entry.difficulty_table_symbol = "⑤";
    entry.difficulty_table_level = "LEVEL 13";
    CHECK(song_difficulty_label(entry) == "⑤LEVEL 13");
    CHECK(song_difficulty_is_aery_five_key(entry, aery_url));
    CHECK_FALSE(song_difficulty_is_aery_five_key(entry, "https://asumatoki.kr/table/aery7/header.json"));
    CHECK_FALSE(song_difficulty_is_aery_five_key(entry, "https://example.com/other-five-key/header.json"));
    CHECK_FALSE(song_difficulty_is_aery_five_key(entry, ""));
    entry.key_count = 7;
    CHECK_FALSE(song_difficulty_is_aery_five_key(entry, aery_url));
    entry.key_count = 5;
    entry.difficulty_table_symbol = "st";
    CHECK_FALSE(song_difficulty_is_aery_five_key(entry, aery_url));
    entry.difficulty_table_symbol = "⑤";
    entry.difficulty_table_level.clear();
    CHECK_FALSE(song_difficulty_is_aery_five_key(entry, aery_url));
}

TEST_CASE("registered difficulty table prefixes require the exact table and symbol") {
    struct PrefixCase {
        std::string_view url;
        std::string_view symbol;
        int prefix_key_count;
    };
    constexpr PrefixCase cases[]{
        {"https://classmaterma.github.io/4UE/table.html", "4UE", 4},
        {"https://classmaterma.github.io/UE/table.html", "6UE", 6},
        {"https://classmaterma.github.io/8UE/table.html", "8UE", 8},
        {"https://calc.10k-revive.cloud/table.html", "Ⓡ", 10},
    };
    for (const auto& expected : cases) {
        SongEntry entry;
        entry.difficulty_table_symbol = expected.symbol;
        entry.difficulty_table_level = "13+";
        for (const int chart_keys : {0, 4, 5, 6, 8, 10, 16}) {
            entry.key_count = chart_keys;
            CHECK(song_difficulty_prefix_key_count(entry, expected.url) == expected.prefix_key_count);
        }
        CHECK(song_difficulty_label(entry) == std::string(expected.symbol) + "13+");
        for (const auto& other : cases) {
            if (other.url != expected.url) {
                CHECK(song_difficulty_prefix_key_count(entry, other.url) == 0);
            }
        }
        CHECK(song_difficulty_prefix_key_count(entry, "") == 0);
        CHECK(song_difficulty_prefix_key_count(entry, "https://example.com/custom/table.html") == 0);
        CHECK(song_difficulty_prefix_key_count(entry, std::string(expected.url) + "?custom=1") == 0);
        CHECK(song_difficulty_prefix_key_count(entry, "https://asumatoki.kr/table/aery/header.json") == 0);
        entry.difficulty_table_symbol = std::string(expected.symbol) + "LEVEL";
        CHECK(song_difficulty_prefix_key_count(entry, expected.url) == 0);
        entry.difficulty_table_symbol = " " + std::string(expected.symbol);
        CHECK(song_difficulty_prefix_key_count(entry, expected.url) == 0);
        entry.difficulty_table_symbol.clear();
        CHECK(song_difficulty_prefix_key_count(entry, expected.url) == 0);
        entry.difficulty_table_symbol = expected.symbol;
        entry.difficulty_table_level.clear();
        CHECK(song_difficulty_prefix_key_count(entry, expected.url) == 0);
    }
}

TEST_CASE("numeric chart and table difficulties cannot select a colored table prefix") {
    constexpr std::string_view ue_url = "https://classmaterma.github.io/4UE/table.html";
    SongEntry entry;
    CHECK(song_difficulty_prefix_key_count(entry, "") == 0);
    CHECK(song_difficulty_prefix_key_count(entry, ue_url) == 0);
    for (const int level : {4, 6, 8, 10}) {
        entry.level = level;
        entry.key_count = level;
        CHECK(song_difficulty_prefix_key_count(entry, ue_url) == 0);
        entry.difficulty_table_level = std::to_string(level);
        entry.difficulty_table_symbol = "LV ";
        CHECK(song_difficulty_prefix_key_count(entry, ue_url) == 0);
        entry.difficulty_table_symbol = std::to_string(level);
        CHECK(song_difficulty_prefix_key_count(entry, ue_url) == 0);
        entry.difficulty_table_symbol = "4UE";
        CHECK(song_difficulty_prefix_key_count(entry, ue_url) == 4);
        CHECK(song_difficulty_prefix_key_count(entry, "") == 0);
        entry.difficulty_table_symbol.clear();
        entry.difficulty_table_level.clear();
    }

    entry.key_count = 5;
    entry.difficulty_table_symbol = "⑤";
    entry.difficulty_table_level = "LEVEL 10";
    constexpr std::string_view aery_url = "https://asumatoki.kr/table/aery/header.json";
    CHECK(song_difficulty_is_aery_five_key(entry, aery_url));
    CHECK(song_difficulty_prefix_key_count(entry, aery_url) == 0);
}

TEST_CASE("difficulty table levels without level_order use natural numeric ordering") {
    SongEntry one;
    one.title = "one";
    one.difficulty_table_level = "1";
    SongEntry ten = one;
    ten.title = "ten";
    ten.difficulty_table_level = "10";
    SongEntry two = one;
    two.title = "two";
    two.difficulty_table_level = "2";

    std::vector<SongEntry> entries{ten, two, one};
    std::stable_sort(entries.begin(), entries.end(), song_entry_less_by_difficulty_asc);
    CHECK(entries[0].difficulty_table_level == "1");
    CHECK(entries[1].difficulty_table_level == "2");
    CHECK(entries[2].difficulty_table_level == "10");
    CHECK(song_group_level_key(two) < song_group_level_key(ten));

    one.difficulty_table_level = "st1";
    two.difficulty_table_level = "st2";
    ten.difficulty_table_level = "st10";
    entries = {ten, two, one};
    std::stable_sort(entries.begin(), entries.end(), song_entry_less_by_difficulty_asc);
    CHECK(entries[0].difficulty_table_level == "st1");
    CHECK(entries[1].difficulty_table_level == "st2");
    CHECK(entries[2].difficulty_table_level == "st10");
}

TEST_CASE("selected table numeric bounds follow displayed labels and never fall back to native LV") {
    SongEntry entry;
    entry.level = 50;
    entry.difficulty_table_symbol = "st";
    for (const std::string label : {"12", "012", "12+", "st12", "sl12+", u8"★12", "  LV 12  "}) {
        entry.difficulty_table_level = label;
        CHECK(song_entry_matches_level_filter(entry, 12, 12, true));
        CHECK_FALSE(song_entry_matches_level_filter(entry, 13, 0, true));
        CHECK_FALSE(song_entry_matches_level_filter(entry, 0, 11, true));
        CHECK(song_entry_matches_level_filter(entry, 50, 50, false));
    }
    for (const std::string label : {"", "?", "A", "EX", "12-13", "12/13", "12.5", "12A", "-12", u8"−12", "99999999999999"}) {
        entry.difficulty_table_level = label;
        CHECK_FALSE(song_entry_matches_level_filter(entry, 1, 99, true));
        CHECK(song_entry_matches_level_filter(entry, 0, 0, true));
        CHECK(song_entry_matches_level_filter(entry, 50, 50, false));
    }
    entry.difficulty_table_level = "st0";
    CHECK(song_entry_matches_level_filter(entry, 0, 12, true));
    CHECK_FALSE(song_entry_matches_level_filter(entry, 12, 0, true));
    entry.difficulty_table_level = "st13";
    CHECK(song_entry_matches_level_filter(entry, 12, 0, true));
    CHECK_FALSE(song_entry_matches_level_filter(entry, 12, 12, true));
    entry.level = 0;
    CHECK_FALSE(song_entry_matches_level_filter(entry, 0, 12, false));
}

TEST_CASE("native LV restores cached labels filters and sorting after a difficulty table was selected") {
    namespace fs = std::filesystem;
    using namespace tenriff::app;

    struct ScratchDirectory {
        fs::path path;
        ~ScratchDirectory() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    };
    for (const bool calculate_difficulty : {false, true}) {
        ScratchDirectory scratch{fs::temp_directory_path() /
            ("tenriff-native-lv-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
        REQUIRE(fs::create_directory(scratch.path));
        const auto header = scratch.path / "header.json";
        {
            std::ofstream body(scratch.path / "table.json");
            body << R"([{"md5":"11111111111111111111111111111111","level":"9"},)"
                    R"({"md5":"22222222222222222222222222222222","level":"1"}])";
            REQUIRE(body.good());
            std::ofstream file(header);
            file << R"({"name":"Test Table","symbol":"T","level_order":["1","9"],"data_url":"table.json"})";
            REQUIRE(file.good());
        }

        SongIndexOptions options;
        options.calculate_difficulty = calculate_difficulty;
        options.difficulty_table_path = header.u8string();
        const int low_level = options.calculate_difficulty ? 5 : 2;
        const int high_level = options.calculate_difficulty ? 20 : 12;
        SongEntry low;
        low.path = "absent-low.bms";
        low.title = "Low Native";
        low.format = "bms";
        low.key_count = 10;
        low.level = low.native_level = low_level;
        low.md5 = "11111111111111111111111111111111";
        SongEntry high = low;
        high.path = "absent-high.bms";
        high.title = "High Native";
        high.level = high.native_level = high_level;
        high.md5 = "22222222222222222222222222222222";
        SongIndex index{{low, high}};
        const auto cache_path = scratch.path / "song_index.json";
        std::string error;
        REQUIRE(save_song_index(cache_path.u8string(), index, options, &error));
        auto table_index = load_song_index(cache_path.u8string(), options);
        REQUIRE(table_index.success());
        REQUIRE(table_index.loaded_from_file);
        REQUIRE(table_index.index.entries.size() == 2);
        std::stable_sort(table_index.index.entries.begin(), table_index.index.entries.end(), song_entry_less_by_difficulty_asc);
        CHECK(table_index.index.entries[0].title == "High Native");
        CHECK(song_difficulty_label(table_index.index.entries[0]) == "T1");
        CHECK(song_entry_matches_search(table_index.index.entries[0], "lv:1", true));
        CHECK_FALSE(song_entry_matches_search(table_index.index.entries[1], "lv:1", true));
        REQUIRE(save_song_index(cache_path.u8string(), table_index.index, options, &error));

        // The native selection persists empty table fields. Loading that profile and
        // the existing cache must work even when no original charts are available.
        tenriff::config::ConfigLoader loader;
        auto profile = loader.defaults();
        profile.mode.calculate_song_index_difficulty = options.calculate_difficulty;
        profile.ui.difficulty_table_path.clear();
        profile.ui.difficulty_table_url.clear();
        const auto profile_dir = (scratch.path / "profile").u8string();
        REQUIRE(loader.save_profile(profile_dir, profile, &error));
        const auto restored_profile = loader.load_profile(profile_dir);
        REQUIRE(restored_profile.success());
        CHECK(restored_profile.config.ui.difficulty_table_path.empty());
        CHECK(restored_profile.config.ui.difficulty_table_url.empty());
        options.difficulty_table_path = restored_profile.config.ui.difficulty_table_path;
        options.calculate_difficulty = restored_profile.config.mode.calculate_song_index_difficulty;
        auto native = load_song_index(cache_path.u8string(), options);
        REQUIRE(native.success());
        REQUIRE(native.loaded_from_file);
        REQUIRE(native.index.entries.size() == 2);
        REQUIRE(native.warnings.empty());
        for (const auto& entry : native.index.entries) {
            CHECK(entry.level == entry.native_level);
            CHECK(entry.difficulty_table_name.empty());
            CHECK(entry.difficulty_table_symbol.empty());
            CHECK(entry.difficulty_table_level.empty());
            CHECK(entry.difficulty_table_order == -1);
        }
        std::stable_sort(native.index.entries.begin(), native.index.entries.end(), song_entry_less_by_difficulty_asc);
        CHECK(native.index.entries[0].title == "Low Native");
        CHECK(native.index.entries[1].title == "High Native");
        CHECK(song_difficulty_label(native.index.entries[0]) == "LV " + std::to_string(low_level));
        CHECK(song_difficulty_label(native.index.entries[1]) == "LV " + std::to_string(high_level));
        CHECK(song_entry_matches_level_filter(native.index.entries[0], low_level, low_level));
        CHECK_FALSE(song_entry_matches_level_filter(native.index.entries[1], low_level, low_level));
        CHECK(song_entry_matches_search(native.index.entries[0], "lv:" + std::to_string(low_level)));
        CHECK_FALSE(song_entry_matches_search(native.index.entries[1], "lv:" + std::to_string(low_level)));
        CHECK(song_group_level_key(native.index.entries[0]) < song_group_level_key(native.index.entries[1]));
        std::stable_sort(native.index.entries.begin(), native.index.entries.end(), song_entry_less_by_difficulty_desc);
    CHECK(native.index.entries[0].title == "High Native");
    }
}

TEST_CASE("ALL SONG with an active table excludes nonmembers and Native LV restores them") {
    using tenriff::app::menu_song_select::song_entry_matches_table_scope;
    SongEntry member, nonmember;
    member.difficulty_table_level = "?";
    member.difficulty_table_order = -1; // A valid unlisted/custom level.
    nonmember.native_level = 99;
    CHECK(song_entry_matches_table_scope(member, true, true));
    CHECK_FALSE(song_entry_matches_table_scope(nonmember, true, true));
    CHECK(song_entry_matches_table_scope(nonmember, true, false));
    CHECK(song_entry_matches_table_scope(nonmember, false, true));
    CHECK(nonmember.native_level == 99);
}
