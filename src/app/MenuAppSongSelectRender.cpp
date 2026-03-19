#include "app/MenuApp.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuRecordUtils.h"
#include "app/MenuSongUtils.h"
#include "timing/HighResClock.h"

namespace tenriff::app {

namespace {

constexpr int kSongSelectVisibleCardCount = 5;

std::string song_sort_detail_label(MenuApp::SongSortMode mode) {
    switch (mode) {
        case MenuApp::SongSortMode::DifficultyDesc: return "LV DESC";
        case MenuApp::SongSortMode::TitleAsc: return "A-Z";
        case MenuApp::SongSortMode::TitleDesc: return "Z-A";
        case MenuApp::SongSortMode::DifficultyAsc:
        default: return "LV ASC";
    }
}

}  // namespace

void MenuApp::populate_song_select_render_data(render::MenuRenderData& render,
                                               const std::string& current_track,
                                               const MenuApp::BestResultRecord& current_best,
                                               const MenuApp::LocalPlayRecord* selected_record) {
    using namespace menu_song_select;

    // Song Select, Sources, and Records intentionally share one render payload so the
    // renderer can switch views without a second screen-specific data model.
    render.kind = render::MenuScreenKind::SongSelect;
    render.song_select.profile = options_.profile;
    render.song_select.track = current_track;
    render.song_select.song_count = static_cast<int>(visible_song_count());
    render.song_select.source_count = static_cast<int>(config_.ui.recent_song_sources.size());
    render.song_select.record_count = static_cast<int>(current_song_record_indices_.size());
    render.song_select.showing_sources = (song_select_view_ == SongSelectView::Sources);
    render.song_select.showing_records = (song_select_view_ == SongSelectView::Records);
    render.song_select.high_score =
        render.song_select.showing_sources ? 0 :
        (render.song_select.showing_records && selected_record ? selected_record->score :
         (current_best.has_value ? current_best.best_score : 0));
    render.song_select.current_source_name =
        safe_ui_text(menu_songs::song_source_display_name(songs_path_), "Songs");
    render.song_select.current_source_path = safe_ui_text_or_placeholder(songs_path_, "<invalid path>");
    render.song_select.index_profile_label = song_index_profile_label(config_.mode.song_index_profile);
    render.song_select.browser_summary = browser_summary_label(song_search_query_,
                                                               song_key_filter_,
                                                               song_level_min_filter_,
                                                               song_level_max_filter_);
    render.song_select.sort_summary = song_sort_detail_label(song_sort_mode_);
    render.song_select.primary_hint =
        render.song_select.showing_sources
            ? "UP/DOWN or wheel  MOVE     ENTER / dbl-click  OPEN SOURCE     PGUP/PGDN  PAGE"
            : (render.song_select.showing_records
                   ? "UP/DOWN or wheel  MOVE     ENTER / dbl-click  OPEN RESULT     PGUP/PGDN  PAGE"
                   : "UP/DOWN or wheel  MOVE     ENTER / dbl-click  PLAY     PGUP/PGDN  PAGE");
    render.song_select.secondary_hint =
        render.song_select.showing_sources
            ? "LEFT/RIGHT  NAV FOCUS     BACKSPACE  SONGS     F2  BROWSE     F5  REINDEX     F1  HELP"
            : (render.song_select.showing_records
                   ? "LEFT/RIGHT  NAV FOCUS     BACKSPACE  SONGS     ESC  TITLE     F1  HELP"
                   : "LEFT/RIGHT  NAV FOCUS     BACKSPACE  SOURCES     A/G/I/M/K  SETTINGS     F5  REINDEX     F1  HELP");

    const std::string source_detail =
        std::to_string(render.song_select.source_count) + " ROOT" +
        (render.song_select.source_count == 1 ? "" : "S");
    const std::string browser_detail = render.song_select.browser_summary;
    const std::string records_detail =
        (render.song_select.record_count > 0)
            ? (std::to_string(render.song_select.record_count) + " PLAYS")
            : std::string("NO PLAYS");

    render.song_select.indexing = song_indexer_.is_running();
    if (render.song_select.indexing) {
        const auto progress = song_indexer_.progress();
        render.song_select.indexing_stage = song_index_stage_label(progress.stage);
        render.song_select.indexing_processed = std::max(0, progress.processed);
        render.song_select.indexing_total = progress.total;
        if (progress.total > 0) {
            const int processed = std::max(0, std::min(progress.processed, progress.total));
            const int percent = static_cast<int>(std::llround(
                100.0 * static_cast<double>(processed) / static_cast<double>(progress.total)));
            render.song_select.indexing_percent = percent;

            const int remaining = progress.total - processed;
            const int64_t now_ns = timing::HighResClock::now_ns();
            const int64_t elapsed_ns = (progress.started_ns > 0 && now_ns > progress.started_ns)
                                           ? (now_ns - progress.started_ns)
                                           : 0;
            if (remaining > 0 && processed > 0 && elapsed_ns > 0) {
                const double elapsed_sec = static_cast<double>(elapsed_ns) / 1'000'000'000.0;
                const double eta_sec =
                    elapsed_sec * static_cast<double>(remaining) / static_cast<double>(processed);
                render.song_select.indexing_eta =
                    format_eta_seconds(static_cast<int64_t>(std::llround(eta_sec)));
            }
        } else if (progress.total == 0) {
            render.song_select.indexing_percent = 0;
            render.song_select.indexing_eta = "0s";
        } else {
            render.song_select.indexing_percent = -1;
            render.song_select.indexing_eta.clear();
        }
    }

    render.song_select.left_nav = {
        render::MenuButtonData{"LEVEL", "L", song_select_nav_cursor_ == 0,
                               (song_sort_mode_ == SongSortMode::DifficultyAsc ||
                                song_sort_mode_ == SongSortMode::DifficultyDesc)
                                   ? song_sort_detail_label(song_sort_mode_)
                                   : "LV ASC"},
        render::MenuButtonData{"TITLE", "T", song_select_nav_cursor_ == 1,
                               (song_sort_mode_ == SongSortMode::TitleAsc ||
                                song_sort_mode_ == SongSortMode::TitleDesc)
                                   ? song_sort_detail_label(song_sort_mode_)
                                   : "A-Z"},
        render::MenuButtonData{"SOURCES", "D", song_select_nav_cursor_ == 2,
                               render.song_select.showing_sources ? "ACTIVE" : source_detail},
        render::MenuButtonData{"KEY", "K", song_select_nav_cursor_ == 3, key_filter_label(song_key_filter_)},
        render::MenuButtonData{"BROWSE", "F", song_select_nav_cursor_ == 4, browser_detail},
        render::MenuButtonData{"MOD", "M", song_select_nav_cursor_ == 5,
                               format_multiplier(config_.speed.rate) + " / HS " +
                                   format_decimal(config_.speed.hi_speed)},
        render::MenuButtonData{"OPTIONS", "O", song_select_nav_cursor_ == 6, "AUDIO / GFX"},
        render::MenuButtonData{"RECORDS", "R", song_select_nav_cursor_ == 7,
                               render.song_select.showing_records ? "ACTIVE" : records_detail},
    };

    if (render.song_select.showing_sources) {
        const int total_sources = static_cast<int>(config_.ui.recent_song_sources.size());
        if (total_sources > 0) {
            selected_source_ = clamp_int(selected_source_, 0, total_sources - 1);
            constexpr int visible = kSongSelectVisibleCardCount;
            int start = std::max(0, selected_source_ - (visible / 2));
            const int max_start = std::max(0, total_sources - visible);
            start = std::min(start, max_start);
            const int end = std::min(total_sources, start + visible);
            render.song_select.list_total_count = total_sources;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_source_;

            for (int i = start; i < end; ++i) {
                const std::string& source_path = config_.ui.recent_song_sources[static_cast<std::size_t>(i)];
                render::SongCardData card;
                card.title = safe_ui_text(menu_songs::song_source_display_name(source_path), "<invalid title>");
                card.artist = safe_ui_text_or_placeholder(source_path, "<invalid artist>");
                const auto count_it =
                    source_song_counts_.find(menu_songs::normalize_path_key(path_from_utf8(source_path)));
                card.level = (count_it != source_song_counts_.end()) ? count_it->second : 0;
                card.song_index = i;
                card.selected = (i == selected_source_);
                card.detail = safe_ui_text((menu_songs::normalize_path_key(path_from_utf8(source_path)) ==
                                            menu_songs::normalize_path_key(path_from_utf8(songs_path_)))
                                               ? "CURRENT SOURCE"
                                               : "RECENT SOURCE");
                render.song_select.songs.push_back(std::move(card));
            }

            const std::string& selected_source =
                config_.ui.recent_song_sources[static_cast<std::size_t>(selected_source_)];
            render.song_select.selected_source_name =
                safe_ui_text(menu_songs::song_source_display_name(selected_source), "Songs");
            render.song_select.selected_source_path =
                safe_ui_text_or_placeholder(selected_source, "<invalid path>");
            const auto count_it =
                source_song_counts_.find(menu_songs::normalize_path_key(path_from_utf8(selected_source)));
            render.song_select.selected_source_song_count =
                (count_it != source_song_counts_.end()) ? count_it->second : -1;
            render.song_select.selected_source_active =
                menu_songs::normalize_path_key(path_from_utf8(selected_source)) ==
                menu_songs::normalize_path_key(path_from_utf8(songs_path_));
        }
        if (total_sources == 0) {
            render.song_select.empty_title = "NO SONG SOURCES";
            render.song_select.empty_message =
                "Press F2 to choose a folder, or drag and drop one onto the window.";
        }
    } else if (render.song_select.showing_records) {
        const int total = static_cast<int>(current_song_record_indices_.size());
        if (total > 0) {
            constexpr int visible = kSongSelectVisibleCardCount;
            selected_record_ = clamp_int(selected_record_, 0, total - 1);
            int start = std::max(0, selected_record_ - (visible / 2));
            const int max_start = std::max(0, total - visible);
            start = std::min(start, max_start);
            const int end = std::min(total, start + visible);
            render.song_select.list_total_count = total;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_record_;

            for (int i = start; i < end; ++i) {
                const LocalPlayRecord& record =
                    local_play_records_[current_song_record_indices_[static_cast<std::size_t>(i)]];
                render::SongCardData card;
                card.title = menu_records::compact_timestamp_label(record.created_utc);
                card.artist = record.clear_status + "  " + record.rank + "  SCORE " +
                              format_int_with_commas(record.score);
                card.detail = record.replay_path.empty()
                                  ? "RESULT ONLY"
                                  : "REPLAY " + filename_only(record.replay_path);
                card.song_index = i;
                card.selected = (i == selected_record_);
                render.song_select.songs.push_back(std::move(card));
            }
        }
        if (total == 0) {
            render.song_select.empty_title = "NO LOCAL RECORDS";
            render.song_select.empty_message =
                "Play a chart first. Saved results and replays will appear here.";
        }
    } else {
        const int total = static_cast<int>(visible_song_count());
        if (total > 0) {
            constexpr int visible = kSongSelectVisibleCardCount;
            int start = std::max(0, selected_song_ - (visible / 2));
            const int max_start = std::max(0, total - visible);
            start = std::min(start, max_start);
            const int end = std::min(total, start + visible);
            render.song_select.list_total_count = total;
            render.song_select.list_visible_count = end - start;
            render.song_select.list_window_start = start;
            render.song_select.list_selected_index = selected_song_;

            for (int i = start; i < end; ++i) {
                const SongEntry* entry = visible_song_entry(static_cast<std::size_t>(i));
                if (!entry) {
                    continue;
                }
                render::SongCardData card;
                card.title = song_title_for_ui(*entry);
                card.artist = song_artist_for_ui(*entry);
                card.detail = safe_ui_text_or_placeholder(song_detail_label(*entry), "<invalid detail>");
                card.background_path = song_background_preview_path_for_entry(*entry);
                card.level = entry->level;
                card.rating = entry->rating;
                card.song_index = i;
                card.selected = (i == selected_song_);
                render.song_select.songs.push_back(std::move(card));
            }
        }
        if (total == 0) {
            if (!song_search_query_.empty() || song_key_filter_ > 0 || song_level_min_filter_ > 0 ||
                song_level_max_filter_ > 0) {
                render.song_select.empty_title = "NO CHARTS MATCH";
                render.song_select.empty_message =
                    "Clear filters in Browse or switch the active source to see more charts.";
            } else if (render.song_select.indexing) {
                render.song_select.empty_title = "BUILDING LIBRARY";
                render.song_select.empty_message =
                    "The current source is still indexing. You can keep browsing while scan progress updates above.";
            } else {
                render.song_select.empty_title = "NO CHARTS INDEXED";
                render.song_select.empty_message =
                    "Press F5 to scan the current source, or use F2 / drag-and-drop to point TenRiff at a songs folder.";
            }
        }
    }

    if (!render.song_select.showing_sources) {
        if (const SongEntry* entry = (selected_song_ >= 0)
                                         ? visible_song_entry(static_cast<std::size_t>(selected_song_))
                                         : nullptr) {
            render.song_select.selected_song_title = song_title_for_ui(*entry);
            render.song_select.selected_song_artist = song_artist_for_ui(*entry);
            render.song_select.selected_song_detail = safe_ui_text_or_placeholder(song_detail_label(*entry), "-");
            render.song_select.selected_song_background_path = selected_song_background_preview_path();
        }
    }

    if (render.song_select.showing_records) {
        if (selected_record) {
            render.song_select.rank = selected_record->rank;
            render.song_select.best_score = selected_record->score;
            render.song_select.max_combo = selected_record->max_combo;
            render.song_select.perfect = selected_record->perfect;
            render.song_select.great = selected_record->great;
            render.song_select.good = selected_record->good;
            render.song_select.bad = selected_record->bad;
            render.song_select.miss = selected_record->miss;
            render.song_select.accuracy = selected_record->accuracy;
            render.song_select.selected_record_created_utc =
                menu_records::compact_timestamp_label(selected_record->created_utc);
            render.song_select.selected_record_status = selected_record->clear_status;
            render.song_select.selected_record_replay_file = filename_only(selected_record->replay_path);
            if (const ReplaySummary* replay = replay_summary_for_path(selected_record->replay_path)) {
                render.song_select.selected_record_replay_lane_count = replay->lane_count;
                render.song_select.selected_record_replay_event_count = replay->event_count;
                if (!replay->exists) {
                    render.song_select.selected_record_replay_detail = "Replay missing";
                } else if (!replay->error.empty()) {
                    render.song_select.selected_record_replay_detail = "Replay parse warning";
                } else {
                    render.song_select.selected_record_replay_detail =
                        format_multiplier(replay->rate) + " / " +
                        format_signed_offset_ms(replay->input_offset_ms);
                }
            } else {
                render.song_select.selected_record_replay_detail = "No replay";
            }
        }
    } else if (!render.song_select.showing_sources) {
        render.song_select.rank = current_best.has_value ? current_best.rank : "--";
        render.song_select.best_score = current_best.has_value ? current_best.best_score : 0;
        render.song_select.max_combo = current_best.has_value ? current_best.max_combo : 0;
        render.song_select.perfect = current_best.has_value ? current_best.perfect : 0;
        render.song_select.great = current_best.has_value ? current_best.great : 0;
        render.song_select.good = current_best.has_value ? current_best.good : 0;
        render.song_select.bad = current_best.has_value ? current_best.bad : 0;
        render.song_select.miss = current_best.has_value ? current_best.miss : 0;
    }
}

void MenuApp::populate_song_browser_render_data(render::MenuRenderData& render) {
    using namespace menu_song_select;

    render.kind = render::MenuScreenKind::GenericList;

    append_menu_row(render.generic,
                    "Search",
                    song_search_query_.empty() ? std::string("<type to search>") : song_search_query_,
                    settings_cursor_ == 0,
                    render::MenuHitTargetKind::SettingsRow,
                    0,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Key Filter",
                    key_filter_label(song_key_filter_),
                    settings_cursor_ == 1,
                    render::MenuHitTargetKind::SettingsRow,
                    1,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Difficulty Min",
                    song_level_min_filter_ > 0 ? std::to_string(song_level_min_filter_) : "Any",
                    settings_cursor_ == 2,
                    render::MenuHitTargetKind::SettingsRow,
                    2,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Difficulty Max",
                    song_level_max_filter_ > 0 ? std::to_string(song_level_max_filter_) : "Any",
                    settings_cursor_ == 3,
                    render::MenuHitTargetKind::SettingsRow,
                    3,
                    false,
                    true);
    append_menu_row(render.generic,
                    "Clear Filters",
                    "",
                    settings_cursor_ == 4,
                    render::MenuHitTargetKind::SettingsRow,
                    4,
                    true,
                    false);
    append_menu_row(render.generic,
                    "Back",
                    "",
                    settings_cursor_ == 5,
                    render::MenuHitTargetKind::SettingsRow,
                    5,
                    true,
                    false);

    render.generic.notes.push_back("Search matches title, artist, and chart path.");
    render.generic.notes.push_back("Use letters/numbers/space on the Search row. Backspace deletes, Delete clears.");
    render.generic.notes.push_back("Key Filter supports All plus 4K through 10K and 16K. Song Select also has a quick KEY toggle.");
    render.generic.notes.push_back("Difficulty filters apply to indexed LV values only.");
    render.generic.notes.push_back("Sort order stays available from Song Select: LEVEL toggles ASC/DESC, TITLE toggles A-Z/Z-A.");
}

}  // namespace tenriff::app
