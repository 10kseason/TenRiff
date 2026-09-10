#pragma once

#include <algorithm>
#include "app/MenuSongUtils.h"
#include "config/Config.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

// Removes only the saved reference. No filesystem mutation is performed here.
inline bool remove_song_source_history(config::UiConfig& ui, int& selected) {
    if (selected < 0 || selected >= static_cast<int>(ui.recent_song_sources.size())) return false;
    const auto key = [](const std::string& path) {
        return menu_songs::normalize_path_key(util::path_from_utf8_lossy(path));
    };
    if (!ui.active_song_source.empty() &&
        key(ui.active_song_source) == key(ui.recent_song_sources[static_cast<std::size_t>(selected)])) {
        ui.active_song_source.clear();
    }
    ui.recent_song_sources.erase(ui.recent_song_sources.begin() + selected);
    ui.song_sources_initialized = true;
    selected = ui.recent_song_sources.empty() ? 0 :
        std::min(selected, static_cast<int>(ui.recent_song_sources.size()) - 1);
    return true;
}

inline bool song_source_history_is_intentionally_empty(const config::UiConfig& ui) {
    return ui.song_sources_initialized && ui.active_song_source.empty() && ui.recent_song_sources.empty();
}

} // namespace tenriff::app
