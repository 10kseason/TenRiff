#include "doctest/doctest.h"
#include <fstream>
#include <chrono>

#include "app/SessionResultStatus.h"
#include "gameplay/GameplayEngine.h"
#include "render/GameplayGaugePalette.h"
#include "render/SongSelectClearArt.h"
#include "render/ExScorePresentation.h"
#include "render/SongSelectReviewLayout.h"
#include "render/OptionsConstellation.h"

namespace {
tenriff::gameplay::ResultStats combo_stats(bool resolve_tail) {
    using namespace tenriff;
    gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 6000;
    chart.notes = {{1, 1000, std::nullopt}, {1, 3000, 5000, true}};
    gameplay::GameplayConfig config;
    config.sample_rate = 1000;
    gameplay::GameplayEngine engine(chart, config);
    (void)engine.handle_input(1, input::InputState::Pressed, 1000);
    (void)engine.handle_input(1, input::InputState::Released, 1100);
    (void)engine.handle_input(1, input::InputState::Pressed, 3000);
    if (resolve_tail) (void)engine.handle_input(1, input::InputState::Released, 5000);
    return engine.stats();
}
}

TEST_CASE("lobby score uses clear palette independently of its grade and leaves neutral scores white") {
    using namespace tenriff::render;
    for (const auto& fixture : {
        std::pair{"EX CLEAR", kClearExRgb}, std::pair{"GAUGE SHIFT EX CLEAR", kClearExRgb},
        std::pair{"EX-HARD CLEAR", kClearExRgb}, std::pair{"HARD CLEAR", kClearHardRgb},
        std::pair{"GAUGE SHIFT HARD CLEAR", kClearHardRgb}, std::pair{"NORMAL CLEAR", kClearNormalRgb},
        std::pair{"CLEAR", kClearNormalRgb}, std::pair{"EASY CLEAR", kClearEasyRgb}}) {
        const auto paint = song_select_score_paint(fixture.first);
        CHECK(paint.rgb == fixture.second);
        CHECK(paint.rainbow == (fixture.second == kClearExRgb));
    }
    for (const auto status : {"", "NO PLAY", "FAILED", "ABORTED", "AUTOPLAY", "UNKNOWN CLEAR",
                              "ASSIST PRACTICE GAUGE SHIFT HARD CLEAR"}) {
        const auto paint = song_select_score_paint(status);
        CHECK(paint.rgb == 0xFFFFFF);
        CHECK_FALSE(paint.rainbow);
    }
    CHECK(kFullComboTextRgb == 0xCBEF6B);
}

TEST_CASE("historical full combo requires verified complete chart weight and all LN combo steps") {
    using tenriff::app::verified_record_full_combo;
    const auto pending = combo_stats(false);
    REQUIRE(pending.total_notes == 2);
    REQUIRE(pending.max_combo == pending.total_notes);
    REQUIRE(pending.total_combo_steps == 3);
    CHECK_FALSE(verified_record_full_combo(true, true, false, false, false, pending));
    const auto completed = combo_stats(true);
    REQUIRE(completed.combo == completed.total_combo_steps);
    REQUIRE(completed.accuracy_weight == 2);
    CHECK(verified_record_full_combo(true, true, false, false, false, completed));
    CHECK_FALSE(verified_record_full_combo(false, true, false, false, false, completed));
    CHECK_FALSE(verified_record_full_combo(true, false, false, false, false, completed));
    CHECK_FALSE(verified_record_full_combo(true, true, true, false, false, completed));
    CHECK_FALSE(verified_record_full_combo(true, true, false, true, false, completed));
    CHECK_FALSE(verified_record_full_combo(true, true, false, false, true, completed));
    auto unknown_steps = completed;
    unknown_steps.total_combo_steps = 0;
    CHECK_FALSE(verified_record_full_combo(true, true, false, false, false, unknown_steps));
}

#ifdef _WIN32
#include "app/MenuApp.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "app/ReplayVerifier.h"
#include "app/ProfileSetupFlow.h"

