#include "app/MenuAppSongSelectUtils.h"

#include <algorithm>

#include "app/MenuAppSettingsUtils.h"
#include "util/Utf8Compat.h"

namespace tenriff::app::menu_song_select {

namespace {

std::string song_layout_label(const SongEntry& entry) {
    if (!entry.layout_label.empty()) {
        return entry.layout_label;
    }
    return key_mode_label(std::to_string(std::max(1, entry.key_count)) + "k");
}

std::string song_sort_title_key(const SongEntry& entry) {
    const std::string display = entry.title.empty() ? entry.path : entry.title;
    return to_lower_ascii(display);
}

std::string song_sort_artist_key(const SongEntry& entry) {
    const std::string display = entry.artist.empty() ? song_title_for_ui(entry) : entry.artist;
    return to_lower_ascii(display);
}

std::string level_filter_label(int level_min, int level_max) {
    if (level_min <= 0 && level_max <= 0) {
        return "All Levels";
    }
    if (level_min > 0 && level_max > 0) {
        return "LV " + std::to_string(level_min) + "-" + std::to_string(level_max);
    }
    if (level_min > 0) {
        return "LV " + std::to_string(level_min) + "+";
    }
    return "LV <= " + std::to_string(level_max);
}

}  // namespace

std::filesystem::path path_from_utf8(std::string_view value) {
    try {
        return util::path_from_utf8_lossy(value);
    } catch (...) {
        return {};
    }
}

std::string safe_ui_text(std::string_view value, std::string_view fallback) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return std::string(fallback);
}

std::string safe_ui_text_or_placeholder(std::string_view value, std::string_view placeholder) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return value.empty() ? std::string{} : std::string(placeholder);
}

std::string song_title_for_ui(const SongEntry& entry) {
    std::string title = util::sanitize_ui_text(entry.title);
    if (!title.empty()) {
        return title;
    }
    std::string path = util::sanitize_ui_text(entry.path);
    if (!path.empty()) {
        return path;
    }
    return "<invalid title>";
}

std::string song_artist_for_ui(const SongEntry& entry) {
    return safe_ui_text_or_placeholder(entry.artist, "<invalid artist>");
}

std::string song_group_artist_key(const SongEntry& entry) {
    const std::string artist = safe_ui_text(entry.artist);
    if (!artist.empty()) {
        return to_lower_ascii(artist);
    }
    return "~unknown-artist";
}

std::string song_group_level_key(const SongEntry& entry) {
    if (entry.level > 0) {
        const int clamped_level = std::clamp(entry.level, 0, 9999);
        const int thousands = clamped_level / 1000;
        const int hundreds = (clamped_level / 100) % 10;
        const int tens = (clamped_level / 10) % 10;
        const int ones = clamped_level % 10;
        std::string key = "lv:";
        key.push_back(static_cast<char>('0' + thousands));
        key.push_back(static_cast<char>('0' + hundreds));
        key.push_back(static_cast<char>('0' + tens));
        key.push_back(static_cast<char>('0' + ones));
        return key;
    }
    return "lv:9999";
}

std::string song_group_folder_label(const SongEntry& entry) {
    const std::filesystem::path parent = path_from_utf8(entry.path).parent_path();
    if (parent.empty()) {
        return {};
    }
    const std::string label = util::sanitize_ui_text(parent.filename().u8string());
    if (!label.empty()) {
        return label;
    }
    const std::string fallback = util::sanitize_ui_text(parent.u8string());
    return fallback;
}

std::string song_group_folder_key(const SongEntry& entry) {
    const std::string label = song_group_folder_label(entry);
    if (!label.empty()) {
        return to_lower_ascii(label);
    }
    return "root";
}

std::string song_detail_label(const SongEntry& entry) {
    std::string detail = song_layout_label(entry) + " " + format_label(to_lower_ascii(entry.format));
    const std::string chart_name = safe_ui_text(entry.chart_name);
    if (!chart_name.empty()) {
        detail += " / " + chart_name;
    }
    return detail;
}

std::string song_index_stage_label(SongIndexProgressStage stage) {
    switch (stage) {
    case SongIndexProgressStage::ScanningFiles:
        return "SCANNING FILES";
    case SongIndexProgressStage::BuildingMetadata:
        return "BUILDING METADATA";
    case SongIndexProgressStage::SavingCache:
        return "WRITING CACHE";
    default:
        return "INDEXING";
    }
}

std::string format_eta_seconds(int64_t seconds) {
    if (seconds <= 0) {
        return "0s";
    }
    const int64_t hours = seconds / 3600;
    const int64_t minutes = (seconds % 3600) / 60;
    const int64_t secs = seconds % 60;
    if (hours > 0) {
        return std::to_string(hours) + "h" + std::to_string(minutes) + "m";
    }
    if (minutes > 0) {
        return std::to_string(minutes) + "m" + std::to_string(secs) + "s";
    }
    return std::to_string(secs) + "s";
}

