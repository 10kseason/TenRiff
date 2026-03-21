#include "app/MenuApp.h"

#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <utility>

#include "app/MenuRecordUtils.h"
#include "app/MenuAppSettingsUtils.h"
#include "app/MenuAppSongSelectUtils.h"
#include "app/MenuSongUtils.h"

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

            const int best_judged = best.perfect + best.great + best.good + best.bad + best.miss;
            const int found_judged = found->second.perfect + found->second.great + found->second.good +
                                     found->second.bad + found->second.miss;
            if (menu_records::is_better_record(found->second.best_score,
                                               menu_records::clear_status_priority(found->second.clear_status,
                                                                                   found->second.game_over,
                                                                                   found->second.final_gauge),
                                               found->second.max_combo,
                                               found_judged,
                                               found->second.created_utc,
                                               best.best_score,
                                               menu_records::clear_status_priority(best.clear_status,
                                                                                   best.game_over,
                                                                                   best.final_gauge),
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

void MenuApp::update_song_list(SongIndex index) {
    using namespace menu_song_select;

    std::string selected_path = selected_song_path();
    indexed_songs_ = std::move(index.entries);
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
