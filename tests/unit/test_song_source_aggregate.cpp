#include "doctest/doctest.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "app/MenuSongUtils.h"
#include "app/SongIndexerThread.h"
#include "app/SongSourceAggregate.h"
#include "util/Utf8Compat.h"

#ifdef _WIN32
#include "app/MenuApp.h"
#include "app/MenuRecordUtils.h"
#include "app/ReplayVerifier.h"

namespace tenriff::app {
struct MenuAppSongSourceTestAccess {
    static void check_table_reapply(const std::filesystem::path& profile, bool all_sources) {
        namespace fs = std::filesystem;
        fs::create_directories(profile);
        const auto header = profile / "table-header.json";
        const auto data = profile / "data.json";
        std::ofstream(header) << R"({"name":"Live table","symbol":"T","data_url":"data.json","level_order":["1","2","9"]})";
        const auto write_data = [&](const char* level) {
            std::ofstream(data) << "[{\"md5\":\"0123456789abcdef0123456789abcdef\",\"level\":\""
                                << level << "\"}]";
        };
        write_data("1");
        auto menu = std::make_unique<MenuApp>();
        menu->profile_dir_ = profile.u8string();
        menu->songs_path_ = (profile / "songs").u8string();
        menu->config_.ui.all_song_sources = all_sources;
        menu->config_.mode.song_index_profile = "safe";
        SongEntry entry;
        entry.path = all_sources ? (profile / "songs/chart.bms").u8string() : "chart.bms";
        entry.title = "Retained chart";
        entry.format = "bms";
        entry.level = entry.native_level = 4;
        entry.md5 = "0123456789abcdef0123456789abcdef";
        entry.sha256 = std::string(64, 'a');
        entry.rating = 3.25;
        entry.note_count = 128;
        SongIndex index;
        index.entries = {entry};
        menu->update_song_list(std::move(index));
        const auto* storage = menu->indexed_songs_.data();
        const auto revision = menu->song_index_revision_;
        MenuApp::LocalPlayRecord history;
        history.result_path = "live-session-record";
        menu->local_play_records_.push_back(history);
        MenuApp::BestResultRecord best;
        best.has_value = true;
        best.best_score = 1234;
        menu->chart_best_results_["retained-record-key"] = best;
        menu->config_.ui.difficulty_table_path = header.u8string();
        menu->refresh_difficulty_table_view(false);
        REQUIRE(menu->indexed_songs_.size() == 1);
        CHECK(menu->indexed_songs_.data() == storage);
        CHECK(menu->indexed_songs_[0].difficulty_table_level == "1");
        CHECK(menu->indexed_songs_[0].path == entry.path);
        CHECK(menu->indexed_songs_[0].sha256 == entry.sha256);
        CHECK(menu->indexed_songs_[0].rating == entry.rating);
        CHECK(menu->indexed_songs_[0].note_count == entry.note_count);
        CHECK((menu->table_levels_ == std::vector<std::string>{"1", "2", "9"}));
        CHECK(menu->difficulty_table_display_name_ == "Live table");
        CHECK(menu->song_index_revision_ > revision);
        CHECK(menu->config_.ui.all_song_sources == all_sources);
        REQUIRE(menu->local_play_records_.size() == 1);
        CHECK(menu->local_play_records_[0].result_path == "live-session-record");
        CHECK(menu->chart_best_results_.at("retained-record-key").best_score == 1234);
        CHECK(menu->song_indexer_.is_idle());
        CHECK_FALSE(menu->difficulty_table_loading());
        write_data("2"); // Re-importing the same path must see the new body.
        menu->refresh_difficulty_table_view(false);
        CHECK(menu->indexed_songs_[0].difficulty_table_level == "2");
        menu->select_native_difficulty_levels();
        CHECK(menu->indexed_songs_.size() == 1);
        CHECK(menu->indexed_songs_[0].difficulty_table_level.empty());
        CHECK(menu->indexed_songs_[0].difficulty_table_order == -1);
        CHECK(menu->indexed_songs_[0].level == 4);
        CHECK(menu->table_levels_.empty());
        CHECK(menu->local_play_records_.size() == 1);
        CHECK(menu->chart_best_results_.at("retained-record-key").best_score == 1234);
    }
    static void check_cyclic_lobby_navigation() {
        auto menu = std::make_unique<MenuApp>();
        menu->song_select_view_ = MenuApp::SongSelectView::Songs;
        menu->config_.ui.difficulty_table_path.clear();
        menu->song_select_search_active_ = false;
        CHECK_FALSE(menu->move_song_select_selection(-1));
        CHECK(menu->selected_song_ == 0);
        menu->indexed_songs_.resize(10);
        for (std::size_t i = 0; i < 10; ++i) {
            menu->indexed_songs_[i].path = "chart-" + std::to_string(i) + ".bms";
            menu->visible_song_indices_.push_back(i);
        }
        CHECK(menu->move_song_select_selection(-1));
        CHECK(menu->selected_song_ == 9);
        CHECK(menu->move_song_select_selection(1));
        CHECK(menu->selected_song_ == 0);
        CHECK(menu->move_song_select_selection(7));
        CHECK(menu->selected_song_ == 7);
        CHECK(menu->move_song_select_selection(7));
        CHECK(menu->selected_song_ == 4);
        CHECK(menu->move_song_select_selection(-14));
        CHECK(menu->selected_song_ == 0);
        CHECK_FALSE(menu->move_song_select_selection(0));
        CHECK_FALSE(menu->move_song_select_selection(10));
        CHECK(menu->move_song_select_selection(std::numeric_limits<int>::max()));
        CHECK(menu->selected_song_ == 7);
        CHECK(menu->move_song_select_selection(-std::numeric_limits<int>::max()));
        CHECK(menu->selected_song_ == 0);
        CHECK(menu->move_song_select_selection(std::numeric_limits<int>::min()));
        CHECK(menu->selected_song_ == 2);
        menu->visible_song_indices_ = {0};
        menu->selected_song_ = 0;
        CHECK_FALSE(menu->move_song_select_selection(1));
        CHECK_FALSE(menu->move_song_select_selection(-1));

        menu->config_.ui.difficulty_table_path = "navigation-fixture.json";
        menu->table_levels_ = {"1", "2", "3"};
        REQUIRE(menu->showing_table_level_folders());
        CHECK(menu->move_song_select_selection(-1));
        CHECK(menu->selected_table_level_ == 2);
        CHECK(menu->move_song_select_selection(1));
        CHECK(menu->selected_table_level_ == 0);
        menu->table_levels_.clear();
        CHECK_FALSE(menu->move_song_select_selection(1));
        menu->config_.ui.difficulty_table_path.clear();

        menu->song_select_view_ = MenuApp::SongSelectView::Sources;
        menu->config_.ui.recent_song_sources = {"first", "second"};
        CHECK(menu->move_song_select_selection(-1));
        CHECK(menu->selected_source_ == 2);  // ALL SONG is the first library entry.
        CHECK(menu->move_song_select_selection(1));
        CHECK(menu->selected_source_ == 0);
        menu->config_.ui.recent_song_sources.clear();
        CHECK_FALSE(menu->move_song_select_selection(-1));

        menu->song_select_view_ = MenuApp::SongSelectView::Records;
        menu->local_play_records_.resize(3);
        for (const auto& key : menu_songs::build_chart_path_keys("chart-0.bms", menu->songs_path_)) {
            menu->chart_play_record_indices_[key] = {0, 1, 2};
        }
        CHECK(menu->move_song_select_selection(-1));
        CHECK(menu->selected_record_ == 2);
        CHECK(menu->move_song_select_selection(1));
        CHECK(menu->selected_record_ == 0);
        menu->local_play_records_.clear();
        CHECK_FALSE(menu->move_song_select_selection(-1));
        menu->online_records_view_ = true;
        CHECK_FALSE(menu->move_song_select_selection(1));
    }

