#include "app/MenuApp.h"

#include <algorithm>
#include <vector>

#include "app/MemoryDiagnostics.h"
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
    app::sync_song_select_state(state, visible_song_count(), config_.ui.recent_song_sources.size());
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
    using namespace menu_song_select;

    std::string preserved_path;
    if (selected_path) {
        preserved_path = *selected_path;
    } else {
        preserved_path = selected_song_path();
    }

    visible_song_indices_.clear();
    visible_song_indices_.reserve(indexed_songs_.size());
    for (std::size_t index = 0; index < indexed_songs_.size(); ++index) {
        const SongEntry& entry = indexed_songs_[index];
        if (song_entry_matches_search(entry, song_search_query_) &&
            song_entry_matches_key_filter(entry, song_key_filter_) &&
            song_entry_matches_level_filter(entry, song_level_min_filter_, song_level_max_filter_) &&
            song_entry_matches_collection_filter(entry)) {
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
                const bool reverse_group_order =
                    (song_group_mode_ == SongGroupMode::Artist && song_sort_mode_ == SongSortMode::ArtistDesc) ||
                    (song_group_mode_ == SongGroupMode::Level && song_sort_mode_ == SongSortMode::DifficultyDesc);
                return reverse_group_order ? (lhs_group > rhs_group) : (lhs_group < rhs_group);
            }
        }
        return song_sort_compare(lhs_entry, rhs_entry);
    });

    if (visible_song_indices_.empty()) {
        selected_song_ = 0;
        sync_song_select_state();
        if (!songs_path_.empty()) {
            source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] = 0;
        }
        return;
    }

    // Resolve selection against the filtered/sorted view, but preserve the original chart
    // path when possible so cursor focus survives reindex/filter/sort churn.
    std::vector<SongEntry> selection_view;
    selection_view.reserve(visible_song_indices_.size());
    for (std::size_t song_index : visible_song_indices_) {
        selection_view.push_back(indexed_songs_[song_index]);
    }
    selected_song_ = resolve_selected_song_index(selection_view,
                                                 selected_song_,
                                                 preserved_path.empty() ? nullptr : &preserved_path);
    sync_song_select_state();
    rebuild_current_song_record_indices();

    source_song_counts_[menu_songs::normalize_path_key(path_from_utf8(songs_path_))] =
        static_cast<int>(visible_song_count());
    log_memory_phase("MenuApp",
                     "visible-list-rebuilt",
                     query_process_memory_snapshot(),
                     "entries=" + std::to_string(indexed_songs_.size()) +
                         " visible=" + std::to_string(visible_song_count()));
}

}  // namespace tenriff::app
