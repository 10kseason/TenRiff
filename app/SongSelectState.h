#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app {

struct SongSelectState {
    int selected_song = 0;
    int selected_source = 0;
    bool showing_sources = false;
};

inline int resolve_selected_song_index(const std::vector<SongEntry>& songs,
                                       int requested_index,
                                       const std::string* preserved_path = nullptr) {
    if (songs.empty()) {
        return 0;
    }

    if (preserved_path && !preserved_path->empty()) {
        for (std::size_t i = 0; i < songs.size(); ++i) {
            if (songs[i].path == *preserved_path) {
                return static_cast<int>(i);
            }
        }
    }

    return std::clamp(requested_index, 0, static_cast<int>(songs.size() - 1));
}

inline void sync_song_select_state(SongSelectState& state, std::size_t song_count, std::size_t source_count) {
    state.selected_song =
        (song_count == 0) ? 0 : std::clamp(state.selected_song, 0, static_cast<int>(song_count - 1));
    state.selected_source =
        (source_count == 0) ? 0 : std::clamp(state.selected_source, 0, static_cast<int>(source_count - 1));

    if (state.showing_sources && source_count == 0) {
        state.showing_sources = false;
    }
}

}  // namespace tenriff::app