    static void check_fixed_timing_best_and_ghost(const std::string& root) {
        auto menu = std::make_unique<MenuApp>();
        menu->songs_path_ = root;
        menu->config_.mode.mods.clear();
        menu_records::ParsedResultRecord parsed;
        parsed.chart_path = "timing-generation.bms";
        ReplayVerificationResult verified;
        verified.status = ReplayVerificationStatus::Verified;
        verified.official_eligible = verified.claims_match = true;
        verified.game_over = false;
        verified.clear_status = "CLEAR";
        verified.final_gauge = "normal";
        const auto append = [&](std::string_view id, int64_t score, int expected_generation) {
            parsed.ruleset_id = id;
            verified.final_score = score;
            const auto suffix = std::to_string(menu->local_play_records_.size());
            menu->append_chart_result_record(parsed, "result-" + suffix, "replay-" + suffix, verified);
            CHECK(menu->local_play_records_.back().judgement_generation == expected_generation);
        };
        append(kRuleset7ReplayRulesetId, 9000, 1);
        REQUIRE_FALSE(menu->chart_best_results_.empty());
        for (const auto& item : menu->chart_best_results_) CHECK(item.second.best_score == 9000);
        append(kCanonicalReplayRulesetId, 1000, 4);
        append(kRuleset7ReplayRulesetId, 9999, 1);
        REQUIRE(menu->local_play_records_.size() == 3);
        for (const auto& item : menu->chart_best_results_) {
            CHECK(item.second.best_score == 1000);
            CHECK(item.second.judgement_generation == 4);
        }
        append(kRuleset9ReplayRulesetId, 9999, 2);
        CHECK(menu->chart_best_results_.begin()->second.best_score == 1000);
        // Feed score-sorted history: the current generation takes priority,
        // while an old replay remains selectable when no current one exists.
        menu->current_song_record_indices_ = {2, 0, 1};
        CHECK(menu->best_replay_path_for_selected_song() == "replay-1");
        menu->current_song_record_indices_ = {2, 0};
        CHECK(menu->best_replay_path_for_selected_song() == "replay-2");
        const auto keys = menu_songs::build_chart_path_keys(parsed.chart_path, root);
        REQUIRE(keys.size() >= 2);
        const auto current = menu->chart_best_results_.begin()->second;
        auto historical = current;
        historical.judgement_generation = 1;
        historical.best_score = 99999;
        // A higher old BMS policy must not outrank the new judgement baseline.
        historical.bms_gauge_version = historical.bms_control_flow_version = historical.bms_timing_version = 2;
        SongEntry entry;
        entry.path = parsed.chart_path;
        for (const bool reverse : {false, true}) {
            menu->chart_best_results_.clear();
            menu->chart_best_results_[keys[0]] = reverse ? current : historical;
            menu->chart_best_results_[keys[1]] = reverse ? historical : current;
            const auto best = menu->best_result_for_song_entry(entry);
            CHECK(best.best_score == 1000);
            CHECK(best.judgement_generation == 4);
        }
    }

