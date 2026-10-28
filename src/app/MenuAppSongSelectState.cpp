#include "app/MenuApp.h"

#include <algorithm>
#include <vector>

#include "app/MemoryDiagnostics.h"
#include "app/DifficultyTable.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuSongUtils.h"
#include "app/SongSelectState.h"

namespace tenriff::app {

namespace {

std::string song_group_sort_key(const SongEntry& entry, MenuApp::SongGroupMode mode) {
    using namespace menu_song_select;

    switch (mode) {
        case MenuApp::SongGroupMode::Artist: return song_group_artist_key(entry);
        case MenuApp::SongGroupMode::Level: return song_group_level_key(entry);
        case MenuApp::SongGroupMode::Folder: return song_group_folder_key(entry);
        case MenuApp::SongGroupMode::None:
        default: return {};
    }
}

}  // namespace

bool MenuApp::showing_table_level_folders() const {
    return song_select_view_ == SongSelectView::Songs && !config_.ui.difficulty_table_path.empty() &&
        active_table_level_.empty() && song_search_query_.empty() && !song_select_search_active_;
}

void MenuApp::refresh_table_level_folders() {
    if (table_levels_path_ == config_.ui.difficulty_table_path) return;
    table_levels_path_ = config_.ui.difficulty_table_path;
    table_levels_.clear();
    active_table_level_.clear();
    selected_table_level_ = 0;
    if (table_levels_path_.empty()) return;
    const auto loaded = load_difficulty_table_utf8(table_levels_path_);
    if (loaded.success()) table_levels_ = loaded.table.levels();
    std::stable_sort(table_levels_.begin(), table_levels_.end(), menu_song_select::table_level_less);
    table_levels_.erase(std::unique(table_levels_.begin(), table_levels_.end()), table_levels_.end());
}

void MenuApp::open_selected_table_level() {
    if (!showing_table_level_folders() || selected_table_level_ < 0 ||
        static_cast<std::size_t>(selected_table_level_) >= table_levels_.size()) return;
    active_table_level_ = table_levels_[static_cast<std::size_t>(selected_table_level_)];
    selected_song_ = 0;
    rebuild_visible_song_list();
    publish_snapshot();
}

void MenuApp::apply_song_sort(SongSortMode mode) {
    song_sort_mode_ = mode;
    sort_song_list_preserving_selection();
}

void MenuApp::sort_song_list_preserving_selection() {
    rebuild_visible_song_list();
}

void MenuApp::sync_song_select_state() {
    const bool preserve_records = (song_select_view_ == SongSelectView::Records);
    app::SongSelectState state;
    state.selected_song = selected_song_;
    state.selected_source = selected_source_;
    state.showing_sources = (song_select_view_ == SongSelectView::Sources);
    app::sync_song_select_state(state, visible_song_count(), config_.ui.recent_song_sources.size() + 1);
    selected_song_ = state.selected_song;
    selected_source_ = state.selected_source;
    if (state.showing_sources) {
        song_select_view_ = SongSelectView::Sources;
    } else if (preserve_records) {
        song_select_view_ = SongSelectView::Records;
    } else {
        song_select_view_ = SongSelectView::Songs;
    }
}

void MenuApp::rebuild_visible_song_list(const std::string* selected_path) {
    session_mix_auto_dirty_ = true;
    using namespace menu_song_select;

    std::string preserved_path;
    if (selected_path) {
        preserved_path = *selected_path;
    } else {
        preserved_path = selected_song_path();
    }

    std::shared_ptr<const MultiplayerChartHashSet> remote_library;
    if (multiplayer_selecting_chart_) {
        const network::PeerSessionSnapshot peer = peer_session_.snapshot();
        if (peer.remote_library_ready) remote_library = peer.remote_library_sha256;
    }

    refresh_table_level_folders();
    table_level_counts_.assign(table_levels_.size(), 0);
    visible_song_indices_.clear();
    visible_song_indices_.reserve(indexed_songs_.size());
    for (std::size_t index = 0; index < indexed_songs_.size(); ++index) {
        const SongEntry& entry = indexed_songs_[index];
        if (multiplayer_selecting_chart_ &&
            (!remote_library || !multiplayer_chart_is_shared(entry, *remote_library))) {
            continue;
        }
        if (song_entry_matches_table_scope(entry, config_.ui.all_song_sources,
                                           !config_.ui.difficulty_table_path.empty()) &&
            song_entry_matches_search(entry, song_search_query_, !config_.ui.difficulty_table_path.empty()) &&
            song_entry_matches_key_filter(entry, song_key_filter_) &&
            song_entry_matches_level_filter(entry, song_level_min_filter_, song_level_max_filter_,
                                            !config_.ui.difficulty_table_path.empty()) &&
            song_entry_matches_collection_filter(entry)) {
            const auto tier = std::find(table_levels_.begin(), table_levels_.end(), entry.difficulty_table_level);
            if (tier != table_levels_.end()) ++table_level_counts_[static_cast<std::size_t>(tier - table_levels_.begin())];
            if (active_table_level_.empty() || entry.difficulty_table_level == active_table_level_)
                visible_song_indices_.push_back(index);
        }
    }

    const auto song_sort_compare = [this](const SongEntry& lhs, const SongEntry& rhs) {
        switch (song_sort_mode_) {
            case SongSortMode::DifficultyAsc: return song_entry_less_by_difficulty_asc(lhs, rhs);
            case SongSortMode::DifficultyDesc: return song_entry_less_by_difficulty_desc(lhs, rhs);
            case SongSortMode::TitleAsc: return song_entry_less_by_title_asc(lhs, rhs);
            case SongSortMode::ArtistAsc: return song_entry_less_by_artist_asc(lhs, rhs);
            case SongSortMode::ArtistDesc: return song_entry_less_by_artist_desc(lhs, rhs);
            case SongSortMode::TitleDesc:
            default: return song_entry_less_by_title_desc(lhs, rhs);
        }
    };

    std::stable_sort(visible_song_indices_.begin(), visible_song_indices_.end(), [this, &song_sort_compare](std::size_t lhs,
                                                                                                              std::size_t rhs) {
        const SongEntry& lhs_entry = indexed_songs_[lhs];
        const SongEntry& rhs_entry = indexed_songs_[rhs];
        if (song_group_mode_ != SongGroupMode::None) {
            const std::string lhs_group = song_group_sort_key(lhs_entry, song_group_mode_);
            const std::string rhs_group = song_group_sort_key(rhs_entry, song_group_mode_);
            if (lhs_group != rhs_group) {
                // Reversing the encoded group key also put unknown LV and
                // non-table charts first. Use the same known-before-unknown
                // level ordering as the ungrouped view in both directions.
                if (song_group_mode_ == SongGroupMode::Level) {
                    return song_sort_mode_ == SongSortMode::DifficultyDesc
                        ? song_entry_less_by_difficulty_desc(lhs_entry, rhs_entry)
                        : song_entry_less_by_difficulty_asc(lhs_entry, rhs_entry);
                }
                const bool reverse_group_order =
                    song_group_mode_ == SongGroupMode::Artist && song_sort_mode_ == SongSortMode::ArtistDesc;
                return reverse_group_order ? (lhs_group > rhs_group) : (lhs_group < rhs_group);
            }
        }
        return song_sort_compare(lhs_entry, rhs_entry);
    });

    if (visible_song_indices_.empty()) {
        selected_song_ = 0;
        sync_song_select_state();
        if (!multiplayer_selecting_chart_ && !config_.ui.all_song_sources && !songs_path_.empty()) {
            source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] = 0;
        }
        return;
    }

    // Resolve selection against the filtered/sorted view, but preserve the original chart
    // path when possible so cursor focus survives reindex/filter/sort churn.
    selected_song_ = visible_song_indices_.empty() ? 0 :
        std::clamp(selected_song_, 0, static_cast<int>(visible_song_indices_.size() - 1));
    if (!preserved_path.empty()) {
        for (std::size_t i = 0; i < visible_song_indices_.size(); ++i) {
            if (indexed_songs_[visible_song_indices_[i]].path == preserved_path) {
                selected_song_ = static_cast<int>(i);
                break;
            }
        }
    }
    sync_song_select_state();
    rebuild_current_song_record_indices();

    if (!multiplayer_selecting_chart_ && !config_.ui.all_song_sources) {
        source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] =
            static_cast<int>(visible_song_count());
    }
    log_memory_phase("MenuApp",
                     "visible-list-rebuilt",
                     query_process_memory_snapshot(),
                     "entries=" + std::to_string(indexed_songs_.size()) +
                         " visible=" + std::to_string(visible_song_count()));
}

}  // namespace tenriff::app
