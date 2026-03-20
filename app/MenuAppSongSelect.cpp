#include "app/MenuApp.h"

#include <filesystem>
#include <utility>

#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"

namespace tenriff::app {

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

void MenuApp::update_song_list(SongIndex index) {
    using namespace menu_song_select;

    std::string selected_path = selected_song_path();
    song_background_preview_cache_.clear();
    indexed_songs_ = std::move(index.entries);
    // Sanitize once when the source list changes so later view rebuilds can stay cheap.
    for (auto& entry : indexed_songs_) {
        entry.title = safe_ui_text(entry.title);
        entry.artist = safe_ui_text(entry.artist);
        entry.format = safe_ui_text(entry.format);
    }
    rebuild_visible_song_list(selected_path.empty() ? nullptr : &selected_path);
    sync_song_select_state();
}

} // namespace tenriff::app