    static void check_bms_policy_best(const std::string& root) {
        auto menu = std::make_unique<MenuApp>();
        menu->songs_path_ = root;
        menu_records::ParsedResultRecord parsed;
        parsed.chart_path = "policy.bms";
        parsed.bms_random_seed = 0;
        ReplayVerificationResult verified;
        verified.status = ReplayVerificationStatus::Verified;
        verified.official_eligible = verified.claims_match = true;
        verified.game_over = false;
        verified.clear_status = "CLEAR";
        verified.final_gauge = "normal";
        const std::array<std::array<int, 3>, 7> policies{{
            {{1, 1, 1}}, {{1, 1, 2}}, {{1, 2, 1}}, {{2, 1, 1}},
            {{2, 1, 2}}, {{2, 2, 1}}, {{2, 2, 2}},
        }};
        const auto append = [&](const std::array<int, 3>& policy, int64_t score, uint32_t seed) {
            parsed.bms_gauge_version = policy[0];
            parsed.bms_control_flow_version = policy[1];
            parsed.bms_timing_version = policy[2];
            parsed.bms_random_seed = seed;
            verified.final_score = score;
            const auto name = std::to_string(menu->local_play_records_.size());
            menu->append_chart_result_record(parsed, "result-" + name, "replay-" + name, verified);
            const auto& stored = menu->local_play_records_.back();
            CHECK(stored.bms_gauge_version == policy[0]);
            CHECK(stored.bms_control_flow_version == policy[1]);
            CHECK(stored.bms_timing_version == policy[2]);
            CHECK(stored.bms_random_seed == seed);
        };
        for (std::size_t i = 0; i < policies.size(); ++i) {
            const int64_t score = 900 - static_cast<int64_t>(i) * 100;
            append(policies[i], score, static_cast<uint32_t>(i));
            for (const auto& item : menu->chart_best_results_) {
                CHECK(item.second.bms_gauge_version == policies[i][0]);
                CHECK(item.second.bms_control_flow_version == policies[i][1]);
                CHECK(item.second.bms_timing_version == policies[i][2]);
                CHECK(item.second.best_score == score);
            }
        }
        for (std::size_t i = 0; i + 1 < policies.size(); ++i) append(policies[i], 99999, 42);
        for (const auto& item : menu->chart_best_results_) CHECK(item.second.best_score == 300);
        append(policies.back(), 400, UINT32_MAX);
        for (const auto& item : menu->chart_best_results_) CHECK(item.second.best_score == 400);
        CHECK(menu->local_play_records_.size() == 14);
        // Loading aliases in either order must not compare legacy high scores
        // with the newest policy's score. Random seed never partitions BEST.
        const auto keys = menu_songs::build_chart_path_keys(parsed.chart_path, root);
        REQUIRE(keys.size() >= 2);
        auto current = menu->chart_best_results_.begin()->second;
        auto historical = current;
        historical.bms_gauge_version = 1;
        historical.best_score = 99999;
        SongEntry entry;
        entry.path = parsed.chart_path;
        for (int reverse = 0; reverse < 2; ++reverse) {
            menu->chart_best_results_.clear();
            menu->chart_best_results_[keys[0]] = reverse ? current : historical;
            menu->chart_best_results_[keys[1]] = reverse ? historical : current;
            CHECK(menu->best_result_for_song_entry(entry).best_score == 400);
        }
    }
    static void check_bms_policy_live_and_restore(const std::filesystem::path& root) {
        const auto replay_path = root / "bound-replay.json";
        { std::ofstream out(replay_path); out << "live-export-evidence-fixture"; }
        gameplay::ResultFile result;
        result.chart_path = (root / "chart.bms").u8string();
        result.chart_format = "bms";
        result.chart_sha256 = std::string(64, 'a');
        result.replay_sha256 = std::string(64, 'b');
        result.replay_path = replay_path.u8string();
        result.ruleset_id = std::string(kCanonicalReplayRulesetId);
        result.clear_status = "CLEAR";
        result.final_gauge = "normal";
        result.bms_timing_version = result.bms_gauge_version = result.bms_control_flow_version = 2;
        result.bms_random_seed = UINT32_MAX;
        for (int mismatch = 0; mismatch < 5; ++mismatch) {
            auto menu = std::make_unique<MenuApp>();
            menu->songs_path_ = root.u8string();
            menu->last_result_ = result.stats;
            menu->last_game_over_ = false;
            menu->last_clear_status_ = result.clear_status;
            menu->last_final_gauge_ = result.final_gauge;
            menu->last_result_bms_timing_version_ = 2;
            menu->last_result_bms_gauge_version_ = 2;
            menu->last_result_bms_control_flow_version_ = 2;
            menu->last_result_bms_random_seed_ = UINT32_MAX;
            auto stored = result;
            if (mismatch == 1) stored.bms_gauge_version = 1;
            if (mismatch == 2) stored.bms_control_flow_version = 1;
            if (mismatch == 3) stored.bms_random_seed = 0;
            if (mismatch == 4) stored.bms_random_seed.reset();
            const auto result_path = root / ("result-policy-" + std::to_string(mismatch) + ".json");
            REQUIRE(gameplay::save_result_json(result_path.u8string(), stored).success());
            menu->cache_current_session_result(result.chart_path, result_path.u8string(),
                replay_path.u8string(), result.replay_sha256, true, false);
            REQUIRE(menu->local_play_records_.size() == 1);
            CHECK(menu->local_play_records_.front().replay_claims_match == (mismatch == 0));
            CHECK(menu->chart_best_results_.empty() == (mismatch != 0));
            // History display restores the record's policy even when the next
            // run's defaults are different; it does not reinterpret old stats.
            REQUIRE(menu->open_result_record(result_path.u8string(), replay_path.u8string()));
            CHECK(menu->last_result_bms_gauge_version_ == stored.bms_gauge_version);
            CHECK(menu->last_result_bms_control_flow_version_ == stored.bms_control_flow_version);
            CHECK(menu->last_result_bms_random_seed_ == stored.bms_random_seed);
        }
    }
    static void check_result_difficulty_handoff() {
        struct TableCase { const char* url; const char* symbol; int keys; bool aery; int prefix_keys; };
        const TableCase cases[] = {
            {"https://asumatoki.kr/table/aery/header.json", "⑤", 5, true, 0},
            {"https://classmaterma.github.io/4UE/table.html", "4UE", 4, false, 4},
            {"https://classmaterma.github.io/UE/table.html", "6UE", 6, false, 6},
            {"https://classmaterma.github.io/8UE/table.html", "8UE", 8, false, 8},
            {"https://calc.10k-revive.cloud/table.html", "Ⓡ", 10, false, 10},
            {"https://fixture.invalid/custom.json", "⑤", 5, false, 0},
            {"", "", 5, false, 0},
        };
        auto menu = std::make_unique<MenuApp>();
        menu->songs_path_ = "fixture";
        for (const auto& fixture : cases) {
            menu->config_.ui.difficulty_table_url = fixture.url;
            menu->config_.ui.difficulty_table_path = *fixture.url ? "fixture-table.json" : "";
            SongEntry entry;
            entry.path = "fixture.bms";
            entry.title = "Result difficulty fixture";
            entry.level = 12;
            entry.key_count = fixture.keys;
            entry.difficulty_table_symbol = fixture.symbol;
            entry.difficulty_table_level = *fixture.url ? "LEVEL 12" : "";
            menu->update_last_chart_metadata("fixture/fixture.bms", &entry);
            render::MenuRenderData result;
            menu->populate_result_render_data(result, {});
            CHECK(result.result.difficulty_label == (*fixture.url
                ? std::string(fixture.symbol) + "LEVEL 12" : "LV 12"));
            CHECK(result.result.difficulty_is_aery_five_key == fixture.aery);
            CHECK(result.result.difficulty_prefix_key_count == fixture.prefix_keys);
        }
    }
    static void check_table_loading(const std::string& profile) {
        auto menu = std::make_unique<MenuApp>();
        menu->profile_dir_ = profile;
        menu->config_.ui.language = "ko";
        menu->config_.ui.all_song_sources = true;
        menu->config_.ui.recent_song_sources.clear();
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->menu_navigator_.reset(MenuApp::Screen::SongSelect);
        menu->difficulty_table_url_editing_ = true;
        const auto check_loading = [&] {
            CHECK(menu->difficulty_table_loading());
            CHECK(menu->snapshot_.render.song_select.difficulty_table_name == u8"로딩 중");
            CHECK(menu->snapshot_.render.song_select.difficulty_table_status == u8"로딩 중");
            render::MenuRenderData browser;
            menu->populate_song_browser_render_data(browser);
            bool loading_row = false;
            for (const auto& row : browser.generic.rows)
                if (row.row_index == 5) loading_row = row.value == u8"로딩 중";
            CHECK(loading_row);
        };
        const auto check_finished = [&] {
            CHECK_FALSE(menu->difficulty_table_loading());
            CHECK(menu->snapshot_.render.song_select.difficulty_table_name != u8"로딩 중");
            CHECK(menu->snapshot_.render.song_select.difficulty_table_status != u8"로딩 중");
        };
        int fetches = 0;
        const DifficultyTableFetchFunction fetch = [&](std::string_view url, std::size_t) {
            check_loading(); // The blocking operation sees the already-published UI.
            if (!menu->config_.ui.difficulty_table_path.empty()) {
                menu->refresh_song_source(false);
                check_loading(); // Nested cache application retains the outer import.
            }
            ++fetches;
            DifficultyTableHttpResponse response;
            response.status_code = 200;
            response.final_url = std::string(url);
            response.body = url.find("data.json") != std::string_view::npos
                ? R"([{"md5":"0123456789abcdef0123456789abcdef","level":"1"}])"
                : R"({"name":"Loading fixture","symbol":"lv","data_url":"data.json"})";
            return response;
        };
        for (int refresh = 0; refresh < 2; ++refresh) {
            REQUIRE(menu->apply_difficulty_table_url("https://fixture.invalid/header.json", fetch));
            check_finished();
            CHECK(menu->snapshot_.render.song_select.difficulty_table_name == "Loading fixture");
        }
        CHECK(fetches == 4);
        const auto selected_path = menu->config_.ui.difficulty_table_path;
        CHECK_FALSE(menu->apply_difficulty_table_url("https://fixture.invalid/failure.json",
            [&](std::string_view, std::size_t) {
                check_loading();
                DifficultyTableHttpResponse response;
                response.error = "Fixture download failed";
                return response;
            }));
        check_finished();
        CHECK_FALSE(menu->song_browser_status_message_.empty());
        CHECK(menu->config_.ui.difficulty_table_path == selected_path);
        bool threw = false;
        try {
            static_cast<void>(menu->apply_difficulty_table_url("https://fixture.invalid/throw.json",
                [&](std::string_view, std::size_t) -> DifficultyTableHttpResponse {
                    check_loading();
                    throw std::runtime_error("Fixture fetch exception");
                }));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        CHECK(threw);
        check_finished();
        CHECK(menu->config_.ui.difficulty_table_path == selected_path);
        menu->select_native_difficulty_levels();
        check_finished();
        CHECK(menu->config_.ui.difficulty_table_path.empty());
    }
    static render::MenuRenderData render_index(SongIndex index, const std::vector<std::string>& roots) {
        auto menu = std::make_unique<MenuApp>();
        menu->config_.ui.all_song_sources = true;
        menu->config_.ui.recent_song_sources = roots;
        menu->songs_path_ = roots.front();
        menu->song_select_view_ = MenuApp::SongSelectView::Songs;
        menu->update_song_list(std::move(index));
        render::MenuRenderData render;
        menu->populate_song_select_render_data(render, {}, {}, nullptr);
        return render;
    }
    static void check_metadata_handoff(const SongEntry& entry, const std::string& root) {
        auto menu = std::make_unique<MenuApp>();
        menu->songs_path_ = root;
        menu->song_select_view_ = MenuApp::SongSelectView::Songs;
        menu->update_song_list(SongIndex{{entry}});
        render::MenuRenderData render;
        menu->populate_song_select_render_data(render, {}, {}, nullptr);
        CHECK(render.song_select.selected_song_chart_author == entry.chart_author);
        CHECK(render.song_select.selected_song_artist == entry.artist);
        REQUIRE(render.song_select.songs.size() == 1);
        CHECK(render.song_select.songs.front().artist == entry.artist);
        CHECK(render.song_select.selected_song_has_bpm_changes == entry.has_bpm_changes);
        menu->update_last_chart_metadata((std::filesystem::u8path(root) / entry.path).u8string(), &entry);
        menu->populate_result_render_data(render, {});
        CHECK(render.result.chart_author == entry.chart_author);
        CHECK(render.result.artist == entry.artist);
        menu->update_last_chart_metadata((std::filesystem::u8path(root) / "unindexed.bms").u8string(), nullptr);
        menu->populate_result_render_data(render, {});
        CHECK(render.result.chart_author.empty());
    }
};
}  // namespace tenriff::app
#endif

namespace {
namespace fs = std::filesystem;
using namespace tenriff::app;

struct AggregateFixture {
    fs::path base;
    AggregateFixture() {
        const auto parent = fs::temp_directory_path() / "tenriff_source_aggregate_tests";
        fs::create_directories(parent);
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = parent / (std::to_string(stamp) + "-" + std::to_string(attempt));
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) { base = std::move(candidate); break; }
        }
        REQUIRE_FALSE(base.empty());
    }
    ~AggregateFixture() { std::error_code ignored; fs::remove_all(base, ignored); }
    fs::path source(const std::string& name) {
        auto value = base / fs::u8path(name);
        fs::create_directories(value);
        return value;
    }
    std::string profile() const { return (base / "profile").u8string(); }
    std::string cache(const fs::path& source) const {
        return song_index_cache_path_for_source(profile(), source.u8string());
    }
};

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << text;
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_chart(const fs::path& path, const std::string& title) {
    write_text(path, "#TITLE " + title + "\n#ARTIST Fixture\n#BPM 150\n#PLAYLEVEL 5\n#00111:01\n");
}

