#include "app/MenuApp.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <utility>

#include "app/MenuRecordUtils.h"
#include "app/ChartFileHash.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuSongUtils.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

std::vector<std::string> ordered_collection_names(const config::RuntimeConfig& config) {
    std::vector<std::string> names;
    names.reserve(config.ui.collections.size());
    for (const auto& [name, items] : config.ui.collections) {
        static_cast<void>(items);
        if (!name.empty()) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace

void MenuApp::refresh_song_collection_membership_cache() {
    using namespace menu_song_select;

    favorite_song_keys_ = build_song_membership_set(config_.ui.favorite_chart_keys);
    song_collection_membership_ = build_song_collection_membership_lookup(config_.ui.collections);

    indexed_favorite_count_ = 0;
    if (indexed_songs_.empty() || favorite_song_keys_.empty()) {
        return;
    }

    for (const auto& entry : indexed_songs_) {
        const std::string absolute = song_absolute_path(entry);
        if (absolute.empty()) {
            continue;
        }
        const std::string key = menu_songs::normalize_path_key(path_from_utf8(absolute));
        if (song_membership_contains(favorite_song_keys_, key)) {
            ++indexed_favorite_count_;
        }
    }
}

std::string MenuApp::selected_song_path() const {
    if (selected_song_ < 0) {
        return {};
    }
    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    return entry ? entry->path : std::string{};
}

std::string MenuApp::format_song_line(std::size_t index) const {
    using namespace menu_song_select;

    const SongEntry* entry = visible_song_entry(index);
    if (!entry) {
        return "";
    }
    std::string label = std::to_string(index + 1) + ". ";
    label += song_title_for_ui(*entry);
    const std::string artist = song_artist_for_ui(*entry);
    if (!artist.empty()) {
        label += " - " + artist;
    }
    if (!entry->format.empty()) {
        label += " [" + safe_ui_text_or_placeholder(entry->format, "<invalid format>");
        if (entry->key_count > 0) {
            label += " " + std::to_string(entry->key_count) + "K";
        }
        label += "]";
    }
    if (entry->bpm > 0.0) {
        label += " BPM " + std::to_string(static_cast<int>(entry->bpm));
    }
    return label;
}

std::string MenuApp::selected_song_storage_key() const {
    using namespace menu_song_select;

    const std::string absolute = selected_song_absolute_path();
    if (absolute.empty()) {
        return {};
    }
    return menu_songs::normalize_path_key(path_from_utf8(absolute));
}

bool MenuApp::selected_song_is_favorite() const {
    using namespace menu_song_select;

    const std::string key = selected_song_storage_key();
    if (key.empty()) {
        return false;
    }
    return song_membership_contains(favorite_song_keys_, key);
}

bool MenuApp::selected_song_is_in_collection(std::string_view name) const {
    using namespace menu_song_select;

    const std::string key = selected_song_storage_key();
    if (key.empty() || name.empty()) {
        return false;
    }
    return song_collection_membership_contains(song_collection_membership_, name, key);
}

bool MenuApp::song_entry_matches_collection_filter(const SongEntry& entry) const {
    using namespace menu_song_select;

    const std::string filter = to_lower_ascii(config_.ui.song_collection_filter);
    if (filter.empty() || filter == "all") {
        return true;
    }

    const std::string key = menu_songs::normalize_path_key(path_from_utf8(song_absolute_path(entry)));
    if (key.empty()) {
        return (filter != "favorites");
    }

    if (filter == "favorites") {
        return song_membership_contains(favorite_song_keys_, key);
    }

    const auto it = song_collection_membership_.find(config_.ui.song_collection_filter);
    if (it == song_collection_membership_.end()) {
        return true;
    }
    return song_membership_contains(it->second, key);
}

MenuApp::BestResultRecord MenuApp::best_result_for_song_entry(const SongEntry& entry) const {
    using namespace menu_song_select;

    BestResultRecord best;
    try {
        for (const auto& key : menu_songs::build_chart_path_keys(entry.path, songs_path_)) {
            auto found = chart_best_results_.find(key);
            if (found == chart_best_results_.end()) {
                continue;
            }
            if (!best.has_value) {
                best = found->second;
                continue;
            }

            const int best_judged = best.perfect + best.great + best.good + best.bad;
            const int found_judged = found->second.perfect + found->second.great + found->second.good +
                                     found->second.bad;
            if (menu_records::is_better_record(found->second.best_score,
                                               menu_records::clear_status_priority(found->second.clear_status,
                                                                                   found->second.game_over,
                                                                                   found->second.final_gauge),
                                               found->second.detail_score,
                                               found->second.detailed_accuracy,
                                               found->second.max_combo,
                                               found_judged,
                                               found->second.created_utc,
                                               best.best_score,
                                               menu_records::clear_status_priority(best.clear_status,
                                                                                   best.game_over,
                                                                                   best.final_gauge),
                                               best.detail_score,
                                               best.detailed_accuracy,
                                               best.max_combo,
                                               best_judged,
                                               best.created_utc)) {
                best = found->second;
            }
        }
    } catch (...) {
    }
    return best;
}

std::string MenuApp::current_named_song_collection() const {
    const std::string filter = to_lower_ascii(config_.ui.song_collection_filter);
    if (filter.empty() || filter == "all" || filter == "favorites") {
        return {};
    }
    const auto it = config_.ui.collections.find(config_.ui.song_collection_filter);
    if (it == config_.ui.collections.end()) {
        return {};
    }
    return it->first;
}

std::string MenuApp::song_collection_filter_label() const {
    const std::string filter = to_lower_ascii(config_.ui.song_collection_filter);
    if (filter.empty() || filter == "all") {
        return ui_text("All Charts", "전체 차트");
    }
    if (filter == "favorites") {
        return ui_text("Favorites", "페이보릿");
    }
    const std::string named = current_named_song_collection();
    return named.empty() ? ui_text("All Charts", "전체 차트") : named;
}

void MenuApp::cycle_song_collection_filter(int direction) {
    std::vector<std::string> filters = {"all", "favorites"};
    const auto names = ordered_collection_names(config_);
    filters.insert(filters.end(), names.begin(), names.end());

    int index = 0;
    for (int i = 0; i < static_cast<int>(filters.size()); ++i) {
        if (filters[static_cast<std::size_t>(i)] == config_.ui.song_collection_filter) {
            index = i;
            break;
        }
    }
    index += direction;
    if (index < 0) {
        index = static_cast<int>(filters.size() - 1);
    } else if (index >= static_cast<int>(filters.size())) {
        index = 0;
    }
    config_.ui.song_collection_filter = filters[static_cast<std::size_t>(index)];
}

void MenuApp::create_next_song_collection() {
    int next_index = 1;
    for (;;) {
        const std::string name = "Collection " + std::to_string(next_index);
        if (config_.ui.collections.find(name) == config_.ui.collections.end()) {
            config_.ui.collections.emplace(name, std::vector<std::string>{});
            config_.ui.song_collection_filter = name;
            refresh_song_collection_membership_cache();
            return;
        }
        ++next_index;
    }
}

bool MenuApp::toggle_selected_song_favorite() {
    const std::string key = selected_song_storage_key();
    if (key.empty()) {
        return false;
    }

    auto& favorites = config_.ui.favorite_chart_keys;
    auto it = std::find(favorites.begin(), favorites.end(), key);
    if (it == favorites.end()) {
        favorites.push_back(key);
    } else {
        favorites.erase(it);
    }
    refresh_song_collection_membership_cache();
    return true;
}

bool MenuApp::toggle_selected_song_in_collection(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    const std::string key = selected_song_storage_key();
    if (key.empty()) {
        return false;
    }

    auto& items = config_.ui.collections[std::string(name)];
    auto it = std::find(items.begin(), items.end(), key);
    if (it == items.end()) {
        items.push_back(key);
    } else {
        items.erase(it);
    }
    refresh_song_collection_membership_cache();
    return true;
}

std::string MenuApp::session_mix_phase_label(SessionMixPhase phase) const {
    switch (phase) {
        case SessionMixPhase::Warmup: return ui_text("WARM-UP", "워밍업");
        case SessionMixPhase::Cooldown: return ui_text("COOLDOWN", "마무리");
        case SessionMixPhase::Challenge:
        default: return ui_text("CHALLENGE", "도전");
    }
}

bool MenuApp::load_session_mix_lr2_course_file(std::string_view path, bool remember_path) {
    namespace fs = std::filesystem;

    const Lr2CourseLoadResult result = load_lr2_course_file(path);
    for (const auto& warning : result.warnings) {
        std::cerr << "[warn] " << warning << std::endl;
    }
    if (!result.success()) {
        session_mix_status_message_ =
            ui_text("LR2 course load failed: ", "LR2 코스 로드 실패: ") + result.error;
        return false;
    }

    session_mix_lr2_courses_ = result.courses;
    session_mix_source_index_ = session_mix_lr2_courses_.empty() ? 0 : 2;

    fs::path stored_path = menu_song_select::path_from_utf8(path);
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(stored_path, ec);
    if (!ec && !canonical.empty()) {
        stored_path = canonical;
    }
    config_.ui.session_mix_lr2_course_path = stored_path.u8string();
    if (remember_path) {
        persist_runtime_config();
    }

    session_mix_status_message_ =
        ui_text("Loaded LR2 courses: ", "LR2 코스 로드: ") +
        std::to_string(session_mix_lr2_courses_.size());
    return true;
}

const Lr2CourseDefinition* MenuApp::selected_session_mix_lr2_course() const {
    if (session_mix_source_index_ <= 1) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(session_mix_source_index_ - 2);
    return index < session_mix_lr2_courses_.size() ? &session_mix_lr2_courses_[index] : nullptr;
}

bool MenuApp::add_selected_song_to_session_mix_draft() {
    if (song_select_view_ != SongSelectView::Songs || selected_song_ < 0) {
        return false;
    }
    const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(selected_song_));
    if (!entry) {
        return false;
    }
    const std::string chart_path = song_absolute_path(*entry);
    if (chart_path.empty()) {
        return false;
    }
    if (!session_mix_draft_.empty() && entry->key_count > 0 &&
        session_mix_draft_.front().key_count > 0 &&
        entry->key_count != session_mix_draft_.front().key_count) {
        session_mix_status_message_ = ui_text(
            "Course draft key mode mismatch. Clear the draft before changing key modes.",
            "코스 초안의 키 모드가 다릅니다. 다른 키 모드로 바꾸려면 초안을 먼저 비워주세요.");
        return false;
    }

    std::string md5 = entry->md5;
    if (md5.size() != 32u) {
        std::string hash_error;
        md5 = hash_chart_file_utf8(chart_path, &hash_error).md5;
        if (md5.size() != 32u) {
            session_mix_status_message_ = ui_text(
                "Could not hash the selected chart for LR2 course export: ",
                "LR2 코스 저장용 차트 해시를 만들지 못했습니다: ") + hash_error;
            return false;
        }
    }

    int difficulty = entry->level > 0 ? entry->level : entry->native_level;
    if (difficulty <= 0 && entry->rating > 0.0) {
        difficulty = static_cast<int>(std::llround(entry->rating));
    }
    session_mix_draft_.push_back(SessionMixDraftEntry{
        chart_path, md5, entry->title, entry->key_count, difficulty});
    session_mix_status_message_ =
        ui_text("Added to course draft: ", "코스 초안에 추가: ") +
        std::to_string(session_mix_draft_.size()) + "  " + entry->title;
    return true;
}

void MenuApp::remove_last_session_mix_draft_song() {
    if (session_mix_draft_.empty()) {
        session_mix_status_message_ = ui_text("Course draft is already empty.",
                                              "코스 초안이 이미 비어 있습니다.");
        return;
    }
    const std::string title = session_mix_draft_.back().title;
    session_mix_draft_.pop_back();
    session_mix_status_message_ =
        ui_text("Removed last draft stage: ", "초안의 마지막 스테이지 제거: ") + title;
    if (session_mix_draft_.empty() && session_mix_source_index_ == 1) {
        session_mix_source_index_ = 0;
    }
}

bool MenuApp::save_session_mix_draft(std::string_view path) {
    if (session_mix_draft_.empty()) {
        session_mix_status_message_ = ui_text("Course draft is empty.", "코스 초안이 비어 있습니다.");
        return false;
    }
    Lr2CourseDefinition course;
    const std::filesystem::path file_path = menu_song_select::path_from_utf8(path);
    course.title = file_path.stem().u8string();
    if (course.title.empty()) {
        course.title = "TenRiff Custom Course";
    }
    course.key_count = session_mix_draft_.front().key_count;
    course.type = 0;
    course.chart_md5.reserve(session_mix_draft_.size());
    for (const auto& entry : session_mix_draft_) {
        course.chart_md5.push_back(entry.chart_md5);
    }
    const Lr2CourseSaveResult saved = save_lr2_course_file(path, course);
    if (!saved.success()) {
        session_mix_status_message_ =
            ui_text("LR2 course save failed: ", "LR2 코스 저장 실패: ") + saved.error;
        return false;
    }
    session_mix_status_message_ =
        ui_text("Saved LR2 course: ", "LR2 코스 저장 완료: ") + course.title;
    return true;
}

void MenuApp::start_session_mix() {
    if (session_mix_source_index_ == 1) {
        session_mix_plan_ = build_session_mix_draft_plan(session_mix_draft_);
        session_mix_active_course_title_ = "TenRiff Custom Course";
    } else if (const Lr2CourseDefinition* course = selected_session_mix_lr2_course()) {
        const Lr2CourseMatch match = match_lr2_course(*course, indexed_songs_);
        if (!match.missing_md5.empty()) {
            session_mix_status_message_ =
                ui_text("LR2 course unavailable: ", "LR2 코스 실행 불가: ") +
                std::to_string(match.missing_md5.size()) + "/" +
                std::to_string(course->chart_md5.size()) +
                ui_text(" charts are missing from the active song source.",
                        "개 채보가 현재 곡 소스에 없습니다.");
            publish_snapshot();
            return;
        }
        if (match.song_indices.empty()) {
            session_mix_status_message_ = ui_text(
                "The selected LR2 course has no playable charts.",
                "선택한 LR2 코스에 플레이 가능한 채보가 없습니다.");
            publish_snapshot();
            return;
        }

        session_mix_plan_ = {};
        session_mix_plan_.target_minutes = static_cast<int>(match.song_indices.size()) * 3;
        session_mix_plan_.entries.reserve(match.song_indices.size());
        for (const std::size_t song_index : match.song_indices) {
            const SongEntry& entry = indexed_songs_[song_index];
            int difficulty = entry.level > 0 ? entry.level : entry.native_level;
            if (difficulty <= 0 && entry.rating > 0.0) {
                difficulty = static_cast<int>(std::llround(entry.rating));
            }
            session_mix_plan_.entries.push_back(SessionMixEntry{
                song_absolute_path(entry), SessionMixPhase::Challenge, difficulty});
        }
        session_mix_active_course_title_ = course->title;
    } else {
        session_mix_active_course_title_.clear();
        std::vector<SessionMixCandidate> candidates;
        candidates.reserve(visible_song_count());
        for (std::size_t i = 0; i < visible_song_count(); ++i) {
            const SongEntry* entry = visible_song_entry(i);
            if (!entry) {
                continue;
            }
            const std::string chart_path = song_absolute_path(*entry);
            if (chart_path.empty()) {
                continue;
            }
            const BestResultRecord best = best_result_for_song_entry(*entry);
            int difficulty = entry->level > 0 ? entry->level : entry->native_level;
            if (difficulty <= 0 && entry->rating > 0.0) {
                difficulty = static_cast<int>(std::llround(entry->rating));
            }
            candidates.push_back(SessionMixCandidate{
                chart_path,
                difficulty,
                best.has_value,
                best.has_value && !best.game_over,
                best.accuracy,
            });
        }

        const std::uint32_t seed = static_cast<std::uint32_t>(
            timing::HighResClock::now_ns() ^ static_cast<int64_t>(candidates.size()));
        session_mix_plan_ = build_session_mix_plan(
            std::move(candidates), session_mix_minutes_, seed);
    }
    if (session_mix_plan_.entries.empty()) {
        session_mix_status_message_ = ui_text(
            "No playable charts match the current filter.",
            "현재 필터에 맞는 플레이 가능한 차트가 없습니다.");
        publish_snapshot();
        return;
    }

    session_mix_cursor_ = 0;
    session_mix_completed_ = 0;
    session_mix_cleared_ = 0;
    session_mix_total_score_ = 0;
    session_mix_gauge_value_ = 100.0;
    session_mix_active_ = true;
    session_mix_current_result_recorded_ = false;
    session_mix_status_message_.clear();
    launch_current_session_mix_song();
}

void MenuApp::launch_current_session_mix_song() {
    using namespace menu_song_select;

    if (!session_mix_active_ || session_mix_cursor_ >= session_mix_plan_.entries.size()) {
        stop_session_mix(true);
        screen_ = Screen::SongSelect;
        publish_snapshot();
        return;
    }

    const std::string& chart_path = session_mix_plan_.entries[session_mix_cursor_].chart_id;
    const std::string target_key = menu_songs::normalize_path_key(path_from_utf8(chart_path));
    for (std::size_t i = 0; i < visible_song_count(); ++i) {
        const SongEntry* entry = visible_song_entry(i);
        if (!entry) {
            continue;
        }
        const std::string candidate_key =
            menu_songs::normalize_path_key(path_from_utf8(song_absolute_path(*entry)));
        if (candidate_key == target_key) {
            selected_song_ = static_cast<int>(i);
            break;
        }
    }

    session_mix_current_result_recorded_ = false;
    launch_gameplay(chart_path);
    if (session_mix_active_ && screen_ == Screen::SongSelect) {
        stop_session_mix(false);
        publish_snapshot();
    }
}

void MenuApp::record_current_session_mix_result() {
    if (!session_mix_active_ || session_mix_current_result_recorded_ || !has_result_) {
        return;
    }
    session_mix_current_result_recorded_ = true;
    ++session_mix_completed_;
    if (!last_game_over_) {
        ++session_mix_cleared_;
    }
    session_mix_total_score_ += last_result_final_score_;
}

void MenuApp::advance_session_mix_from_result() {
    if (!session_mix_active_) {
        return;
    }
    record_current_session_mix_result();
    if (last_game_over_) {
        const std::size_t failed_stage = session_mix_cursor_ + 1;
        const std::size_t total_stages = session_mix_plan_.entries.size();
        stop_session_mix(false);
        session_mix_status_message_ =
            ui_text("COURSE FAILED", "코스 실패") + "  " +
            std::to_string(failed_stage) + "/" + std::to_string(total_stages);
        screen_ = Screen::SongSelect;
        publish_snapshot();
        return;
    }
    if (session_mix_cursor_ + 1 >= session_mix_plan_.entries.size()) {
        stop_session_mix(true);
        screen_ = Screen::SongSelect;
        publish_snapshot();
        return;
    }
    ++session_mix_cursor_;
    launch_current_session_mix_song();
}

void MenuApp::stop_session_mix(bool completed) {
    const int planned = static_cast<int>(session_mix_plan_.entries.size());
    const int average_score = session_mix_completed_ > 0
                                  ? static_cast<int>(session_mix_total_score_ / session_mix_completed_)
                                  : 0;
    const std::string progress = std::to_string(session_mix_completed_) + "/" +
                                 std::to_string(planned);
    if (completed) {
        session_mix_status_message_ =
            ui_text("SESSION MIX COMPLETE", "세션 믹스 완료") + "  " + progress + "  " +
            ui_text("CLEARS ", "클리어 ") + std::to_string(session_mix_cleared_) + "  " +
            ui_text("AVG ", "평균 ") + std::to_string(average_score);
    } else {
        session_mix_status_message_ =
            ui_text("SESSION MIX STOPPED", "세션 믹스 중단") + "  " + progress + "  " +
            ui_text("CLEARS ", "클리어 ") + std::to_string(session_mix_cleared_);
    }
    session_mix_active_ = false;
    session_mix_current_result_recorded_ = false;
    session_mix_active_course_title_.clear();
}

void MenuApp::update_song_list(SongIndex index) {
    using namespace menu_song_select;

    std::string selected_path = selected_song_path();
    indexed_songs_ = std::move(index.entries);
    ++song_index_revision_;
    // Sanitize once when the source list changes so later view rebuilds can stay cheap.
    for (auto& entry : indexed_songs_) {
        entry.title = safe_ui_text(entry.title);
        entry.artist = safe_ui_text(entry.artist);
        entry.format = safe_ui_text(entry.format);
    }
    refresh_song_collection_membership_cache();
    rebuild_visible_song_list(selected_path.empty() ? nullptr : &selected_path);
    sync_song_select_state();
}

} // namespace tenriff::app
