#include "doctest/doctest.h"

#include <set>
#include <vector>

#include "app/SessionMix.h"

using tenriff::app::SessionMixCandidate;
using tenriff::app::SessionMixDraftEntry;
using tenriff::app::SessionMixPhase;
using tenriff::app::build_session_mix_draft_plan;
using tenriff::app::build_session_mix_plan;
using tenriff::app::cycle_session_mix_minutes;
using tenriff::app::session_mix_phase_counts;

TEST_CASE("session mix exposes the three supported target lengths") {
    CHECK(cycle_session_mix_minutes(15, 1) == 30);
    CHECK(cycle_session_mix_minutes(30, 1) == 60);
    CHECK(cycle_session_mix_minutes(60, 1) == 15);
    CHECK(cycle_session_mix_minutes(15, -1) == 60);

    CHECK(session_mix_phase_counts(15, 99).warmup == 1);
    CHECK(session_mix_phase_counts(15, 99).challenge == 3);
    CHECK(session_mix_phase_counts(15, 99).cooldown == 1);
    CHECK(session_mix_phase_counts(30, 99).total() == 10);
    CHECK(session_mix_phase_counts(60, 99).total() == 20);
}

TEST_CASE("session mix draft plan preserves authored order and repeats") {
    const std::vector<SessionMixDraftEntry> draft = {
        {"first.bms", "a", "First", 7, 3},
        {"second.bms", "b", "Second", 7, 7},
        {"first.bms", "a", "First", 7, 3},
    };

    const auto plan = build_session_mix_draft_plan(draft);
    REQUIRE(plan.entries.size() == 3u);
    CHECK(plan.entries[0].chart_id == "first.bms");
    CHECK(plan.entries[1].chart_id == "second.bms");
    CHECK(plan.entries[2].chart_id == "first.bms");
    CHECK(plan.entries[0].phase == SessionMixPhase::Challenge);
}

TEST_CASE("session mix keeps every selected chart unique and preserves phase order") {
    std::vector<SessionMixCandidate> candidates;
    for (int level = 1; level <= 20; ++level) {
        candidates.push_back(SessionMixCandidate{
            "chart-" + std::to_string(level), level, level <= 8, level <= 8, 93.0});
    }
    candidates.push_back(candidates.front());

    const auto plan = build_session_mix_plan(candidates, 30, 77);
    REQUIRE(plan.entries.size() == 10);

    std::set<std::string> ids;
    bool challenge_seen = false;
    bool cooldown_seen = false;
    for (const auto& entry : plan.entries) {
        CHECK(ids.insert(entry.chart_id).second);
        if (entry.phase == SessionMixPhase::Challenge) {
            challenge_seen = true;
        }
        if (entry.phase == SessionMixPhase::Cooldown) {
            cooldown_seen = true;
        }
        if (challenge_seen) {
            CHECK(entry.phase != SessionMixPhase::Warmup);
        }
        if (cooldown_seen) {
            CHECK(entry.phase == SessionMixPhase::Cooldown);
        }
    }
}

TEST_CASE("session mix uses local clears to place challenge songs above warmup") {
    std::vector<SessionMixCandidate> candidates;
    for (int level = 1; level <= 18; ++level) {
        const bool cleared = level <= 8;
        candidates.push_back(SessionMixCandidate{
            "level-" + std::to_string(level), level, cleared, cleared, cleared ? 94.0 : 0.0});
    }

    const auto plan = build_session_mix_plan(candidates, 30, 1234);
    REQUIRE(plan.entries.size() == 10);

    int warmup_sum = 0;
    int warmup_count = 0;
    int challenge_sum = 0;
    int challenge_count = 0;
    for (const auto& entry : plan.entries) {
        if (entry.phase == SessionMixPhase::Warmup) {
            warmup_sum += entry.difficulty;
            ++warmup_count;
        } else if (entry.phase == SessionMixPhase::Challenge) {
            challenge_sum += entry.difficulty;
            ++challenge_count;
        }
    }
    REQUIRE(warmup_count > 0);
    REQUIRE(challenge_count > 0);
    CHECK(challenge_sum * warmup_count > warmup_sum * challenge_count);
}

