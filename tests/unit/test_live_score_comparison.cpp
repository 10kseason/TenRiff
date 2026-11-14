#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "app/LiveScoreComparison.h"
#include "app/GameplayHudRevisions.h"
#include "app/MenuRecordUtils.h"
#include "app/menu/settings/SkinSettingsController.h"
#include "config/Config.h"
#include "gameplay/GameplayEngine.h"

using tenriff::app::live_score_comparison;
using tenriff::app::live_score_replay_policy_matches;
using tenriff::gameplay::ResultStats;

#ifdef _WIN32
#include "app/MenuApp.h"
#include "app/GameSession.h"
#include "app/ChartFileHash.h"
#include "app/ReplayVerifier.h"
#include "app/MenuSongUtils.h"
#include "render/SkinGameplayPreview.h"
namespace tenriff::app {
struct MenuAppScoreComparisonTestAccess {
    static void check_render_publication(const std::vector<LiveScoreComparison>& comparisons) {
        REQUIRE(comparisons.size() > 1);
        auto menu = std::make_unique<MenuApp>();
        menu->config_.skin.score_comparison_enabled = true;
        menu->gameplay_hud_.score_comparison = comparisons.front();
        render::GameplayHudData cached;
        uint64_t rendered_motion = 0, rendered_text = 0;
        menu->populate_gameplay_render_data(cached, &rendered_motion, &rendered_text);
        for (std::size_t i = 1; i < comparisons.size(); ++i) {
            const auto previous = menu->gameplay_hud_revision_input(menu->gameplay_hud_);
            // Hold player, opponent, and motion fixed: an independent reference
            // or eligibility change must invalidate the production cache itself.
            menu->gameplay_hud_.score_comparison = comparisons[i];
            const auto next = menu->gameplay_hud_revision_input(menu->gameplay_hud_);
            const auto diff = diff_gameplay_hud_revisions(previous, next);
            CHECK(diff.text_changed);
            CHECK_FALSE(diff.motion_changed);
            menu->advance_gameplay_hud_revisions(menu->gameplay_hud_, diff.motion_changed, diff.text_changed);
            if (rendered_motion != menu->gameplay_hud_.motion_revision ||
                rendered_text != menu->gameplay_hud_.text_revision) {
                menu->populate_gameplay_render_data(cached, &rendered_motion, &rendered_text);
            }
            CHECK(cached.score_comparison.live == comparisons[i].live);
            CHECK(cached.score_comparison.best_available == comparisons[i].best_available);
            CHECK(cached.score_comparison.rank_available == comparisons[i].rank_available);
            CHECK(cached.score_comparison.best_score == comparisons[i].best_score);
            CHECK(cached.score_comparison.score_difference == comparisons[i].score_difference);
            CHECK(cached.score_comparison.rank_difference == comparisons[i].rank_difference);
        }
    }
    static void prepare_paused_best(GameSession& session, const std::string& chart_path,
                                    const std::string& replay_path) {
        auto menu = std::make_unique<MenuApp>();
        SongEntry song;
        song.path = chart_path;
        menu->indexed_songs_.push_back(song);
        menu->visible_song_indices_ = {0};
        menu->selected_song_ = 0;
        menu->config_.skin.score_comparison_enabled = true;
        MenuApp::BestResultRecord best;
        best.has_value = best.replay_verified = true;
        best.replay_path = replay_path;
        best.best_score = 10000;
        best.ex_score = 4;
        const auto keys = menu_songs::build_chart_path_keys(menu->song_absolute_path(song), menu->songs_path_);
        REQUIRE_FALSE(keys.empty());
        menu->chart_score_high_water_[keys.front()].best = best;
        MenuApp::LocalPlayRecord record;
        record.replay_path = replay_path;
        record.ex_score = 4;
        record.pause_used = true;
        record.clear_status = "NORMAL CLEAR";
        menu->local_play_records_.push_back(record);
        menu->current_song_record_indices_ = {0};
        CHECK(menu->score_comparison_replay_path_for_selected_song() == replay_path);
        CommandLineOptions options;
        menu->prepare_score_comparison_replay(session, options);
        CHECK(options.ghost_replay_path == replay_path);
        menu->config_.mode.ghost_battle_enabled = true;
        menu->prepare_score_comparison_replay(session, options);
        CHECK(options.ghost_replay_path == replay_path);
    }
    static void check_selection_and_preview() {
        auto menu = std::make_unique<MenuApp>();
        SongEntry song;
        song.path = "comparison-fixture.bms";
        menu->indexed_songs_.push_back(song);
        menu->visible_song_indices_ = {0};
        menu->selected_song_ = 0;
        menu->config_.mode.mods.clear();
        CHECK(menu->score_comparison_replay_path_for_selected_song().empty());
        MenuApp::BestResultRecord best;
        best.has_value = best.replay_verified = true;
        best.replay_path = "score-best.replay.json";
        best.best_score = 9500;
        best.ex_score = 3;
        const auto keys = menu_songs::build_chart_path_keys(menu->song_absolute_path(song), menu->songs_path_);
        REQUIRE_FALSE(keys.empty());
        menu->chart_score_high_water_[keys.front()].best = best;
        MenuApp::LocalPlayRecord record;
        record.replay_path = best.replay_path;
        record.ex_score = 3;
        record.clear_status = "NORMAL CLEAR";
        menu->local_play_records_.push_back(record);
        CHECK(menu->score_comparison_replay_path_for_selected_song() == best.replay_path);
        // A large historical score does not establish EX evidence. Zero EX,
        // however, is a valid known result and may provide the replay reference.
        auto& selected_best = menu->chart_score_high_water_[keys.front()].best;
        selected_best.ex_score.reset();
        selected_best.best_score = 10000;
        CHECK(menu->score_comparison_replay_path_for_selected_song().empty());
        selected_best.ex_score = 0;
        CHECK(menu->score_comparison_replay_path_for_selected_song() == best.replay_path);
        selected_best = best;
        menu->chart_best_results_[keys.front()] = best;
        menu->chart_best_results_[keys.front()].replay_path = "clear-best.replay.json";
        CHECK(menu->score_comparison_replay_path_for_selected_song() == best.replay_path);
        menu->local_play_records_.front().autoplay_enabled = true;
        CHECK(menu->score_comparison_replay_path_for_selected_song().empty());
        menu->local_play_records_.front().autoplay_enabled = false;
        menu->local_play_records_.front().mods = {"judge_hard"};
        CHECK(menu->score_comparison_replay_path_for_selected_song().empty());
        menu->config_.mode.mods = {"judge_hard"};
        CHECK(menu->score_comparison_replay_path_for_selected_song() == best.replay_path);
        auto visible_ghost = menu->local_play_records_.front();
        visible_ghost.replay_path = "clear-best.replay.json";
        menu->local_play_records_.push_back(visible_ghost);
        menu->current_song_record_indices_ = {1, 0};
        menu->config_.skin.score_comparison_enabled = true;
        menu->config_.mode.ghost_battle_enabled = true;
        auto session = std::make_unique<GameSession>();
        CommandLineOptions options;
        menu->prepare_score_comparison_replay(*session, options);
        CHECK(options.ghost_replay_path == "clear-best.replay.json");
        menu->config_.mode.ghost_battle_enabled = false;
        menu->prepare_score_comparison_replay(*session, options);
        CHECK(options.ghost_replay_path == best.replay_path);
        menu->config_.mode.ghost_battle_enabled = true;
        menu->config_.skin.score_comparison_enabled = false;
        menu->prepare_score_comparison_replay(*session, options);
        CHECK(options.ghost_replay_path == "clear-best.replay.json");
        render::SkinPreviewData preview;
        preview.score_comparison_enabled = true;
        preview.score_comparison_x = 0.27;
        preview.score_comparison_y = 0.68;
        const auto hud = render::make_skin_gameplay_preview(preview);
        CHECK(hud.score_comparison_enabled);
        CHECK(hud.score_comparison_x == 0.27);
        CHECK(hud.score_comparison_y == 0.68);
        CHECK(hud.score_comparison.best_available);
        CHECK(hud.score_comparison.score_difference == 42);
    }
};

struct GameSessionScoreComparisonTestAccess {
    static void check_rank_refresh() {
        auto session = std::make_unique<GameSession>();
        session->config_.skin.score_comparison_enabled = true;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 300000;
        session->chart_.notes = {{1, 48000, std::nullopt}, {1, 144000, std::nullopt}, {1, 240000, std::nullopt}};
        gameplay::GameplayConfig config;
        config.judge.indirect_miss_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, config);
        const auto empty = session->hud_snapshot().score_comparison;
        CHECK_FALSE(empty.rank_available);
        (void)session->engine_->handle_input(1, input::InputState::Pressed, 48000);
        const auto perfect = session->hud_snapshot().score_comparison;
        REQUIRE(perfect.rank_available);
        (void)session->engine_->handle_input(1, input::InputState::Released, 60000);
        (void)session->engine_->handle_input(1, input::InputState::Pressed, 145500);
        const auto great = session->hud_snapshot().score_comparison;
        REQUIRE(session->engine_->stats().counts.gr == 1);
        (void)session->engine_->handle_input(1, input::InputState::Released, 156000);
        session->engine_->advance(300000);
        const auto missed = session->hud_snapshot().score_comparison;
        REQUIRE(session->engine_->stats().counts.pr == 1);
        for (std::size_t i = 0; i < kLiveRankAccuracyTargets.size(); ++i) {
            CHECK(perfect.rank_difference[i] == doctest::Approx(100.0 - kLiveRankAccuracyTargets[i]));
            CHECK(great.rank_difference[i] == doctest::Approx(90.0 - kLiveRankAccuracyTargets[i]));
            CHECK(missed.rank_difference[i] == doctest::Approx(60.0 - kLiveRankAccuracyTargets[i]));
        }
        MenuAppScoreComparisonTestAccess::check_render_publication({empty, perfect, great, missed});
    }
    static void check_independent_reference() {
        struct Files {
            std::filesystem::path chart, replay;
            Files() {
                const auto name = "tenriff-score-reference-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                chart = std::filesystem::temp_directory_path() / (name + ".bms");
                replay = std::filesystem::temp_directory_path() / (name + ".replay.json");
                std::ofstream(chart) << "#TITLE Synthetic score reference fixture\n";
            }
            ~Files() {
                std::error_code ec;
                std::filesystem::remove(chart, ec);
                std::filesystem::remove(replay, ec);
            }
        } files;
        auto session = std::make_unique<GameSession>();
        session->sample_rate_ = 48000;
        session->config_.skin.score_comparison_enabled = true;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 192000;
        session->chart_.notes = {{1, 48000, std::nullopt}, {1, 144000, std::nullopt}};
        session->chart_path_ = files.chart.u8string();
        session->gauge_shift_enabled_ = true;
        session->bms_gauge_version_ = session->bms_control_flow_version_ = 2;
        gameplay::GameplayConfig game_config;
        game_config.sample_rate = 48000;
        game_config.gauge_shift_enabled = true;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, game_config);
        session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, game_config);
        session->set_score_comparison_only(false);
        gameplay::ReplayFile best;
        best.sample_rate = best.trace.sample_rate = 48000;
        best.trace.lane_count = 1;
        best.trace.duration_samples = 192000;
        best.ruleset_id = replay_ruleset_id_for_runtime(session->config_.judge, session->config_.gauge, true, false, "off", {});
        best.chart_sha256 = hash_chart_file(files.chart).sha256;
        best.mode.key_conversion_version = 3;
        best.mode.bms_timing_version = best.mode.bms_gauge_version = best.mode.bms_control_flow_version = 2;
        best.mode.key_mode = session->config_.mode.key_mode;
        best.mode.key_conversion_algorithm = session->config_.mode.key_conversion_algorithm;
        best.mode.key_conversion_nk2_preset = session->config_.mode.key_conversion_nk2_preset;
        best.mode.random = session->config_.mode.random;
        best.mode.random_seed = static_cast<int>(session->config_.mode.random_seed);
        best.mode.gauge = "normal";
        best.pause_used = true;
        best.trace.events = {{1, input::InputState::Pressed, 48000}, {1, input::InputState::Released, 60000},
                             {1, input::InputState::Pressed, 144000}, {1, input::InputState::Released, 156000}};
        CHECK(session->score_reference_matches(best, true));
        CHECK(session->score_reference_matches(best, false));
        auto mismatch = best;
        mismatch.rate = 1.5;
        CHECK_FALSE(session->score_reference_matches(mismatch, true));
        mismatch = best;
        mismatch.mode.random_seed = 1;
        CHECK_FALSE(session->score_reference_matches(mismatch, false));
        mismatch = best;
        mismatch.mode.key_conversion_version = 2;
        CHECK_FALSE(session->score_reference_matches(mismatch, true));
        mismatch = best;
        mismatch.chart_sha256 = std::string(64, '0');
        CHECK_FALSE(session->score_reference_matches(mismatch, true));
        mismatch = best;
        mismatch.mods = {"judge_hard"};
        CHECK_FALSE(session->score_reference_matches(mismatch, true));
        REQUIRE(gameplay::save_replay_json(files.replay.u8string(), best).success());
        MenuAppScoreComparisonTestAccess::prepare_paused_best(*session, files.chart.u8string(), files.replay.u8string());
        session->ghost_replay_source_ = best;
        session->ghost_replay_source_.trace.events = {{1, input::InputState::Pressed, 49500}, {1, input::InputState::Released, 60000}};
        session->ghost_replay_enabled_ = true;
        session->initialize_score_reference(game_config, true);
        REQUIRE(session->score_reference_engine_);
        (void)session->engine_->handle_input(1, input::InputState::Pressed, 48000);
        const auto player_score = session->engine_->stats().raw_score;
        const auto player_events = session->engine_->replay().events.size();
        session->process_ghost_replay_queue(96000, 0);
        session->ghost_engine_->advance(96000);
        REQUIRE(session->ghost_engine_->stats().raw_score == 2500);
        session->advance_score_reference(96000);
        CHECK(session->engine_->stats().raw_score == player_score);
        CHECK(session->engine_->replay().events.size() == player_events);
        CHECK(session->ghost_engine_->stats().raw_score == 2500);
        CHECK(session->ghost_replay_source_.trace.events.size() == 2);
        CHECK(session->ghost_replay_source_.trace.events.front().sample == 49500);
        const auto mid = session->hud_snapshot();
        CHECK(mid.ghost_visible);
        CHECK(mid.ghost_score == 1); // One GR; legacy engine score remains 2500 above.
        CHECK(mid.score_comparison.best_available);
        CHECK(mid.score_comparison.best_score == 2);
        CHECK(mid.score_comparison.score_difference == 0);
        session->paused_.store(true);
        session->pause_used_.store(true);
        const auto frozen_index = session->score_reference_event_index_;
        session->advance_score_reference(192000);
        CHECK(session->score_reference_event_index_ == frozen_index);
        CHECK(session->score_reference_engine_->stats().raw_score == 5000);
        session->paused_.store(false);
        session->advance_score_reference(192000);
        CHECK(session->score_reference_engine_->stats().raw_score == 10000);
        const auto end = session->hud_snapshot();
        CHECK(end.score_comparison.live);
        CHECK(end.score_comparison.best_available);
        CHECK(end.score_comparison.score_difference == -2);
        MenuAppScoreComparisonTestAccess::check_render_publication({mid.score_comparison, end.score_comparison});
        CHECK(session->ghost_engine_->stats().raw_score == 2500);
        CHECK(session->engine_->stats().raw_score == player_score);
        session->reset_score_reference();
        CHECK_FALSE(session->score_reference_engine_);
        CHECK(session->score_reference_event_index_ == 0);
        CHECK(session->score_reference_source_.trace.events.empty());
        session->initialize_score_reference(game_config, true);
        REQUIRE(session->score_reference_engine_);
        session->advance_score_reference(96000);
        CHECK(session->score_reference_engine_->stats().raw_score == 5000);
        CHECK(session->ghost_engine_->stats().raw_score == 2500);

        // Drive the actual audio callback, including its physical-to-logical
        // pause offset, instead of advancing a reference engine directly.
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, game_config);
        session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, game_config);
        session->ghost_replay_event_index_ = 0;
        session->chronological_judgement_ = false;
        session->ghost_chronological_judgement_ = false;
        session->pause_used_.store(false);
        session->initialize_score_reference(game_config, true);
        REQUIRE(session->score_reference_engine_);
        float output[2]{};
        session->audio_callback(output, 1, 47999, 47999);
        CHECK(session->hud_snapshot().score_comparison.best_score == 0);
        (void)session->engine_->handle_input(1, input::InputState::Pressed, 48000);
        session->audio_callback(output, 1, 48000, 48000);
        CHECK(session->hud_snapshot().score_comparison.best_score == 2);
        session->audio_callback(output, 1, 50000, 50000);
        CHECK(session->hud_snapshot().ghost_score == 1);
        session->pause_used_.store(true);
        session->paused_.store(true);
        session->audio_callback(output, 1, 96000, 96000);
        const auto paused = session->hud_snapshot();
        CHECK(paused.current_sample == 96000);
        CHECK(paused.score_comparison.live);
        CHECK(paused.score_comparison.best_score == 2);
        session->audio_callback(output, 1, 144000, 144000);
        CHECK(session->hud_snapshot().current_sample == paused.current_sample);
        CHECK(session->hud_snapshot().score_comparison.best_score == 2);
        session->pause_resume_requested_.store(true);
        session->audio_callback(output, 1, 144000, 144000);
        REQUIRE(session->pause_resume_end_sample_ > 144000);
        const auto resume_sample = session->pause_resume_end_sample_;
        session->audio_callback(output, 1, resume_sample, resume_sample);
        CHECK_FALSE(session->paused_.load());
        CHECK(session->hud_snapshot().score_comparison.best_score == 2);
        const auto physical_second_note = session->pause_sample_offset_ + 144000;
        session->audio_callback(output, 1, physical_second_note, physical_second_note);
        const auto resumed = session->hud_snapshot();
        CHECK(resumed.current_sample == 144000);
        CHECK(resumed.score_comparison.live);
        CHECK(resumed.score_comparison.best_available);
        CHECK(resumed.score_comparison.best_score == 4);
        CHECK(resumed.score_comparison.score_difference == -2);
        CHECK(resumed.score == 2); // One PG, independent of the two-note normalization.
        CHECK(resumed.ghost_score == 1);
    }
    static void check_hidden_snapshot() {
        auto session = std::make_unique<GameSession>();
        session->config_.skin.score_comparison_enabled = true;
        session->chart_.lane_count = 1;
        session->chart_.duration_samples = 192000;
        session->chart_.notes = {{1, 48000, std::nullopt}, {1, 144000, std::nullopt}};
        gameplay::GameplayConfig game_config;
        game_config.sample_rate = 48000;
        session->engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, game_config);
        session->ghost_engine_ = std::make_unique<gameplay::GameplayEngine>(session->chart_, game_config);
        session->score_comparison_compatible_ = true;
        session->set_score_comparison_only(true);
        (void)session->engine_->handle_input(1, input::InputState::Pressed, 48000);
        (void)session->ghost_engine_->handle_input(1, input::InputState::Pressed, 48000);
        const auto before_score = session->engine_->stats().raw_score;
        const auto before_events = session->engine_->replay().events.size();
        const auto before_mode = config::serialize_skin_config(session->config_.skin);
        const auto hidden = session->hud_snapshot();
        CHECK_FALSE(hidden.ghost_visible);
        CHECK(hidden.score_comparison.live);
        CHECK(hidden.score_comparison.best_available);
        CHECK(hidden.score_comparison.best_score == 2);
        CHECK(hidden.score_comparison.score_difference == 0);
        CHECK(session->engine_->stats().raw_score == before_score);
        CHECK(session->engine_->replay().events.size() == before_events);
        CHECK(config::serialize_skin_config(session->config_.skin) == before_mode);
        session->set_score_comparison_only(false);
        CHECK(session->hud_snapshot().ghost_visible);
        session->score_comparison_compatible_ = false;
        CHECK_FALSE(session->hud_snapshot().score_comparison.best_available);
        session->autoplay_enabled_ = true;
        CHECK_FALSE(session->hud_snapshot().score_comparison.live);
        session->autoplay_enabled_ = false;
        session->practice_start_seconds_ = 1;
        CHECK_FALSE(session->hud_snapshot().score_comparison.live);
        session->practice_start_seconds_ = 0;
        session->score_comparison_compatible_ = true;
        session->pause_used_.store(true);
        CHECK(session->hud_snapshot().score_comparison.live);
        CHECK(session->hud_snapshot().score_comparison.best_available);
    }
};
}