SongEntry cached_entry(std::string path, std::string title) {
    SongEntry entry;
    entry.path = std::move(path);
    entry.title = std::move(title);
    entry.format = "bms";
    entry.key_count = 10;
    entry.level = 5;
    entry.native_level = 5;
    return entry;
}

std::string source_key(const fs::path& source) {
    return menu_songs::normalize_path_key(fs::weakly_canonical(source));
}

bool wait_until_finished(SongIndexerThread& indexer) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (indexer.is_running() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !indexer.is_running();
}
}  // namespace

TEST_CASE("ALL SONG aggregates cached roots and deduplicates overlapping canonical chart paths") {
    AggregateFixture f;
    const auto library = f.source(u8"음악_ライブラリー");
    const auto nested = library / "nested";
    write_chart(nested / "same.bms", "Actual file");
    write_chart(library / "second.bms", "Second actual");
    write_text(nested / "cover.png", "fixture");
    write_text(nested / "preview.wav", "fixture");
    auto first = cached_entry("nested/same.bms", "First cached winner");
    first.background_preview_path = "nested/cover.png";
    first.audio_preview_path = (nested / "preview.wav").u8string();
    SongIndex parent{{first, cached_entry("second.bms", "Second cached")}};
    REQUIRE(save_song_index(f.cache(library), parent));
    REQUIRE(save_song_index(f.cache(nested), SongIndex{{cached_entry("same.bms", "Nested duplicate")}}));
    const auto before_parent = read_text(fs::u8path(f.cache(library)));
    const auto before_nested = read_text(fs::u8path(f.cache(nested)));

    const auto combined = aggregate_song_sources(
        {library.u8string(), (library / ".").u8string(), nested.u8string()}, f.profile());
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.cached_sources == 2);
    CHECK(combined.scanned_sources == 0);
    CHECK(combined.warnings.empty());
    REQUIRE(combined.index.entries.size() == 2);
    CHECK(combined.source_counts.at(source_key(library)) == 2);
    CHECK(combined.source_counts.at(source_key(nested)) == 1);
    const auto& entry = combined.index.entries.front();
    CHECK(entry.title == "First cached winner");
    CHECK(fs::u8path(entry.path) == fs::weakly_canonical(nested / "same.bms"));
    CHECK(fs::u8path(entry.background_preview_path) == fs::weakly_canonical(nested / "cover.png"));
    CHECK(fs::u8path(entry.audio_preview_path) == fs::weakly_canonical(nested / "preview.wav"));
    CHECK(read_text(fs::u8path(f.cache(library))) == before_parent);
    CHECK(read_text(fs::u8path(f.cache(nested))) == before_nested);
    CHECK(load_song_index(f.cache(library)).index.entries.front().path == "nested/same.bms");
}