namespace tenriff::app {
struct MenuAppLobbySemanticTestAccess {
    static void check_profile_card_dispatch() {
        auto menu = std::make_unique<MenuApp>();
        menu->config_.audio_ui.master_volume = 0.0;
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->config_.audio_ui.title_music = "none";
        render::MenuClickEvent event;
        event.kind = render::MenuHitTargetKind::SongProfilePanel;
        for (const auto screen : {MenuApp::Screen::Title, MenuApp::Screen::SongSelect}) {
            menu->menu_navigator_.reset(screen);
            menu->song_select_search_active_ = true;
            menu->first_run_profile_ = true;
            // Native profile hit targets use index 0; the default -1 is an
            // invalid event and must be rejected by the shared click guard.
            event.index = -1;
            menu->handle_menu_click(event);
            CHECK(menu->current_screen() == screen);
            event.index = 0;
            menu->handle_menu_click(event);
            CHECK(menu->current_screen() == MenuApp::Screen::QuickSetup);
            CHECK(menu->settings_cursor_ == profile_setup::kAvatarRow);
            CHECK_FALSE(menu->song_select_search_active_);
            CHECK_FALSE(menu->first_run_profile_);
        }
        menu->menu_navigator_.reset(MenuApp::Screen::Result);
        menu->handle_menu_click(event);
        CHECK(menu->current_screen() == MenuApp::Screen::Result);
    }

    static void check_ex_result_omits_legacy_multiplier() {
        auto menu = std::make_unique<MenuApp>();
        menu->has_result_ = true;
        menu->last_result_rate_multiplier_ = 1.5;
        menu->last_result_score_multiplier_ = 0.75;
        menu->last_result_ex_score_ = 3;
        menu->last_result_.counts.pg = 1;
        menu->last_result_.counts.gr = 1;
        for (const auto language : {"en", "ko"}) {
            menu->config_.ui.language = language;
            render::MenuRenderData render;
            menu->populate_result_render_data(render, "Synthetic EX fixture");
            CHECK(render.result.ex_score == 3);
            for (const auto& note : render.result.notes) {
                CHECK(note.find("Rate score x") == std::string::npos);
                CHECK(note.find("Rate 점수 x") == std::string::npos);
                CHECK(note.find("Score x") == std::string::npos);
            }
        }
        // Compatibility values remain available to the historical replay path.
        CHECK(menu->last_result_rate_multiplier_ == 1.5);
        CHECK(menu->last_result_score_multiplier_ == 0.75);
    }

    static void check_compact_viewport_and_scores() {
        auto menu = std::make_unique<MenuApp>();
        menu->config_.audio_ui.master_volume = 0.0;
        menu->config_.audio_ui.background_sound_enabled = false;
        menu->config_.audio_ui.title_music = "none";
        menu->song_select_view_ = MenuApp::SongSelectView::Songs;
        menu->song_select_focus_ = MenuApp::SongSelectFocus::SongList;
        for (int index = 0; index < 23; ++index) {
            SongEntry song;
            song.path = "viewport-fixture-" + std::to_string(index) + ".bms";
            song.title = "Viewport fixture " + std::to_string(index);
            song.artist = "Synthetic artist";
            song.format = "bms";
            song.key_count = 7;
            song.level = index + 1;
            menu->indexed_songs_.push_back(song);
            menu->visible_song_indices_.push_back(static_cast<std::size_t>(index));
            if (index % 2 == 0) {
                MenuApp::BestResultRecord best;
                best.has_value = true;
                best.best_score = 7000 + index;
                best.ex_score = 100 + index;
                best.clear_status = index % 4 == 0 ? "NORMAL CLEAR" : "HARD CLEAR";
                best.final_gauge = index % 4 == 0 ? "normal" : "hard";
                best.game_over = false;
                best.verified_full_combo = index % 8 == 0;
                for (const auto& key : menu_songs::build_chart_path_keys(song.path, menu->songs_path_)) {
                    menu->chart_best_results_[key] = best;
                    menu->chart_score_high_water_[key].best = best;
                }
            }
        }

        // Default exported zero deltas do not opt a skin into the old layout.
        // A genuine authored rectangle or the legacy renderer keeps seven rows.
        for (int layout = 0; layout < 4; ++layout) {
            const bool compact = layout < 2;
            const int expected_rows = song_select_visible_card_count(compact);
            for (const int selected : {0, 11, 22}) {
                menu->selected_song_ = selected;
                render::MenuRenderData render;
                render.lobby_skin.enabled = layout != 0;
                render.lobby_skin.native_menu_renderer = layout != 3;
                if (layout == 1 || layout == 2)
                    render.lobby_skin.native_menu.rects["songselect.rect.022"] =
                        {layout == 2 ? 12.0f : 0.0f, 0.0f, 0.0f, 0.0f};
                const auto best = menu->best_result_for_song_entry(menu->indexed_songs_[selected]);
                menu->populate_song_select_render_data(render, "Fixture", best, nullptr);
                const auto& view = render.song_select;
                CHECK(view.native_list_layout == compact);
                CHECK(view.list_total_count == 23);
                CHECK(view.list_visible_count == expected_rows);
                REQUIRE(view.songs.size() == static_cast<std::size_t>(expected_rows));
                CHECK(view.songs.front().song_index == view.list_window_start);
                bool found_selected = false;
                for (std::size_t row = 0; row < view.songs.size(); ++row) {
                    const auto& card = view.songs[row];
                    const int index = card.song_index;
                    CHECK(index == view.list_window_start + static_cast<int>(row));
                    CHECK(card.title == menu->indexed_songs_[index].title);
                    CHECK(card.selected == (index == selected));
                    found_selected |= card.selected;
                    CHECK(card.has_score == (index % 2 == 0));
                    CHECK(card.verified_full_combo == (index % 8 == 0));
                    CHECK(card.score == (index % 2 == 0 ? 7000 + index : 0));
                    CHECK(card.ex_score == (index % 2 == 0 ? std::optional<int64_t>{100 + index} : std::nullopt));
                    if (card.has_score)
                        CHECK(card.lamp == (index % 4 == 0 ? "NORMAL CLEAR" : "HARD CLEAR"));
                }
                CHECK(found_selected);
            }

            menu->config_.skin.source = layout == 0 ? "native" : "tenriff";
            menu->active_tenriff_skin_ = {};
            menu->active_tenriff_skin_.found = layout != 0;
            menu->active_tenriff_skin_.native_menu_renderer = layout != 3;
            if (layout == 1 || layout == 2)
                menu->active_tenriff_skin_.native_menu.rects["songselect.rect.022"] =
                    {layout == 2 ? 12.0f : 0.0f, 0.0f, 0.0f, 0.0f};
            menu->menu_navigator_.reset(MenuApp::Screen::SongSelect);
            menu->selected_song_ = 0;
            menu->key_page_up_ = 33;
            menu->key_page_down_ = 34;
            menu->publish_snapshot();
            CHECK(menu->snapshot_.render.song_select.native_list_layout == compact);
            menu->handle_song_select_input(menu->key_page_down_);
            CHECK(menu->selected_song_ == expected_rows);
            menu->handle_song_select_input(menu->key_page_up_);
            CHECK(menu->selected_song_ == 0);
        }
    }