TEST_CASE("session mix gracefully shrinks to a small filtered library") {
    const std::vector<SessionMixCandidate> candidates = {
        {"one", 4, true, true, 95.0},
        {"two", 5, false, false, 0.0},
    };
    const auto plan = build_session_mix_plan(candidates, 60, 1);
    REQUIRE(plan.entries.size() == 2);
    CHECK(plan.entries[0].phase == SessionMixPhase::Warmup);
    CHECK(plan.entries[1].phase == SessionMixPhase::Challenge);
}

TEST_CASE("session mix deduplicates music variants and is stable under input reordering") {
    std::vector<SessionMixCandidate> candidates;
    for (int song = 0; song < 8; ++song) for (int diff = 3; diff < 7; ++diff)
        candidates.push_back({std::to_string(song) + "-" + std::to_string(diff), diff,
            true, true, 94.0, "music-" + std::to_string(song)});
    const auto plan = build_session_mix_plan(candidates, 60, 42);
    REQUIRE(plan.entries.size() == 8);
    std::set<std::string> music;
    for (const auto& entry : plan.entries) CHECK(music.insert(entry.chart_id.substr(0, 1)).second);
    std::reverse(candidates.begin(), candidates.end());
    const auto reversed = build_session_mix_plan(candidates, 60, 42);
    REQUIRE(reversed.entries.size() == plan.entries.size());
    for (size_t i = 0; i < plan.entries.size(); ++i) CHECK(reversed.entries[i].chart_id == plan.entries[i].chart_id);
}

TEST_CASE("session mix ignores unreliable clear anchors and rerolls similar candidates") {
    using tenriff::app::session_mix_detail::player_difficulty_anchor;
    std::vector<SessionMixCandidate> candidates;
    for (int level = 1; level <= 20; ++level) candidates.push_back({std::to_string(level), level});
    CHECK(player_difficulty_anchor(candidates) == 5);
    candidates.push_back({"invalid", 99, true, true, std::numeric_limits<double>::quiet_NaN()});
    candidates.push_back({"invalid2", 99, true, true, 101});
    candidates.push_back({"poor-clear", 99, true, true, 70});
    candidates.push_back({"reliable", 8, true, true, 94});
    CHECK(player_difficulty_anchor(candidates) == 8);
    candidates.clear();
    for (int song = 0; song < 30; ++song) candidates.push_back({std::to_string(song), 6, true, true, 94});
    std::set<std::string> first_picks;
    for (int seed = 0; seed < 12; ++seed) first_picks.insert(build_session_mix_plan(candidates, 15, seed).entries[0].chart_id);
    CHECK(first_picks.size() > 1);
}

TEST_CASE("session mix phases prefer a controlled rise then cooldown when the library permits it") {
    std::vector<SessionMixCandidate> candidates;
    for (int level = 1; level <= 15; ++level) for (int song = 0; song < 15; ++song)
        candidates.push_back({std::to_string(level) + "-" + std::to_string(song), level,
            level == 8, level == 8, 94});
    const auto plan = build_session_mix_plan(candidates, 60, 42);
    REQUIRE(plan.entries.size() == 20);
    int max_challenge = 0;
    for (size_t i = 0; i < plan.entries.size(); ++i) {
        const auto& stage = plan.entries[i];
        if (stage.phase == SessionMixPhase::Warmup) CHECK(stage.difficulty <= 8);
        if (stage.phase == SessionMixPhase::Challenge) { CHECK(stage.difficulty <= 10); max_challenge = std::max(max_challenge, stage.difficulty); }
        if (stage.phase == SessionMixPhase::Cooldown) CHECK(stage.difficulty <= 8);
        if (i) CHECK(std::abs(stage.difficulty - plan.entries[i-1].difficulty) <= 3);
    }
    CHECK(max_challenge > plan.entries.front().difficulty);
    CHECK(plan.entries.back().difficulty < max_challenge);
}