TEST_CASE("ALL SONG preserves distinct copies and identical filenames in separate roots") {
    AggregateFixture f;
    const auto first = f.source("first"), second = f.source("second");
    write_chart(first / "same.bms", "Same bytes");
    write_chart(second / "same.bms", "Same bytes");
    const auto combined = aggregate_song_sources({first.u8string(), second.u8string()}, f.profile());
    REQUIRE(combined.index.entries.size() == 2);
    CHECK(combined.scanned_sources == 2);
    CHECK(combined.index.entries[0].path != combined.index.entries[1].path);
    CHECK(combined.index.entries[0].md5 == combined.index.entries[1].md5);
    for (const auto& source : {first, second}) {
        const auto cached = load_song_index(f.cache(source));
        REQUIRE(cached.loaded_from_file);
        REQUIRE(cached.index.entries.size() == 1);
        CHECK(cached.index.entries.front().path == "same.bms");
    }
}

TEST_CASE("ALL SONG uses a valid empty cache and scans only roots missing a compatible cache") {
    AggregateFixture f;
    const auto cached = f.source("cached"), cold = f.source("cold");
    const auto missing = f.base / "removed";
    write_chart(cached / "new-unscanned.bms", "Not until F5");
    write_chart(cold / "fresh.bms", "Fresh scanned chart");
    REQUIRE(save_song_index(f.cache(cached), SongIndex{}));
    REQUIRE(save_song_index(f.cache(missing), SongIndex{{cached_entry("stale.bms", "Offline cache")}}));
    std::vector<int> sources;
    const auto combined = aggregate_song_sources(
        {cached.u8string(), cold.u8string(), missing.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            sources.push_back(update.source_index);
            CHECK(update.source_total == 3);
        });
    CHECK(combined.cached_sources == 1);
    CHECK(combined.scanned_sources == 1);
    CHECK(combined.missing_sources == 1);
    REQUIRE(combined.index.entries.size() == 1);
    CHECK(combined.index.entries.front().title == "Fresh scanned chart");
    CHECK(combined.source_counts.at(source_key(cached)) == 0);
    CHECK(combined.source_counts.at(source_key(cold)) == 1);
    CHECK(combined.source_counts.at(source_key(missing)) == 0);
    REQUIRE(combined.warnings.size() == 1);
    CHECK(combined.warnings.front().find("Songs path not found") != std::string::npos);
    REQUIRE_FALSE(sources.empty());
    for (std::size_t i = 1; i < sources.size(); ++i) CHECK(sources[i] >= sources[i - 1]);
}

