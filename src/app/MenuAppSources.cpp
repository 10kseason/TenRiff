#include "app/MenuApp.h"

#include "app/SongIndex.h"
#include "app/SongSourceHistory.h"

namespace tenriff::app {

void MenuApp::open_selected_song_source() {
    // Row zero is a virtual collection, never a filesystem path or deletable folder.
    if (selected_source_ == 0) {
        switch_all_song_sources(false);
    } else if (const auto index = source_browser_history_index(selected_source_, config_.ui.recent_song_sources.size())) {
        switch_song_source(config_.ui.recent_song_sources[static_cast<std::size_t>(*index)], false);
    }
}

void MenuApp::switch_all_song_sources(bool force_reindex) {
    cancel_song_preview_decode();
    stop_song_preview_audio();
    song_select_screen_.clear_preview_target();
    song_indexer_.stop();
    last_indexer_snapshot_ns_ = 0;
    config_.ui.all_song_sources = true;
    config_.ui.song_sources_initialized = true;
    selected_source_ = 0;
    song_select_view_ = SongSelectView::Songs;

    SongIndexOptions options;
    options.difficulty_table_path = config_.ui.difficulty_table_path;
    options.calculate_difficulty = config_.mode.calculate_song_index_difficulty;
    options.reuse_cached_metadata = !force_reindex;
    options.profile = config::normalize_song_index_profile_token(config_.mode.song_index_profile) == "fast"
                          ? SongIndexProfile::Fast : SongIndexProfile::Safe;
    if (config_.ui.recent_song_sources.empty()) {
        update_song_list(SongIndex{});
        all_source_song_count_ = 0;
    } else {
        // Keep the current table usable until the worker atomically publishes the
        // combined absolute-path table. Cancelling cannot replace it with a partial scan.
        (void)song_indexer_.start_sources(config_.ui.recent_song_sources, profile_dir_, options, force_reindex);
    }
    reload_chart_best_results();
    persist_runtime_config();
    sync_song_select_state();
}

}  // namespace tenriff::app