#ifdef _WIN32
#include "app/MenuApp.h"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <memory>

namespace tenriff::app {
struct MenuAppSessionMixTestAccess {
    struct Fixture {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
            ("tenriff-mix-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::unique_ptr<MenuApp> app = std::make_unique<MenuApp>();
        Fixture() {
            std::filesystem::create_directories(root);
            app->config_.ui.language = "en";
            app->config_.audio_ui.menu_music_volume = 0.0;
            app->key_tab_=9; app->key_enter_=13; app->key_escape_=27; app->key_backspace_=8;
            app->key_up_=38; app->key_down_=40; app->key_left_=37; app->key_right_=39;
            app->key_page_up_=33; app->key_page_down_=34;
            app->reset_screen(menu::Screen::SessionMix);
            for (int i=0; i<3; ++i) {
                SongEntry song;
                song.path = (root / (std::to_string(i) + ".bms")).u8string();
                song.title = "Stage " + std::to_string(i); song.md5 = std::string(32, 'a'+i);
                song.key_count = 7; song.level = 5+i;
                std::ofstream(song.path) << "#TITLE Test\n#BPM 120\n";
                app->indexed_songs_.push_back(song);
                app->visible_song_indices_.push_back(i);
            }
            Lr2CourseDefinition course;
            course.title="Ordered"; course.key_count=7;
            course.chart_md5={std::string(32,'b'),std::string(32,'a'),std::string(32,'b')};
            app->session_mix_lr2_courses_.push_back(course);
            course.title="Missing middle"; course.chart_md5[1]=std::string(32,'d');
            app->session_mix_lr2_courses_.push_back(course);
            app->select_session_mix_course(2);
        }
        ~Fixture() { app.reset(); std::error_code ec; std::filesystem::remove_all(root,ec); }
    };
    static void order_and_missing() {
        Fixture f; auto& app=*f.app;
        app.visible_song_indices_.clear(); // LR2 resolves against the indexed library, not the view filter.
        render::MenuRenderData view;
        app.populate_session_mix_render_data(view);
        REQUIRE(view.session_mix.can_start);
        REQUIRE(view.session_mix.stages.size()==3);
        CHECK(view.session_mix.stages[0].title=="Stage 1");
        CHECK(view.session_mix.stages[1].title=="Stage 0");
        CHECK(view.session_mix.stages[2].title=="Stage 1");
        REQUIRE(app.prepare_session_mix_start());
        REQUIRE(app.session_mix_plan_.entries.size()==3);
        CHECK(app.session_mix_plan_.entries[0].chart_id==app.indexed_songs_[1].path);
        CHECK(app.session_mix_plan_.entries[1].chart_id==app.indexed_songs_[0].path);
        CHECK(app.session_mix_plan_.entries[2].chart_id==app.indexed_songs_[1].path);
        app.select_session_mix_course(3);
        app.populate_session_mix_render_data(view);
        CHECK_FALSE(view.session_mix.can_start);
        CHECK_FALSE(view.session_mix.stages[0].missing);
        CHECK(view.session_mix.stages[1].missing);
        CHECK_FALSE(view.session_mix.stages[2].missing);
        CHECK_FALSE(app.prepare_session_mix_start());
        CHECK_FALSE(app.session_mix_active_);
        app.session_mix_draft_={{app.indexed_songs_[2].path, "", "Draft", 7, 7}};
        app.select_session_mix_course(1);
        REQUIRE(app.prepare_session_mix_start());
        app.indexed_songs_.pop_back(); ++app.song_index_revision_;
        app.populate_session_mix_render_data(view); CHECK_FALSE(view.session_mix.can_start);
        CHECK_FALSE(app.prepare_session_mix_start());
        app.select_session_mix_course(2);
        std::filesystem::remove(f.root / "0.bms");
        CHECK_FALSE(app.prepare_session_mix_start());
        CHECK(app.session_mix_status_message_.find("unavailable")!=std::string::npos);
    }
    static void stable_preview_and_navigation() {
        Fixture f; auto& app=*f.app;
        app.select_session_mix_course(0);
        render::MenuRenderData view;
        app.populate_session_mix_render_data(view);
        const auto preview=app.session_mix_auto_plan_;
        app.populate_session_mix_render_data(view);
        REQUIRE(app.prepare_session_mix_start());
        REQUIRE(preview.entries.size()==app.session_mix_plan_.entries.size());
        for (size_t i=0;i<preview.entries.size();++i)
            CHECK(preview.entries[i].chart_id==app.session_mix_plan_.entries[i].chart_id);
        app.handle_session_mix_input(38);
        CHECK(app.session_mix_source_index_==3);
        app.handle_session_mix_input(40);
        CHECK(app.session_mix_source_index_==0);
        app.handle_session_mix_input(9); CHECK(app.session_mix_focus_==1);
        app.settings_cursor_=0; app.handle_session_mix_input(39);
        CHECK(app.session_mix_minutes_==60);
        app.handle_session_mix_input(9); CHECK(app.session_mix_focus_==2);
        app.handle_session_mix_input(9); CHECK(app.session_mix_focus_==0);
        render::MenuClickEvent click; click.kind=render::MenuHitTargetKind::SongCard; click.index=2;
        CHECK(app.handle_session_mix_click(click));
        CHECK(app.session_mix_source_index_==2); CHECK_FALSE(app.session_mix_active_);
        click.index=3; click.double_click=true;
        CHECK(app.handle_session_mix_click(click)); CHECK_FALSE(app.session_mix_active_);
        CHECK(app.current_screen()==menu::Screen::SessionMix);
    }
    static void refresh_and_finish() {
        Fixture f; auto& app=*f.app;
        app.select_session_mix_course(3);
        render::MenuRenderData view;
        app.populate_session_mix_render_data(view); CHECK_FALSE(view.session_mix.can_start);
        app.indexed_songs_[2].md5=std::string(32,'d'); ++app.song_index_revision_;
        app.populate_session_mix_render_data(view); CHECK(view.session_mix.can_start);
        app.session_mix_lr2_courses_[0].chart_md5.resize(12,std::string(32,'a'));
        app.session_mix_course_matches_.clear(); app.select_session_mix_course(2);
        app.populate_session_mix_render_data(view);
        render::MenuClickEvent click; click.kind=render::MenuHitTargetKind::SessionMixStagePage; click.index=1;
        app.handle_session_mix_click(click);
        CHECK(app.session_mix_stage_offset_==5);
        app.handle_session_mix_click(click); CHECK(app.session_mix_stage_offset_==5);
        REQUIRE(app.prepare_session_mix_start());
        app.session_mix_cursor_=app.session_mix_plan_.entries.size()-1;
        app.session_mix_active_=true; app.last_game_over_=false;
        app.advance_session_mix_from_result();
        CHECK_FALSE(app.session_mix_active_);
        CHECK(app.current_screen()==menu::Screen::SessionMix);
        CHECK(app.session_mix_status_message_.find("COMPLETE")!=std::string::npos);
    }
};
}
TEST_CASE("session mix browser preserves course order and blocks missing stages") {
    tenriff::app::MenuAppSessionMixTestAccess::order_and_missing();
}
TEST_CASE("session mix browser previews the exact plan and handles course selection") {
    tenriff::app::MenuAppSessionMixTestAccess::stable_preview_and_navigation();
}
TEST_CASE("session mix browser refreshes library matches pages stages and returns after completion") {
    tenriff::app::MenuAppSessionMixTestAccess::refresh_and_finish();
}
#endif
