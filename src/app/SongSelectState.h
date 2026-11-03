#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app {

inline constexpr int kNativeSongSelectVisibleCardCount = 10;
inline constexpr int kLegacySongSelectVisibleCardCount = 7;

// The native compact list and authored layouts share the same selection model,
// but authored rectangles retain the seven-row viewport they were made for.
[[nodiscard]] inline constexpr int song_select_visible_card_count(bool native_list_layout) {
    return native_list_layout ? kNativeSongSelectVisibleCardCount : kLegacySongSelectVisibleCardCount;
}

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