std::string format_int_with_commas(int64_t value) {
    const bool negative = value < 0;
    uint64_t abs_value = 0;
    if (negative) {
        abs_value = static_cast<uint64_t>(-(value + 1)) + 1;
    } else {
        abs_value = static_cast<uint64_t>(value);
    }
    std::string digits = std::to_string(abs_value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count == 3) {
            out.push_back(',');
            count = 0;
        }
        out.push_back(*it);
        ++count;
    }
    if (negative) {
        out.push_back('-');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool song_entry_matches_chart_filter(const SongEntry& entry, std::string_view filter) {
    const std::string normalized_filter = normalize_chart_filter(std::string(filter));
    if (normalized_filter == "auto") {
        return true;
    }
    return to_lower_ascii(entry.format) == normalized_filter;
}

bool song_entry_matches_search(const SongEntry& entry, std::string_view query) {
    const std::string normalized_query = to_lower_ascii(std::string(query));
    if (normalized_query.empty()) {
        return true;
    }
    const std::string haystacks[] = {
        to_lower_ascii(song_title_for_ui(entry)),
        to_lower_ascii(song_artist_for_ui(entry)),
        to_lower_ascii(entry.path),
    };
    for (const auto& haystack : haystacks) {
        if (haystack.find(normalized_query) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool song_entry_matches_key_filter(const SongEntry& entry, int key_filter) {
    return key_filter <= 0 || entry.key_count == key_filter;
}

bool song_entry_matches_level_filter(const SongEntry& entry, int level_min, int level_max) {
    if (level_min <= 0 && level_max <= 0) {
        return true;
    }
    if (entry.level <= 0) {
        return false;
    }
    if (level_min > 0 && entry.level < level_min) {
        return false;
    }
    if (level_max > 0 && entry.level > level_max) {
        return false;
    }
    return true;
}

bool song_entry_less_by_difficulty_asc(const SongEntry& lhs, const SongEntry& rhs) {
    const int lhs_level = lhs.level > 0 ? lhs.level : 9999;
    const int rhs_level = rhs.level > 0 ? rhs.level : 9999;
    if (lhs_level != rhs_level) {
        return lhs_level < rhs_level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating < rhs.rating;
    }
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_difficulty_desc(const SongEntry& lhs, const SongEntry& rhs) {
    if (lhs.level != rhs.level) {
        return lhs.level > rhs.level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating > rhs.rating;
    }
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_title_asc(const SongEntry& lhs, const SongEntry& rhs) {
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    const int lhs_level = lhs.level > 0 ? lhs.level : 9999;
    const int rhs_level = rhs.level > 0 ? rhs.level : 9999;
    if (lhs_level != rhs_level) {
        return lhs_level < rhs_level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating < rhs.rating;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_title_desc(const SongEntry& lhs, const SongEntry& rhs) {
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title > rhs_title;
    }
    if (lhs.level != rhs.level) {
        return lhs.level > rhs.level;
    }
    if (lhs.rating != rhs.rating) {
        return lhs.rating > rhs.rating;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_artist_asc(const SongEntry& lhs, const SongEntry& rhs) {
    const std::string lhs_artist = song_sort_artist_key(lhs);
    const std::string rhs_artist = song_sort_artist_key(rhs);
    if (lhs_artist != rhs_artist) {
        return lhs_artist < rhs_artist;
    }
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    const int lhs_level = lhs.level > 0 ? lhs.level : 9999;
    const int rhs_level = rhs.level > 0 ? rhs.level : 9999;
    if (lhs_level != rhs_level) {
        return lhs_level < rhs_level;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

bool song_entry_less_by_artist_desc(const SongEntry& lhs, const SongEntry& rhs) {
    const std::string lhs_artist = song_sort_artist_key(lhs);
    const std::string rhs_artist = song_sort_artist_key(rhs);
    if (lhs_artist != rhs_artist) {
        return lhs_artist > rhs_artist;
    }
    const std::string lhs_title = song_sort_title_key(lhs);
    const std::string rhs_title = song_sort_title_key(rhs);
    if (lhs_title != rhs_title) {
        return lhs_title < rhs_title;
    }
    if (lhs.level != rhs.level) {
        return lhs.level > rhs.level;
    }
    return to_lower_ascii(lhs.path) < to_lower_ascii(rhs.path);
}

std::string key_filter_label(int key_filter) {
    return key_filter <= 0 ? "All Keys" : key_mode_label(std::to_string(key_filter) + "k");
}

std::string browser_summary_label(std::string_view query, int key_filter, int level_min, int level_max) {
    std::vector<std::string> parts;
    if (!query.empty()) {
        parts.push_back("Q " + safe_ui_text(query));
    }
    static_cast<void>(key_filter);
    if (level_min > 0 || level_max > 0) {
        parts.push_back(level_filter_label(level_min, level_max));
    }
    if (parts.empty()) {
        return "NO FILTER";
    }
    std::string joined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            joined += " / ";
        }
        joined += parts[i];
    }
    return joined;
}

std::string filename_only(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    return path_from_utf8(path).filename().u8string();
}

SongMembershipSet build_song_membership_set(const std::vector<std::string>& values) {
    SongMembershipSet membership;
    membership.reserve(values.size());
    for (const auto& value : values) {
        if (!value.empty()) {
            membership.emplace(value);
        }
    }
    return membership;
}

SongCollectionMembershipLookup build_song_collection_membership_lookup(
    const std::unordered_map<std::string, std::vector<std::string>>& collections) {
    SongCollectionMembershipLookup lookup;
    lookup.reserve(collections.size());
    for (const auto& [name, items] : collections) {
        lookup.emplace(name, build_song_membership_set(items));
    }
    return lookup;
}

bool song_membership_contains(const SongMembershipSet& values, std::string_view target) {
    if (target.empty()) {
        return false;
    }
    return values.find(std::string(target)) != values.end();
}

bool song_collection_membership_contains(const SongCollectionMembershipLookup& lookup,
                                         std::string_view collection_name,
                                         std::string_view target) {
    if (collection_name.empty() || target.empty()) {
        return false;
    }
    const auto it = lookup.find(std::string(collection_name));
    if (it == lookup.end()) {
        return false;
    }
    return song_membership_contains(it->second, target);
}

int count_song_membership_matches(const std::vector<std::string>& song_keys, const SongMembershipSet& membership) {
    int count = 0;
    for (const auto& key : song_keys) {
        if (song_membership_contains(membership, key)) {
            ++count;
        }
    }
    return count;
}

}  // namespace tenriff::app::menu_song_select