    static void check_ex_cache_rebuild() {
        const auto path = std::filesystem::temp_directory_path() /
            ("tenriff-ex-cache-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{path};
        std::filesystem::create_directories(path / "results");
        const std::string original = R"({"chart_path":"synthetic.bms","final_score":999999,"stats":{"raw_score":9999,"counts":{"pg":2,"gr":1,"gd":0,"bd":0,"pr":0},"total_notes":3}})";
        const auto file = path / "results" / "synthetic.json";
        { std::ofstream out(file); out << original; }
        auto menu = std::make_unique<MenuApp>(); menu->profile_dir_ = path.u8string();
        MenuApp::BestResultRecord stale; stale.has_value = true; stale.ex_score = 999999;
        menu->chart_best_results_["stale"] = stale;
        menu->chart_score_high_water_["stale"].best = stale;
        menu->reload_chart_best_results();
        CHECK(menu->chart_best_results_.find("stale") == menu->chart_best_results_.end());
        CHECK(menu->chart_score_high_water_.find("stale") == menu->chart_score_high_water_.end());
        REQUIRE(menu->local_play_records_.size() == 1);
        CHECK(menu->local_play_records_.front().ex_score == 5);
        std::ifstream in(file); const std::string after((std::istreambuf_iterator<char>(in)), {});
        CHECK(after == original); in.close();
        { std::ofstream out(file); out << R"({"chart_path":"synthetic.bms","final_score":999999,"stats":{"raw_score":9999}})"; }
        menu->reload_chart_best_results();
        REQUIRE(menu->local_play_records_.size() == 1);
        CHECK_FALSE(menu->local_play_records_.front().ex_score.has_value());
        CHECK(menu->chart_score_high_water_.empty());
    }

    static void check_ex_high_water_and_clear_lamp() {
        auto menu = std::make_unique<MenuApp>();
        menu_records::ParsedResultRecord parsed; parsed.chart_path = "synthetic-clear.bms";
        parsed.ruleset_id = kCanonicalReplayRulesetId;
        ReplayVerificationResult verified; verified.status = ReplayVerificationStatus::Verified;
        verified.official_eligible = verified.claims_match = true;
        verified.clear_status = "NORMAL CLEAR"; verified.final_gauge = "normal";
        verified.game_over = false;
        verified.stats.counts.pg = 50; verified.final_score = 9999;
        RecordFeedback feedback;
        CHECK(menu->append_chart_result_record(parsed, "normal-100", "normal-replay", verified, &feedback));
        verified.clear_status = "EX CLEAR"; verified.final_gauge = "ex_hard";
        verified.stats.counts.pg = 40; verified.final_score = 999;
        CHECK_FALSE(menu->append_chart_result_record(parsed, "ex-80", "ex-replay", verified, &feedback));
        REQUIRE_FALSE(menu->chart_best_results_.empty());
        CHECK(menu->chart_best_results_.begin()->second.ex_score == 80);
        CHECK(menu->chart_best_results_.begin()->second.final_gauge == "ex_hard");
        verified.clear_status = "NORMAL CLEAR"; verified.final_gauge = "normal";
        verified.stats.counts.pg = 60; verified.final_score = 1;
        verified.stats.total_notes = verified.stats.total_combo_steps = 100;
        verified.stats.accuracy_weight = 100; verified.stats.accuracy_points = 95;
        verified.stats.max_combo = 60;
        CHECK(menu->append_chart_result_record(parsed, "normal-120", "higher-ex-replay", verified, &feedback));
        CHECK(feedback.previous_score == 100);
        CHECK(menu->chart_best_results_.begin()->second.ex_score == 80);
        CHECK(menu->chart_best_results_.begin()->second.final_gauge == "ex_hard");
        REQUIRE_FALSE(menu->chart_score_high_water_.empty());
        CHECK(menu->chart_score_high_water_.begin()->second.best.ex_score == 120);
        CHECK(menu->chart_score_high_water_.begin()->second.best.replay_path == "higher-ex-replay");
        SongEntry entry; entry.path = parsed.chart_path;
        CHECK(menu->best_result_for_song_entry(entry).final_gauge == "ex_hard");
        CHECK(menu->best_score_result_for_song_entry(entry).ex_score == 120);
        menu->indexed_songs_.push_back(entry); menu->visible_song_indices_ = {0}; menu->selected_song_ = 0;
        menu->song_select_view_ = MenuApp::SongSelectView::Songs;
        render::MenuRenderData render;
        menu->populate_song_select_render_data(render, "Synthetic", menu->best_result_for_song_entry(entry), nullptr);
        CHECK(render.song_select.selected_song_lamp == "EX CLEAR");
        CHECK(render.song_select.ex_score == 120);
        CHECK(render.song_select.high_ex_score == 120);
        CHECK(render.song_select.accuracy == 95.0);
        CHECK(render.song_select.rank == "S");
        CHECK(render.song_select.max_combo == 60);
        CHECK(render.song_select.perfect == 60);
        CHECK(render.song_select.score_clear_status == "NORMAL CLEAR");
        REQUIRE(render.song_select.songs.size() == 1);
        CHECK(render.song_select.songs.front().lamp == "EX CLEAR");
        CHECK(render.song_select.songs.front().ex_score == 120);
    }

    static void check_record_semantics() {
        auto menu = std::make_unique<MenuApp>();
        menu->config_.mode.mods.clear();
        menu_records::ParsedResultRecord parsed;
        parsed.chart_path = "lobby-semantic-fixture.bms";
        parsed.ruleset_id = kCanonicalReplayRulesetId;
        parsed.created_utc = "2026-11-03T00:00:00Z";
        ReplayVerificationResult verified;
        verified.status = ReplayVerificationStatus::Verified;
        verified.official_eligible = verified.claims_match = true;
        verified.game_over = false;
        verified.clear_status = "GAUGE SHIFT EX CLEAR";
        verified.final_gauge = "ex_hard";
        verified.stats = combo_stats(true);
        verified.final_score = verified.stats.raw_score;
        (void)menu->append_chart_result_record(parsed, "result-ex.json", "", verified);
        REQUIRE_FALSE(menu->chart_best_results_.empty());
        const auto best = menu->chart_best_results_.begin()->second;
        CHECK(best.verified_full_combo);
        CHECK(menu->local_play_records_.back().verified_full_combo);
        parsed.created_utc = "2026-11-03T00:00:01Z";
        verified.clear_status = "HARD CLEAR";
        verified.final_gauge = "hard";
        verified.stats = combo_stats(false);
        verified.final_score = verified.stats.raw_score;
        (void)menu->append_chart_result_record(parsed, "result-hard-pending.json", "", verified);
        CHECK_FALSE(menu->local_play_records_.back().verified_full_combo);
        CHECK(menu->chart_best_results_.begin()->second.clear_status == "GAUGE SHIFT EX CLEAR");
        SongEntry song;
        song.path = parsed.chart_path;
        song.title = "Synthetic semantic fixture";
        menu->indexed_songs_.push_back(song);
        menu->visible_song_indices_ = {0};
        menu->selected_song_ = 0;
        menu->song_select_view_ = MenuApp::SongSelectView::Records;
        menu->current_song_record_indices_ = {0, 1};
        render::MenuRenderData render;
        menu->populate_song_select_render_data(render, song.title, best, &menu->local_play_records_.back());
        REQUIRE(render.song_select.songs.size() == 2);
        CHECK(render.song_select.songs[0].verified_full_combo);
        CHECK_FALSE(render.song_select.songs[1].verified_full_combo);
        CHECK(render.song_select.score_clear_status == "HARD CLEAR");
        CHECK(render.song_select.selected_song_lamp == "GAUGE SHIFT EX CLEAR");
        CHECK_FALSE(render.song_select.verified_full_combo);
        menu->song_select_view_ = MenuApp::SongSelectView::Songs;
        menu->populate_song_select_render_data(render, song.title, best, nullptr);
        CHECK(render.song_select.score_clear_status == "GAUGE SHIFT EX CLEAR");
        CHECK(render.song_select.verified_full_combo);
        menu->song_select_view_ = MenuApp::SongSelectView::Records;
        menu->populate_song_select_render_data(render, song.title, best, nullptr);
        CHECK(render.song_select.score_clear_status.empty());
        CHECK_FALSE(render.song_select.verified_full_combo);
        verified.status = ReplayVerificationStatus::LegacyUnverified;
        verified.official_eligible = verified.claims_match = false;
        parsed.stats = combo_stats(true);
        parsed.clear_status = "EX CLEAR";
        parsed.final_score = parsed.stats.raw_score;
        (void)menu->append_chart_result_record(parsed, "result-unverified.json", "", verified);
        CHECK_FALSE(menu->local_play_records_.back().verified_full_combo);
    }
};
}

TEST_CASE("home and song profile cards open the existing avatar setup route") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_profile_card_dispatch();
}

