#include "app/MenuApp.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuSongUtils.h"
#include "timing/HighResClock.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace tenriff::app {
namespace {
int mix_difficulty(const SongEntry& song) {
    if (song.native_level > 0) return song.native_level;
    if (song.level > 0) return song.level;
    return std::isfinite(song.rating) && song.rating > 0.0 && song.rating < 100000.0
        ? static_cast<int>(std::llround(song.rating)) : 0;
}
std::string mix_path_key(const std::string& path) {
    return menu_songs::normalize_path_key(menu_song_select::path_from_utf8(path));
}
}

void MenuApp::refresh_session_mix_auto_plan() {
    if (!session_mix_auto_dirty_) return;
    std::vector<SessionMixCandidate> candidates;
    candidates.reserve(visible_song_count());
    for (std::size_t i = 0; i < visible_song_count(); ++i) {
        const auto* song = visible_song_entry(i);
        if (!song) continue;
        const auto path = song_absolute_path(*song);
        if (path.empty()) continue;
        const auto best = best_result_for_song_entry(*song);
        // Alternate difficulties normally share audio. BMS without a preview
        // shares its folder/title instead; authored courses never use this dedupe.
        const auto parent = menu_song_select::path_from_utf8(path).parent_path();
        auto music_path = song->audio_preview_path.empty()
            ? parent / menu_song_select::path_from_utf8(song->title)
            : menu_song_select::path_from_utf8(song->audio_preview_path);
        if (!song->audio_preview_path.empty() && music_path.is_relative()) music_path = parent / music_path;
        auto music = music_path.u8string();
        if (music.empty()) music = path;
        candidates.push_back({path, mix_difficulty(*song), best.has_value,
            best.has_value && !best.game_over && best.clear_status != "AUTO" && best.clear_status != "ASSIST",
            best.accuracy, mix_path_key(music)});
    }
    session_mix_auto_plan_ = build_session_mix_plan(std::move(candidates), session_mix_minutes_,
        static_cast<uint32_t>(timing::HighResClock::now_ns()));
    session_mix_auto_dirty_ = false;
}

void MenuApp::refresh_session_mix_course_matches() {
    if (session_mix_match_revision_ == song_index_revision_ &&
        session_mix_course_matches_.size() == session_mix_lr2_courses_.size()) return;
    // Build one hash index for the whole course file, only when library/import changes.
    session_mix_course_matches_ = match_lr2_courses(session_mix_lr2_courses_, indexed_songs_);
    // Avoid filesystem/path normalization across the entire library on each
    // cursor move. Store indices, never pointers invalidated by an index refresh.
    session_mix_song_paths_.clear();
    session_mix_song_paths_.reserve(indexed_songs_.size());
    for (std::size_t i = 0; i < indexed_songs_.size(); ++i)
        session_mix_song_paths_.try_emplace(mix_path_key(song_absolute_path(indexed_songs_[i])), i);
    session_mix_match_revision_ = song_index_revision_;
}

void MenuApp::select_session_mix_course(int index) {
    const int count = static_cast<int>(session_mix_lr2_courses_.size()) + 2;
    session_mix_source_index_ = std::clamp(index, 0, count - 1);
    session_mix_stage_offset_ = 0;
    session_mix_focus_ = 0;
    session_mix_status_message_.clear();
    reset_song_select_repeat();
}