TEST_CASE("live comparison uses score high water instead of the clear record and previews its saved position") {
    tenriff::app::MenuAppScoreComparisonTestAccess::check_selection_and_preview();
}

TEST_CASE("hidden score replay HUD cannot show a ghost or mutate the player's score and trace") {
    tenriff::app::GameSessionScoreComparisonTestAccess::check_hidden_snapshot();
}

TEST_CASE("independent score best and visible clear ghost advance separately without altering player or ghost") {
    tenriff::app::GameSessionScoreComparisonTestAccess::check_independent_reference();
}

TEST_CASE("live rank targets publish PG GR and missed-note changes through the gameplay render cache") {
    tenriff::app::GameSessionScoreComparisonTestAccess::check_rank_refresh();
}
#endif

TEST_CASE("live comparison availability and each independent value invalidate HUD text without motion") {
    using namespace tenriff::app;
    GameplayHudRevisionInput before;
    before.score_comparison = {true, true, true, 5000, 100, {5.0, 3.0, 2.0, 1.0}};
    const auto expect_text_refresh = [&](const GameplayHudRevisionInput& after) {
        const auto diff = diff_gameplay_hud_revisions(before, after);
        CHECK(diff.text_changed);
        CHECK_FALSE(diff.motion_changed);
    };
    auto after = before;
    after.score_comparison.live = false;
    expect_text_refresh(after);
    after = before;
    after.score_comparison.best_available = false;
    expect_text_refresh(after);
    after = before;
    after.score_comparison.rank_available = false;
    expect_text_refresh(after);
    after = before;
    ++after.score_comparison.best_score;
    expect_text_refresh(after);
    after = before;
    --after.score_comparison.score_difference;
    expect_text_refresh(after);
    for (std::size_t i = 0; i < kLiveRankNames.size(); ++i) {
        after = before;
        after.score_comparison.rank_difference[i] -= 0.01;
        expect_text_refresh(after);
    }
    CHECK_FALSE(diff_gameplay_hud_revisions(before, before).text_changed);
}