TEST_CASE("EX result notes omit legacy rate and score multiplier claims") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_ex_result_omits_legacy_multiplier();
}

TEST_CASE("EX high water and replay improve independently without downgrading the highest clear lamp") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_ex_high_water_and_clear_lamp();
}

TEST_CASE("EX reload invalidates BEST caches without changing historical files") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_ex_cache_rebuild();
}

TEST_CASE("lobby score and full combo semantic metadata follow the displayed selected record") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_record_semantics();
}

TEST_CASE("compact native song viewport keeps ten scored rows and authored layouts retain seven row navigation") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_compact_viewport_and_scores();
}
#endif

TEST_CASE("row artwork uses five clear textures and verified FC never promotes failure or assist") {
    using namespace tenriff::render;
    const std::array<const char*, 4> clears{"EASY CLEAR", "NORMAL CLEAR", "HARD CLEAR", "EX CLEAR"};
    for (std::size_t i = 0; i < clears.size(); ++i) {
        CHECK(song_select_clear_art_index(clears[i], false) == std::optional<std::size_t>{i});
        CHECK(song_select_clear_art_index(clears[i], true) == std::optional<std::size_t>{4});
    }
    CHECK(song_select_clear_art_index("GAUGE SHIFT EX CLEAR", false) == std::optional<std::size_t>{3});
    for (const auto lamp : {"FAILED", "ABORTED", "", "AUTOPLAY", "UNKNOWN CLEAR", "FULL COMBO",
                            "LR2 EX-HARD CLEAR (FC)", "ASSIST PRACTICE GAUGE SHIFT NORMAL CLEAR"}) {
        CHECK_FALSE(song_select_clear_art_index(lamp, false).has_value());
        CHECK_FALSE(song_select_clear_art_index(lamp, true).has_value());
    }
}

