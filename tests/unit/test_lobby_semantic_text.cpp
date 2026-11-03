#include "doctest/doctest.h"

#include "app/SessionResultStatus.h"
#include "gameplay/GameplayEngine.h"
#include "render/GameplayGaugePalette.h"

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

namespace tenriff::app {
struct MenuAppLobbySemanticTestAccess {
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
                best.clear_status = index % 4 == 0 ? "NORMAL CLEAR" : "HARD CLEAR";
                best.final_gauge = index % 4 == 0 ? "normal" : "hard";
                best.game_over = false;
                for (const auto& key : menu_songs::build_chart_path_keys(song.path, menu->songs_path_))
                    menu->chart_best_results_[key] = best;
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
                    CHECK(card.score == (index % 2 == 0 ? 7000 + index : 0));
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
        render::MenuRenderData render;
        menu->populate_song_select_render_data(render, song.title, best, &menu->local_play_records_.back());
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

TEST_CASE("lobby score and full combo semantic metadata follow the displayed selected record") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_record_semantics();
}

TEST_CASE("compact native song viewport keeps ten scored rows and authored layouts retain seven row navigation") {
    tenriff::app::MenuAppLobbySemanticTestAccess::check_compact_viewport_and_scores();
}
#endif