TEST_CASE("live comparison distinguishes missing best inactive and exact ties") {
    ResultStats current;
    current.record_note_total(100);
    current.record_judgement(tenriff::game::Judgement::PG, 0, tenriff::gameplay::ComboImpact::Increment);
    CHECK_FALSE(live_score_comparison(current, 1, false, &current).live);
    auto missing = live_score_comparison(current, 1, true);
    CHECK(missing.live);
    CHECK_FALSE(missing.best_available);
    CHECK(missing.rank_available);
    auto tied = live_score_comparison(current, 1, true, &current);
    CHECK(tied.best_available);
    CHECK(tied.score_difference == 0);
    ResultStats legacy_only = current;
    legacy_only.raw_score += 9999;
    CHECK(live_score_comparison(legacy_only, 9, true, &current, .1).score_difference == 0);
    ResultStats other = current;
    other.counts.gr += 1;
    other.raw_score += 9999;
    CHECK(live_score_comparison(other, 1, true, &current).score_difference == 1);
    CHECK(live_score_comparison(current, 1, true, &other).score_difference == -1);
    CHECK(live_score_comparison(current, 0.5, true, &current, 1).score_difference == 0);
}

TEST_CASE("rank tracker follows categorical accuracy instead of native score") {
    ResultStats stats;
    stats.record_note_total(100);
    for (int i = 0; i < 10; ++i)
        stats.record_judgement(tenriff::game::Judgement::GR, 30, tenriff::gameplay::ComboImpact::Increment);
    for (int i = 0; i < 90; ++i)
        stats.record_judgement(tenriff::game::Judgement::PG, 0, tenriff::gameplay::ComboImpact::Increment);
    REQUIRE(stats.raw_score == 9500);
    REQUIRE(stats.accuracy_percent() == doctest::Approx(98));
    CHECK(tenriff::app::menu_records::calculate_rank(stats, false, "NORMAL CLEAR") == "S++");
    const auto result = live_score_comparison(stats, 0.75, true);
    CHECK(result.rank_difference[0] == doctest::Approx(3));
    CHECK(result.rank_difference[1] == doctest::Approx(1));
    CHECK(result.rank_difference[2] == doctest::Approx(0));
    CHECK(result.rank_difference[3] == doctest::Approx(-1));
    ResultStats empty;
    CHECK_FALSE(live_score_comparison(empty, 1, true).rank_available);
}

