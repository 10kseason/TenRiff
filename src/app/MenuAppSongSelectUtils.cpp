#include "app/MenuAppSongSelectUtils.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

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

int compare_natural_ascii(std::string_view lhs, std::string_view rhs) {
    std::size_t left = 0;
    std::size_t right = 0;
    while (left < lhs.size() && right < rhs.size()) {
        const unsigned char lhs_byte = static_cast<unsigned char>(lhs[left]);
        const unsigned char rhs_byte = static_cast<unsigned char>(rhs[right]);
        if (std::isdigit(lhs_byte) != 0 && std::isdigit(rhs_byte) != 0) {
            const std::size_t lhs_run_begin = left;
            const std::size_t rhs_run_begin = right;
            while (left < lhs.size() && lhs[left] == '0') ++left;
            while (right < rhs.size() && rhs[right] == '0') ++right;
            const std::size_t lhs_digits_begin = left;
            const std::size_t rhs_digits_begin = right;
            while (left < lhs.size() && std::isdigit(static_cast<unsigned char>(lhs[left])) != 0) ++left;
            while (right < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[right])) != 0) ++right;
            const std::size_t lhs_digits = left - lhs_digits_begin;
            const std::size_t rhs_digits = right - rhs_digits_begin;
            if (lhs_digits != rhs_digits) return lhs_digits < rhs_digits ? -1 : 1;
            const int numeric_compare = lhs.substr(lhs_digits_begin, lhs_digits).compare(
                rhs.substr(rhs_digits_begin, rhs_digits));
            if (numeric_compare != 0) return numeric_compare < 0 ? -1 : 1;
            const std::size_t lhs_run = left - lhs_run_begin;
            const std::size_t rhs_run = right - rhs_run_begin;
            if (lhs_run != rhs_run) return lhs_run < rhs_run ? -1 : 1;
            continue;
        }

        const unsigned char lhs_folded = static_cast<unsigned char>(
            lhs_byte >= 'A' && lhs_byte <= 'Z' ? lhs_byte + ('a' - 'A') : lhs_byte);
        const unsigned char rhs_folded = static_cast<unsigned char>(
            rhs_byte >= 'A' && rhs_byte <= 'Z' ? rhs_byte + ('a' - 'A') : rhs_byte);
        if (lhs_folded != rhs_folded) return lhs_folded < rhs_folded ? -1 : 1;
        ++left;
        ++right;
    }
    if (left == lhs.size() && right == rhs.size()) return 0;
    return left == lhs.size() ? -1 : 1;
}

std::string natural_level_sort_key(std::string_view value) {
    std::string key;
    key.reserve(value.size() + 16);
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const unsigned char byte = static_cast<unsigned char>(value[cursor]);
        if (std::isdigit(byte) == 0) {
            key.push_back(static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte));
            ++cursor;
            continue;
        }
        const std::size_t run_begin = cursor;
        while (cursor < value.size() && value[cursor] == '0') ++cursor;
        const std::size_t digits_begin = cursor;
        while (cursor < value.size() && std::isdigit(static_cast<unsigned char>(value[cursor])) != 0) ++cursor;
        const std::size_t digits = cursor - digits_begin;
        key.push_back('\x01');
        std::ostringstream length;
        length << std::setw(10) << std::setfill('0') << digits;
        key += length.str();
        key.append(value.substr(digits_begin, digits));
        key.push_back('\x02');
        std::ostringstream run_length;
        run_length << std::setw(10) << std::setfill('0') << (cursor - run_begin);
        key += run_length.str();
    }
    return key;
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
    if (!entry.difficulty_table_level.empty()) {
        if (entry.difficulty_table_order >= 0) {
            const int clamped_order = std::clamp(entry.difficulty_table_order, 0, 999999);
            std::string key = "0table:";
            const std::string digits = std::to_string(clamped_order);
            key.append(6u - std::min<std::size_t>(6u, digits.size()), '0');
            key += digits;
            return key;
        }
        return "0table:zz:" + natural_level_sort_key(entry.difficulty_table_level);
    }
    if (entry.level > 0) {
        const int clamped_level = std::clamp(entry.level, 0, 9999);
        const int thousands = clamped_level / 1000;
        const int hundreds = (clamped_level / 100) % 10;
        const int tens = (clamped_level / 10) % 10;
        const int ones = clamped_level % 10;
        std::string key = "1native:";
        key.push_back(static_cast<char>('0' + thousands));
        key.push_back(static_cast<char>('0' + hundreds));
        key.push_back(static_cast<char>('0' + tens));
        key.push_back(static_cast<char>('0' + ones));
        return key;
    }
    return "1native:9999";
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
    std::string detail = song_layout_label(entry) + " BMS";
    const std::string chart_name = safe_ui_text(entry.chart_name);
    if (!chart_name.empty()) {
        detail += " / " + chart_name;
    }
    if (!entry.difficulty_table_level.empty()) {
        const std::string table_name = safe_ui_text(entry.difficulty_table_name);
        detail += " / ";
        if (!table_name.empty()) {
            detail += table_name + " ";
        }
        detail += song_difficulty_label(entry);
    }
    return detail;
}