TEST_CASE("ALL SONG indexer publishes both sides of a missing source in any position") {
    for (int missing_position : {0, 1, 2}) {
        for (int scan_mode : {0, 1, 2}) {
            AggregateFixture f;
            const auto first = f.source("first"), last = f.source("last");
            const auto missing = f.base / fs::u8path(u8"없는_폴더");
            write_chart(first / "one.bms", "First scanned");
            write_chart(last / "two.bms", "Last scanned");
            if (scan_mode != 0) {
                REQUIRE(save_song_index(f.cache(first), SongIndex{{cached_entry("one.bms", "First cached")}}));
                REQUIRE(save_song_index(f.cache(last), SongIndex{{cached_entry("two.bms", "Last cached")}}));
            }
            REQUIRE(save_song_index(f.cache(missing), SongIndex{{cached_entry("stale.bms", "Offline cache")}}));
            const auto old_missing = read_text(fs::u8path(f.cache(missing)));
            std::vector<std::string> roots{first.u8string(), last.u8string()};
            roots.insert(roots.begin() + missing_position, missing.u8string());
            SongIndexerThread indexer;
            REQUIRE(indexer.start_sources(roots, f.profile(), {}, scan_mode == 2));
            REQUIRE(wait_until_finished(indexer));
            SongIndex visible{{cached_entry("prior.bms", "Previous visible index")}};
            std::vector<std::string> warnings;
            std::unordered_map<std::string, int> counts;
            REQUIRE(indexer.poll_result(visible, warnings, &counts));
            REQUIRE(visible.entries.size() == 2);
            CHECK(visible.entries[0].title == (scan_mode == 1 ? "First cached" : "First scanned"));
            CHECK(visible.entries[1].title == (scan_mode == 1 ? "Last cached" : "Last scanned"));
            CHECK(counts.at(source_key(first)) == 1);
            CHECK(counts.at(source_key(missing)) == 0);
            CHECK(counts.at(source_key(last)) == 1);
            CHECK_FALSE(warnings.empty());
            CHECK(read_text(fs::u8path(f.cache(missing))) == old_missing);
        }
    }
}

TEST_CASE("ALL SONG skips deleted folders inside a valid cache and continues with later charts and sources") {
    AggregateFixture f;
    const auto first = f.source("first"), last = f.source("last");
    write_chart(first / "kept" / "one.bms", "First actual");
    write_chart(first / "kept" / "two.bms", "Second actual");
    fs::create_directories(first / "folder.bms");
    write_chart(last / "three.bms", "Last actual");
    REQUIRE(save_song_index(f.cache(first), SongIndex{{
        cached_entry("kept/one.bms", "First cached"),
        cached_entry("deleted/subfolder/stale.bms", "Deleted cached"),
        cached_entry("kept/deleted-file.bms", "Deleted file"),
        cached_entry("folder.bms", "Replaced by directory"),
        cached_entry("kept/two.bms", "Second cached")}}));
    REQUIRE(save_song_index(f.cache(last), SongIndex{{cached_entry("three.bms", "Last cached")}}));
    const auto before = read_text(fs::u8path(f.cache(first)));

    const auto combined = aggregate_song_sources({first.u8string(), last.u8string()}, f.profile());
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.cached_sources == 2);
    CHECK(combined.scanned_sources == 0);
    REQUIRE(combined.index.entries.size() == 3);
    CHECK(combined.index.entries[0].title == "First cached");
    CHECK(combined.index.entries[1].title == "Second cached");
    CHECK(combined.index.entries[2].title == "Last cached");
    CHECK(combined.source_counts.at(source_key(first)) == 2);
    CHECK(combined.source_counts.at(source_key(last)) == 1);
    REQUIRE(combined.warnings.size() == 1);
    CHECK(combined.warnings.front().find("Skipped 3 unavailable cached chart(s)") != std::string::npos);
    CHECK(read_text(fs::u8path(f.cache(first))) == before);
}

#ifdef _WIN32
TEST_CASE("lobby selection wraps songs folders libraries and records without overflow") {
    MenuAppSongSourceTestAccess::check_cyclic_lobby_navigation();
}

TEST_CASE("fixed timing BEST and automatic ghost separate historical scores while preserving history") {
    AggregateFixture f;
    MenuAppSongSourceTestAccess::check_fixed_timing_best_and_ghost(f.base.u8string());
}

TEST_CASE("BMS local best selects policy tuples without mixing old scores or partitioning branch seeds") {
    AggregateFixture f;
    MenuAppSongSourceTestAccess::check_bms_policy_best(f.base.u8string());
}

TEST_CASE("BMS live result cache checks policy and seed integrity and restores history metadata") {
    AggregateFixture f;
    MenuAppSongSourceTestAccess::check_bms_policy_live_and_restore(f.base);
}

TEST_CASE("result difficulty metadata preserves the selected registered table display policy") {
    MenuAppSongSourceTestAccess::check_result_difficulty_handoff();
}

TEST_CASE("difficulty table loading is published before import and cleared on success refresh failure and unwind") {
    AggregateFixture f;
    MenuAppSongSourceTestAccess::check_table_loading(f.profile());
}

TEST_CASE("loading optimization reapplies a table without rereading a single folder or records") {
    AggregateFixture f;
    MenuAppSongSourceTestAccess::check_table_reapply(f.base / "single-profile", false);
}

TEST_CASE("loading optimization reapplies a table without rebuilding ALL SONG") {
    AggregateFixture f;
    MenuAppSongSourceTestAccess::check_table_reapply(f.base / "all-profile", true);
}

TEST_CASE("1.8.9.4 metadata passes indexed author and BPM changes into lobby and result without stale credits") {
    AggregateFixture f;
    const auto root = f.source("metadata");
    write_text(root / "chart.bms", u8"#TITLE Metadata fixture\n#ARTIST Composer\n#SUBARTIST obj: パターン作者 / BGA: Guest\n"
               "#BPM 120\n#00103:96\n#00211:01\n");
    std::vector<std::string> warnings;
    const auto index = scan_songs(root.u8string(), nullptr, warnings);
    REQUIRE(index.entries.size() == 1);
    REQUIRE(index.entries.front().chart_author == u8"パターン作者");
    REQUIRE(index.entries.front().artist == "Composer / BGA: Guest");
    REQUIRE(index.entries.front().has_bpm_changes);
    MenuAppSongSourceTestAccess::check_metadata_handoff(index.entries.front(), root.u8string());
}