TEST_CASE("live best comparison advances an actual replay engine to the shared chart sample") {
    using namespace tenriff;
    gameplay::GameplayChart chart;
    chart.lane_count = 1;
    chart.duration_samples = 4000;
    chart.notes = {{1, 1000, std::nullopt}, {1, 3000, std::nullopt}};
    gameplay::GameplayConfig config;
    config.sample_rate = 1000;
    gameplay::GameplayEngine player(chart, config), best(chart, config);
    (void)player.handle_input(1, input::InputState::Pressed, 1000);
    (void)best.handle_input(1, input::InputState::Pressed, 1000);
    player.advance(2000);
    best.advance(2000);
    const auto mid = live_score_comparison(player.stats(), 1, true, &best.stats());
    CHECK(mid.best_score == 2);
    CHECK(mid.score_difference == 0);
    (void)player.handle_input(1, input::InputState::Released, 2001);
    (void)best.handle_input(1, input::InputState::Released, 2001);
    (void)best.handle_input(1, input::InputState::Pressed, 3000);
    player.advance(4000);
    best.advance(4000);
    const auto end = live_score_comparison(player.stats(), 1, true, &best.stats());
    CHECK(end.best_score == 4);
    CHECK(end.score_difference == -2);
}

TEST_CASE("score comparison excludes historical or assisted replay policies") {
    const auto compatible = [](std::string_view ruleset = "tenriff-r13", int conversion = 3,
        int timing = 2, int gauge = 2, int flow = 2, bool assisted = false, bool aborted = false) {
        return live_score_replay_policy_matches(ruleset, "tenriff-r13", conversion, 3, timing, 2,
                                               gauge, 2, flow, 2, assisted, aborted);
    };
    CHECK(compatible());
    CHECK_FALSE(compatible("tenriff-r11"));
    CHECK_FALSE(compatible("tenriff-r13", 2));
    CHECK_FALSE(compatible("tenriff-r13", 3, 1));
    CHECK_FALSE(compatible("tenriff-r13", 3, 2, 1));
    CHECK_FALSE(compatible("tenriff-r13", 3, 2, 2, 1));
    CHECK_FALSE(compatible("tenriff-r13", 3, 2, 2, 2, true));
    CHECK_FALSE(compatible("tenriff-r13", 3, 2, 2, 2, false, true));
    CHECK_FALSE(live_score_replay_policy_matches("custom-ruleset-13", "custom-ruleset-13",
                                               3, 3, 2, 2, 2, 2, 2, 2, false, false));
    CHECK_FALSE(live_score_replay_policy_matches("", "", 3, 3, 2, 2, 2, 2, 2, 2, false, false));
}