TEST_CASE("EX display distinguishes missing evidence from a valid zero and retains long-chart digits") {
    using tenriff::render::ex_score_text;
    CHECK(ex_score_text(std::nullopt) == "--");
    CHECK(ex_score_text(int64_t{0}) == "0");
    CHECK(ex_score_text(int64_t{123456}) == "123456");
}

TEST_CASE("clear art boosts chroma twenty percent without changing gray or transparency") {
    using namespace tenriff::render;
    CHECK(std::string(kSongSelectClearArtFiles[3]) == "TENRIFF_RESULT_EX_galaxy.png");
    const auto matrix = song_select_clear_art_color_matrix();
    const auto apply = [&](std::array<float, 4> input) {
        std::array<float, 4> output{};
        for (std::size_t col = 0; col < 4; ++col) {
            output[col] = matrix[16 + col];
            for (std::size_t row = 0; row < 4; ++row) output[col] += input[row] * matrix[row * 4 + col];
        }
        return output;
    };
    for (float alpha : {0.0f, .25f, 1.0f}) {
        const auto gray = apply({.4f, .4f, .4f, alpha});
        for (std::size_t channel = 0; channel < 3; ++channel) CHECK(gray[channel] == doctest::Approx(.4f));
        CHECK(gray[3] == doctest::Approx(alpha));
        const auto color = apply({.6f, .4f, .2f, alpha});
        CHECK(color[0] - color[1] == doctest::Approx(.24f));
        CHECK(color[1] - color[2] == doctest::Approx(.24f));
        CHECK(color[3] == doctest::Approx(alpha));
    }
}