TEST_CASE("ALL SONG final aggregate reaches visible song cards despite a missing middle source") {
    AggregateFixture f;
    const auto first = f.source("first"), last = f.source("last");
    const auto missing = f.base / "missing";
    write_chart(first / "one.bms", "First");
    write_chart(last / "two.bms", "Last");
    const std::vector<std::string> roots{first.u8string(), missing.u8string(), last.u8string()};
    auto combined = aggregate_song_sources(roots, f.profile());
    REQUIRE(combined.index.entries.size() == 2);
    const auto render = MenuAppSongSourceTestAccess::render_index(std::move(combined.index), roots);
    CHECK(render.song_select.song_count == 2);
    CHECK(render.song_select.list_total_count == 2);
    REQUIRE(render.song_select.songs.size() == 2);
    CHECK_FALSE(render.song_select.showing_sources);
    CHECK_FALSE(render.song_select.showing_records);
    CHECK_FALSE(render.song_select.indexing);
}
#endif

TEST_CASE("ALL SONG reports cached chart progress and cancellation keeps the caller's prior list") {
    AggregateFixture f;
    const auto root = f.source("root");
    SongIndex cache;
    for (int i = 0; i < 130; ++i) {
        const auto name = std::to_string(i) + ".bms";
        write_chart(root / name, "Actual");
        cache.entries.push_back(cached_entry(name, "Cached"));
    }
    REQUIRE(save_song_index(f.cache(root), cache));
    const auto before = read_text(fs::u8path(f.cache(root)));
    std::vector<int> processed;
    const auto combined = aggregate_song_sources({root.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            if (update.source.stage == SongIndexProgressStage::BuildingMetadata) {
                CHECK(update.source.total == 130);
                processed.push_back(update.source.processed);
            }
        });
    REQUIRE(combined.index.entries.size() == 130);
    CHECK((processed == std::vector<int>{0, 64, 128, 130}));
    bool cancel = false;
    const auto cancelled = aggregate_song_sources({root.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            if (update.source.stage == SongIndexProgressStage::BuildingMetadata &&
                update.source.processed == 64) cancel = true;
        }, [&] { return cancel; });
    CHECK(cancelled.cancelled);
    CHECK(cancelled.index.entries.empty());
    CHECK(cancelled.source_counts.empty());
    CHECK(read_text(fs::u8path(f.cache(root))) == before);
}

TEST_CASE("ALL SONG falls back to legacy caches and rescans invalid or profile-incompatible caches") {
    AggregateFixture f;
    const auto legacy = f.source("legacy"), invalid = f.source("invalid"), mismatch = f.source("mismatch");
    write_chart(legacy / "legacy.bms", "Actual legacy");
    write_chart(invalid / "invalid.bms", "Scanned invalid");
    write_chart(mismatch / "mismatch.bms", "Scanned mismatch");
    REQUIRE(save_song_index(legacy_song_index_cache_path_for_source(legacy.u8string()),
                            SongIndex{{cached_entry("legacy.bms", "Legacy cached title")}}));
    write_text(fs::u8path(f.cache(invalid)), "{broken json");
    SongIndexOptions fast;
    fast.profile = SongIndexProfile::Fast;
    REQUIRE(save_song_index(f.cache(mismatch), SongIndex{{cached_entry("mismatch.bms", "Wrong profile")}}, fast));

    const auto combined = aggregate_song_sources({legacy.u8string(), invalid.u8string(), mismatch.u8string()}, f.profile());
    CHECK(combined.cached_sources == 1);
    CHECK(combined.scanned_sources == 2);
    REQUIRE(combined.index.entries.size() == 3);
    CHECK(combined.index.entries[0].title == "Legacy cached title");
    CHECK(combined.index.entries[1].title == "Scanned invalid");
    CHECK(combined.index.entries[2].title == "Scanned mismatch");
    CHECK_FALSE(combined.warnings.empty());
    CHECK(load_song_index(f.cache(invalid)).loaded_from_file);
    CHECK(load_song_index(f.cache(mismatch)).loaded_from_file);
}

TEST_CASE("ALL SONG force rescan bypasses valid metadata caches") {
    AggregateFixture f;
    const auto root = f.source("root");
    write_chart(root / "song.bms", "Original");
    const auto first = aggregate_song_sources({root.u8string()}, f.profile());
    REQUIRE(first.scanned_sources == 1);
    auto cache = load_song_index(f.cache(root));
    REQUIRE(cache.loaded_from_file);
    cache.index.entries.front().title = "Stale cached metadata";
    REQUIRE(save_song_index(f.cache(root), cache.index));
    const auto reused = aggregate_song_sources({root.u8string()}, f.profile());
    REQUIRE(reused.index.entries.size() == 1);
    CHECK(reused.index.entries.front().title == "Stale cached metadata");
    const auto rescanned = aggregate_song_sources({root.u8string()}, f.profile(), {}, {}, {}, true);
    REQUIRE(rescanned.index.entries.size() == 1);
    CHECK(rescanned.scanned_sources == 1);
    CHECK(rescanned.index.entries.front().title == "Original");
}

TEST_CASE("ALL SONG cancellation drops partial aggregates and preserves existing caches") {
    AggregateFixture f;
    const auto first = f.source("first"), second = f.source("second");
    write_chart(first / "one.bms", "One");
    write_chart(second / "two.bms", "Two");
    REQUIRE(save_song_index(f.cache(first), SongIndex{{cached_entry("one.bms", "Old first")}}));
    REQUIRE(save_song_index(f.cache(second), SongIndex{{cached_entry("two.bms", "Old second")}}));
    const auto old_second = read_text(fs::u8path(f.cache(second)));
    bool cancel = false;
    const auto combined = aggregate_song_sources(
        {first.u8string(), second.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            if (update.source_index == 2 && update.source.stage == SongIndexProgressStage::BuildingMetadata) cancel = true;
        }, [&] { return cancel; }, true);
    CHECK(combined.cancelled);
    CHECK(combined.index.entries.empty());
    CHECK(combined.source_counts.empty());
    CHECK(read_text(fs::u8path(f.cache(second))) == old_second);
    // A completed earlier root may update its own cache, even though the final
    // aggregate is not published. It must still be a complete usable cache.
    CHECK(load_song_index(f.cache(first)).loaded_from_file);
    CHECK(load_song_index(f.cache(second)).loaded_from_file);
}