TEST_CASE("comparison settings are opt in bounded and portable with skin presets") {
    tenriff::config::ConfigLoader loader;
    auto skin = loader.defaults().skin;
    CHECK_FALSE(skin.score_comparison_enabled);
    CHECK(skin.score_comparison_x == 0);
    CHECK(skin.score_comparison_y == doctest::Approx(0.28));
    skin.score_comparison_enabled = true;
    skin.score_comparison_x = 0.39;
    skin.score_comparison_y = 0.87;
    tenriff::config::SkinConfig restored;
    REQUIRE(tenriff::config::deserialize_skin_config(tenriff::config::serialize_skin_config(skin), restored));
    CHECK(restored.score_comparison_enabled);
    CHECK(restored.score_comparison_x == doctest::Approx(0.39));
    CHECK(restored.score_comparison_y == doctest::Approx(0.87));
    skin.score_comparison_x = -2;
    skin.score_comparison_y = 3;
    REQUIRE(tenriff::config::deserialize_skin_config(tenriff::config::serialize_skin_config(skin), restored));
    CHECK(restored.score_comparison_x == 0);
    CHECK(restored.score_comparison_y == 1);
    using namespace tenriff::app;
    using namespace tenriff::app::menu;
    menu::settings::SkinSettingsController controller;
    tenriff::config::RuntimeConfig runtime;
    controller.reset("4k");
    (void)controller.handle(MenuAction::activate(), runtime, {}, {}, SkinSettingsRowId::ScoreComparisonEnabled);
    CHECK(runtime.skin.score_comparison_enabled);
    (void)controller.handle(MenuAction::set_ratio(0.326), runtime, {}, {}, SkinSettingsRowId::ScoreComparisonX);
    CHECK(runtime.skin.score_comparison_x == doctest::Approx(0.33));
    (void)controller.handle(MenuAction::adjust(1), runtime, {}, {}, SkinSettingsRowId::ScoreComparisonY);
    CHECK(runtime.skin.score_comparison_y == doctest::Approx(0.29));
    CHECK(skin_settings_category(SkinSettingsRowId::ScoreComparisonX) == SkinSettingsCategory::Hud);
}

TEST_CASE("live EX comparison ignores contradictory normalized totals and score multipliers") {
    ResultStats current, best;
    current.counts.pg = 100; // EX 200
    current.raw_score = 1;
    best.counts.gr = 199; // EX 199
    best.raw_score = 10000;
    for (double multiplier : {.25, 1.0, 4.0}) {
        const auto ahead = live_score_comparison(current, multiplier, true, &best, 8.0);
        CHECK(ahead.best_available);
        CHECK(ahead.best_score == 199);
        CHECK(ahead.score_difference == 1);
        CHECK(live_score_comparison(best, 8.0, true, &current, multiplier).score_difference == -1);
    }
    ResultStats zero;
    zero.raw_score = 10000; // No PG/GR evidence in this current engine snapshot.
    const auto known_zero = live_score_comparison(zero, 1, true, &zero);
    CHECK(known_zero.best_available);
    CHECK(known_zero.best_score == 0);
    CHECK(known_zero.score_difference == 0);
    CHECK_FALSE(live_score_comparison(zero, 1, true).best_available);
}