TEST_CASE("options galaxy reduces chroma and brightness while retaining source transparency") {
    using namespace tenriff::render;
    const auto matrix = options_galaxy_color_matrix();
    const auto apply = [&](std::array<float, 4> input) {
        std::array<float, 4> output{};
        for (std::size_t col = 0; col < 4; ++col) {
            output[col] = matrix[16 + col];
            for (std::size_t row = 0; row < 4; ++row) output[col] += input[row] * matrix[row * 4 + col];
        }
        return output;
    };
    for (float alpha : {0.0f, .25f, 1.0f}) {
        const auto gray = apply({.4f, .4f, .4f, alpha});
        for (std::size_t channel = 0; channel < 3; ++channel) CHECK(gray[channel] == doctest::Approx(.392f));
        CHECK(gray[3] == alpha);
        const auto color = apply({.6f, .4f, .2f, alpha});
        CHECK(color[0] - color[1] == doctest::Approx(.2f * .92f * .98f));
        CHECK(color[1] - color[2] == doctest::Approx(.2f * .92f * .98f));
        CHECK(color[3] == alpha);
    }
}

TEST_CASE("all combo badge follows the displayed clear record instead of a different EX best") {
    using namespace tenriff::render;
    CHECK_FALSE(song_review_all_combo_badge(true, false, true, false));
    CHECK(song_review_all_combo_badge(true, false, false, true));
    CHECK(song_review_all_combo_badge(true, true, true, false));
    CHECK_FALSE(song_review_all_combo_badge(true, true, false, true));
    CHECK_FALSE(song_review_all_combo_badge(false, false, true, true));
    CHECK_FALSE(song_review_all_combo_badge(false, true, true, true));
    CHECK_FALSE(song_review_all_combo_badge(true, false, false, false));
}

TEST_CASE("song title marquee dwells at both ends and returns continuously without a wrap jump") {
    using namespace tenriff::render;
    constexpr float overflow = 140;
    CHECK(song_review_marquee_offset(0, overflow, false) == 0);
    CHECK(song_review_marquee_offset(2, overflow, false) == 0);
    CHECK(song_review_marquee_offset(7, overflow, false) == doctest::Approx(70));
    CHECK(song_review_marquee_offset(12, overflow, false) == doctest::Approx(140));
    CHECK(song_review_marquee_offset(14, overflow, false) == doctest::Approx(140));
    CHECK(song_review_marquee_offset(19, overflow, false) == doctest::Approx(70));
    CHECK(song_review_marquee_offset(24, overflow, false) == 0);
    CHECK(song_review_marquee_offset(24 - .001, overflow, false) < .02f);
    CHECK(song_review_marquee_offset(24 + .001, overflow, false) == 0);
    CHECK(song_review_marquee_offset(7, overflow, true) == 0);
    CHECK(song_review_marquee_offset(7, 0, false) == 0);
    CHECK(song_review_marquee_offset(-100, overflow, false) == 0);
    CHECK(kSongReviewNavLeft + (4 * kSongReviewNavStep + kSongReviewNavWidth) * .5f == 960);
    CHECK(kSongReviewNavLeft > 418); // Large logo never intersects navigation.
    CHECK(kSongReviewNavLeft + 4 * kSongReviewNavStep + kSongReviewNavWidth < 1530); // Account stays separate.
}