TEST_CASE("atomic source cache cancellation retains byte-identical previous cache") {
    AggregateFixture f;
    const auto root = f.source("root");
    const auto path = f.cache(root);
    const SongIndex old{{cached_entry("old.bms", "Old cache")}};
    const SongIndex replacement{{cached_entry("new.bms", "New cache")}};
    REQUIRE(save_song_index(path, old));
    const auto before = read_text(fs::u8path(path));
    for (bool cancel_at_end : {false, true}) {
        bool cancel = false;
        std::string error;
        CHECK_FALSE(save_song_source_cache_atomically(path, replacement, {}, &error,
            [&](const SongIndexProgress& update) {
                if (update.stage == SongIndexProgressStage::SavingCache &&
                    (!cancel_at_end || update.processed == update.total)) cancel = true;
            }, [&] { return cancel; }));
        CHECK_FALSE(error.empty());
        CHECK(read_text(fs::u8path(path)) == before);
    }
    for (const auto& entry : fs::directory_iterator(fs::u8path(path).parent_path())) {
        CHECK(entry.path().extension() != ".tmp");
    }
    REQUIRE(save_song_source_cache_atomically(path, replacement));
    CHECK(load_song_index(path).index.entries.front().title == "New cache");
}

TEST_CASE("ALL SONG source disappearance during scan cannot replace its previous cache with an empty index") {
    AggregateFixture f;
    const auto root = f.source("removed-during-scan");
    write_chart(root / "song.bms", "Before removal");
    REQUIRE(save_song_index(f.cache(root), SongIndex{{cached_entry("song.bms", "Previous cache")}}));
    const auto before = read_text(fs::u8path(f.cache(root)));
    bool removed = false;
    const auto combined = aggregate_song_sources({root.u8string()}, f.profile(), {},
        [&](const SongSourceAggregateProgress& update) {
            if (!removed && update.source.stage == SongIndexProgressStage::BuildingMetadata) {
                // This directory was created by this fixture and contains no user data.
                fs::remove_all(root);
                removed = true;
            }
        }, {}, true);
    CHECK(removed);
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.index.entries.empty());
    CHECK(combined.missing_sources == 1);
    CHECK_FALSE(combined.warnings.empty());
    CHECK(read_text(fs::u8path(f.cache(root))) == before);
}

TEST_CASE("aggregate indexer returns source counts and stop preserves caller's prior index") {
    AggregateFixture f;
    const auto root = f.source("root");
    write_chart(root / "song.bms", "Indexed");
    SongIndexerThread indexer;
    REQUIRE(indexer.start_sources({root.u8string()}, f.profile()));
    REQUIRE(wait_until_finished(indexer));
    SongIndex visible{{cached_entry("prior.bms", "Previous visible index")}};
    std::vector<std::string> warnings;
    std::unordered_map<std::string, int> counts;
    REQUIRE(indexer.poll_result(visible, warnings, &counts));
    REQUIRE(visible.entries.size() == 1);
    CHECK(visible.entries.front().title == "Indexed");
    CHECK(counts.at(source_key(root)) == 1);
    CHECK(indexer.progress().source_total == 1);
    CHECK(indexer.progress().source_index == 1);
    REQUIRE(indexer.start_sources({root.u8string()}, f.profile(), {}, true));
    indexer.stop();
    CHECK_FALSE(indexer.poll_result(visible, warnings, &counts));
    CHECK(visible.entries.front().title == "Indexed");
    CHECK(counts.at(source_key(root)) == 1);
    REQUIRE(indexer.start(root.u8string(), f.cache(root)));
    REQUIRE(wait_until_finished(indexer));
    REQUIRE(indexer.poll_result(visible, warnings, &counts));
    CHECK(counts.empty());
}

TEST_CASE("ALL SONG empty roots complete without scanning default or current directories") {
    AggregateFixture f;
    write_chart(f.base / "unregistered.bms", "Do not scan");
    const auto combined = aggregate_song_sources({"", ""}, f.profile());
    CHECK_FALSE(combined.cancelled);
    CHECK(combined.index.entries.empty());
    CHECK(combined.source_counts.empty());
    CHECK(combined.scanned_sources == 0);
    CHECK(combined.cached_sources == 0);
    CHECK(combined.warnings.empty());
}

TEST_CASE("polling a completed aggregate never leaves the loading state running") {
    AggregateFixture f;
    SongIndexerThread indexer;
    for (int attempt = 0; attempt < 5000; ++attempt) {
        REQUIRE(indexer.start_sources({}, f.profile()));
        SongIndex result;
        std::vector<std::string> warnings;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!indexer.poll_result(result, warnings)) {
            REQUIRE(std::chrono::steady_clock::now() < deadline);
            std::this_thread::yield();
        }
        // MenuApp publishes its loading overlay immediately after polling.
        // Receiving the final list must also mean the worker has completed.
        CHECK_FALSE(indexer.is_running());
    }
}

TEST_CASE("loading optimization treats completed unpublished indexing as busy") {
    AggregateFixture f;
    const auto root = f.source("pending");
    write_chart(root / "one.bms", "Pending result");
    SongIndexerThread indexer;
    CHECK(indexer.is_idle());
    REQUIRE(indexer.start(root.u8string(), f.cache(root)));
    REQUIRE(wait_until_finished(indexer));
    CHECK_FALSE(indexer.is_idle());
    SongIndex result;
    std::vector<std::string> warnings;
    REQUIRE(indexer.poll_result(result, warnings));
    CHECK(result.entries.size() == 1);
    CHECK(indexer.is_idle());
    indexer.stop();
    CHECK(indexer.is_idle());
}