std::string song_difficulty_label(const SongEntry& entry) {
    if (!entry.difficulty_table_level.empty()) {
        return safe_ui_text(entry.difficulty_table_symbol) +
               safe_ui_text(entry.difficulty_table_level);
    }
    if (entry.level > 0) {
        return "LV " + std::to_string(entry.level);
    }
    return {};
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

bool song_entry_matches_search(const SongEntry& entry, std::string_view query) {
    const std::string normalized_query = to_lower_ascii(std::string(query));
    if (normalized_query.find_first_not_of(" \t") == std::string::npos) {
        return true;
    }

    // Every field the player can see on a card is searchable, so "cn 7k another"
    // narrows by artist, layout and chart name at once.
    const std::string haystacks[] = {
        to_lower_ascii(song_title_for_ui(entry)),
        to_lower_ascii(song_artist_for_ui(entry)),
        to_lower_ascii(entry.path),
        to_lower_ascii(entry.chart_name),
        to_lower_ascii(entry.layout_label),
        to_lower_ascii(entry.format),
        to_lower_ascii(entry.difficulty_table_name),
        to_lower_ascii(entry.difficulty_table_symbol),
        to_lower_ascii(entry.difficulty_table_level),
        std::to_string(entry.key_count) + "k",
        "lv" + std::to_string(entry.level),
    };

    // Space-separated terms all have to land, each in any one field.
    std::size_t cursor = 0;
    while (cursor < normalized_query.size()) {
        const std::size_t begin = normalized_query.find_first_not_of(" \t", cursor);
        if (begin == std::string::npos) {
            break;
        }
        std::size_t end = normalized_query.find_first_of(" \t", begin);
        if (end == std::string::npos) {
            end = normalized_query.size();
        }
        const std::string_view term(normalized_query.data() + begin, end - begin);
        bool term_found = false;
        for (const auto& haystack : haystacks) {
            if (haystack.find(term) != std::string::npos) {
                term_found = true;
                break;
            }
        }
        if (!term_found) {
            return false;
        }
        cursor = end;
    }
    return true;
}

bool song_entry_matches_key_filter(const SongEntry& entry, int key_filter) {
    return key_filter <= 0 || entry.key_count == key_filter;
}

bool song_entry_matches_level_filter(const SongEntry& entry, int level_min, int level_max) {
    if (level_min <= 0 && level_max <= 0) {
        return true;
    }
    int effective_level = entry.level;
    if (!entry.difficulty_table_level.empty()) {
        const std::string& table_level = entry.difficulty_table_level;
        int parsed_level = 0;
        const auto parsed = std::from_chars(table_level.data(),
                                            table_level.data() + table_level.size(),
                                            parsed_level);
        if (parsed.ec == std::errc{} && parsed.ptr != table_level.data()) {
            effective_level = parsed_level;
        }
    }
    if (effective_level <= 0) {
        return false;
    }
    if (level_min > 0 && effective_level < level_min) {
        return false;
    }
    if (level_max > 0 && effective_level > level_max) {
        return false;
    }
    return true;
}

bool song_entry_less_by_difficulty_asc(const SongEntry& lhs, const SongEntry& rhs) {
    const bool lhs_table = !lhs.difficulty_table_level.empty();
    const bool rhs_table = !rhs.difficulty_table_level.empty();
    if (lhs_table != rhs_table) {
        return lhs_table;
    }
    if (lhs_table) {
        const int lhs_order = lhs.difficulty_table_order >= 0
                                  ? lhs.difficulty_table_order
                                  : (std::numeric_limits<int>::max)();
        const int rhs_order = rhs.difficulty_table_order >= 0
                                  ? rhs.difficulty_table_order
                                  : (std::numeric_limits<int>::max)();
        if (lhs_order != rhs_order) {
            return lhs_order < rhs_order;
        }
        const int level_compare = compare_natural_ascii(lhs.difficulty_table_level,
                                                        rhs.difficulty_table_level);
        if (level_compare != 0) {
            return level_compare < 0;
        }
    }
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
    const bool lhs_table = !lhs.difficulty_table_level.empty();
    const bool rhs_table = !rhs.difficulty_table_level.empty();
    if (lhs_table != rhs_table) {
        return lhs_table;
    }
    if (lhs_table) {
        const int lhs_order = lhs.difficulty_table_order >= 0
                                  ? lhs.difficulty_table_order
                                  : (std::numeric_limits<int>::min)();
        const int rhs_order = rhs.difficulty_table_order >= 0
                                  ? rhs.difficulty_table_order
                                  : (std::numeric_limits<int>::min)();
        if (lhs_order != rhs_order) {
            return lhs_order > rhs_order;
        }
        const int level_compare = compare_natural_ascii(lhs.difficulty_table_level,
                                                        rhs.difficulty_table_level);
        if (level_compare != 0) {
            return level_compare > 0;
        }
    }
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