void MenuApp::populate_session_mix_render_data(render::MenuRenderData& render) {
    using namespace menu_song_select;
    refresh_session_mix_auto_plan();
    refresh_session_mix_course_matches();
    session_mix_source_index_ = std::clamp(session_mix_source_index_, 0,
        static_cast<int>(session_mix_lr2_courses_.size()) + 1);
    render.kind = render::MenuScreenKind::SongSelect;
    render.song_select = {};
    render.song_select.profile = profile_display_name();
    render.song_select.profile_avatar_path = config_.ui.profile_avatar_path;
    auto& view = render.session_mix;
    view = {};
    view.active = true;
    view.controls_focused = session_mix_focus_ == 1;
    view.stages_focused = session_mix_focus_ == 2;
    view.control_cursor = settings_cursor_;
    view.selected = session_mix_source_index_;
    view.source = filename_only(config_.ui.session_mix_lr2_course_path);
    view.courses.push_back({ui_text("AUTO MIX", "자동 믹스"), "", view.selected == 0,
        std::to_string(session_mix_minutes_) + ui_text(" MIN", "분") + " · " +
        std::to_string(session_mix_auto_plan_.entries.size()) + ui_text(" STAGES", " 스테이지")});
    view.courses.push_back({ui_text("DRAFT COURSE", "초안 코스"), "", view.selected == 1,
        std::to_string(session_mix_draft_.size()) + ui_text(" STAGES", " 스테이지")});
    for (std::size_t i = 0; i < session_mix_lr2_courses_.size(); ++i) {
        const auto& course = session_mix_lr2_courses_[i];
        const auto& match = session_mix_course_matches_[i];
        view.courses.push_back({safe_ui_text(course.title, "Untitled"), "", view.selected == static_cast<int>(i) + 2,
            std::to_string(course.key_count) + "K · " + std::to_string(match.song_indices.size()) + "/" +
            std::to_string(course.chart_md5.size()) + " · " +
            (match.playable() ? ui_text("READY", "입장 가능") : ui_text("MISSING", "채보 누락"))});
    }
    view.title = view.courses[view.selected].label;
    const auto append_stage = [&](const SongEntry* song, const std::string& missing, const std::string& phase) {
        render::SessionMixStageData stage;
        stage.missing = !song;
        stage.title = song ? safe_ui_text(song->title, filename_only(song->path)) : ui_text("Missing chart", "누락 채보");
        stage.detail = song ? std::to_string(song->key_count) + "K · LV " +
            std::to_string(mix_difficulty(*song)) + (phase.empty() ? "" : " · " + phase)
            : missing;
        view.stages.push_back(std::move(stage));
    };
    if (view.selected >= 2) {
        const auto& course = session_mix_lr2_courses_[view.selected - 2];
        const auto& match = session_mix_course_matches_[view.selected - 2];
        view.can_start = match.playable();
        for (std::size_t i = 0; i < course.chart_md5.size(); ++i)
            append_stage(match.stage_song_indices[i] ? &indexed_songs_[*match.stage_song_indices[i]] : nullptr,
                         course.chart_md5[i], "");
        view.summary = ui_text("Original course order · continuous gauge", "코스 원래 순서 · 연속 게이지");
    } else if (view.selected == 1) {
        view.can_start = !session_mix_draft_.empty();
        for (const auto& draft : session_mix_draft_) {
            const auto song = session_mix_song_paths_.find(mix_path_key(draft.chart_id));
            const bool found = song != session_mix_song_paths_.end();
            append_stage(found ? &indexed_songs_[song->second] : nullptr, draft.title, "");
            view.can_start &= found;
        }
        view.summary = ui_text("Draft order and repeated stages are preserved.", "초안 순서와 반복 스테이지를 유지합니다.");
    } else {
        view.can_start = !session_mix_auto_plan_.entries.empty();
        for (const auto& entry : session_mix_auto_plan_.entries) {
            const auto song = session_mix_song_paths_.find(mix_path_key(entry.chart_id));
            const bool found = song != session_mix_song_paths_.end();
            append_stage(found ? &indexed_songs_[song->second] : nullptr, filename_only(entry.chart_id), session_mix_phase_label(entry.phase));
            view.can_start &= found;
        }
        view.summary = ui_text("Warm-up → challenge → cooldown · about 3 min per stage",
                               "워밍업 → 도전 → 마무리 · 스테이지당 약 3분 기준");
    }
    session_mix_stage_offset_ = std::clamp(session_mix_stage_offset_, 0,
        std::max(0, static_cast<int>(view.stages.size()) - 7));
    view.stage_offset = session_mix_stage_offset_;
    view.status = session_mix_status_message_.empty()
        ? (view.can_start ? ui_text("Select a course, then enter.", "코스를 선택한 뒤 입장하세요.")
                          : ui_text("All stages are required to enter.", "모든 스테이지의 채보가 있어야 입장할 수 있습니다."))
        : session_mix_status_message_;
    view.controls = {
        {ui_text("Auto length", "자동 믹스 길이"), "", settings_cursor_ == 0, std::to_string(session_mix_minutes_) + ui_text(" MIN", "분")},
        {ui_text("Remix", "자동 믹스 다시 만들기"), "", settings_cursor_ == 1, ""},
        {ui_text("Save draft", "초안 저장"), "", settings_cursor_ == 2, ".lr2crs"},
        {ui_text("Import courses", "코스 가져오기"), "", settings_cursor_ == 3, ".lr2crs"},
        {ui_text("Enter course", "코스 입장"), "", settings_cursor_ == 4, "Enter"},
        {ui_text("Back to Song Select", "곡 선택으로 돌아가기"), "", settings_cursor_ == 5, "Esc"},
    };
}

bool MenuApp::prepare_session_mix_start() {
    refresh_session_mix_auto_plan();
    refresh_session_mix_course_matches();
    SessionMixPlan plan;
    std::string title;
    if (session_mix_source_index_ == 0) plan = session_mix_auto_plan_;
    else if (session_mix_source_index_ == 1) {
        for (const auto& stage : session_mix_draft_) {
            if (session_mix_song_paths_.count(mix_path_key(stage.chart_id)) == 0) {
                session_mix_status_message_ = ui_text("All stages are required to enter.", "모든 스테이지의 채보가 있어야 입장할 수 있습니다.");
                return false;
            }
        }
        plan = build_session_mix_draft_plan(session_mix_draft_);
        title = "TenRiff Custom Course";
    } else if (const auto* course = selected_session_mix_lr2_course()) {
        const auto& match = session_mix_course_matches_[session_mix_source_index_ - 2];
        if (!match.playable()) {
            session_mix_status_message_ = ui_text("Missing charts: ", "누락 채보: ") + std::to_string(match.missing_md5.size());
            return false;
        }
        plan.target_minutes = static_cast<int>(course->chart_md5.size()) * 3;
        for (auto index : match.song_indices) {
            const auto& song = indexed_songs_[index];
            plan.entries.push_back({song_absolute_path(song), SessionMixPhase::Challenge, mix_difficulty(song)});
        }
        title = course->title;
    }
    if (plan.entries.empty()) {
        session_mix_status_message_ = ui_text("No playable charts match the current filter.", "현재 필터에 맞는 플레이 가능한 차트가 없습니다.");
        return false;
    }
    // A stale index must never start half a course. Check all paths before the
    // gauge/session state changes; an unavailable stage is never substituted.
    for (const auto& stage : plan.entries) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(menu_song_select::path_from_utf8(stage.chart_id), ec) || ec) {
            session_mix_status_message_ = ui_text("Chart file unavailable: ", "채보 파일 없음: ") + menu_song_select::filename_only(stage.chart_id);
            return false;
        }
    }
    session_mix_plan_ = std::move(plan);
    session_mix_active_course_title_ = std::move(title);
    return true;
}

bool MenuApp::handle_session_mix_click(const render::MenuClickEvent& event) {
    using Kind = render::MenuHitTargetKind;
    if (event.kind == Kind::SongCard) {
        if (event.index < 0 || event.index >= static_cast<int>(session_mix_lr2_courses_.size()) + 2) return true;
        select_session_mix_course(event.index);
        if (event.double_click) start_session_mix(); else publish_snapshot();
        return true;
    }
    if (event.kind == Kind::SongStartButton) { start_session_mix(); return true; }
    if (event.kind == Kind::SongBackButton) { handle_session_mix_input(key_escape_); return true; }
    if (event.kind == Kind::SessionMixStagePage) {
        session_mix_focus_ = 2;
        session_mix_stage_offset_ = std::max(0, session_mix_stage_offset_ + (event.index == 0 ? -7 : 7));
        publish_snapshot(); return true;
    }
    if (event.kind == Kind::MouseWheel) {
        const int steps = std::clamp(event.wheel_steps, -20, 20);
        for (int i = 0; i < std::abs(steps); ++i) handle_session_mix_input(steps > 0 ? key_up_ : key_down_);
        return true;
    }
    if (event.kind == Kind::SettingsRow) {
        if (event.index < 0 || event.index > 5) return true;
        session_mix_focus_ = 1;
        settings_cursor_ = event.index;
        if (event.part == render::MenuHitPart::SelectOnly) publish_snapshot();
        else handle_session_mix_input(event.part == render::MenuHitPart::Decrement ? key_left_ : key_enter_);
        return true;
    }
    return false;
}
} // namespace tenriff::app
