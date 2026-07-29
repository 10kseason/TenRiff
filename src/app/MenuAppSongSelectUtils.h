#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app::menu_song_select {

using SongMembershipSet = std::unordered_set<std::string>;
using SongCollectionMembershipLookup = std::unordered_map<std::string, SongMembershipSet>;

std::filesystem::path path_from_utf8(std::string_view value);
std::string safe_ui_text(std::string_view value, std::string_view fallback = {});
std::string safe_ui_text_or_placeholder(std::string_view value, std::string_view placeholder);
std::string song_title_for_ui(const SongEntry& entry);
std::string song_artist_for_ui(const SongEntry& entry);
std::string song_group_artist_key(const SongEntry& entry);
std::string song_group_level_key(const SongEntry& entry);
std::string song_group_folder_key(const SongEntry& entry);
std::string song_group_folder_label(const SongEntry& entry);
std::string song_difficulty_label(const SongEntry& entry);
std::string song_detail_label(const SongEntry& entry);
std::string song_index_stage_label(SongIndexProgressStage stage);
std::string format_eta_seconds(int64_t seconds);
std::string format_int_with_commas(int64_t value);
bool song_entry_matches_search(const SongEntry& entry, std::string_view query);
bool song_entry_matches_key_filter(const SongEntry& entry, int key_filter);
bool song_entry_matches_level_filter(const SongEntry& entry, int level_min, int level_max);
bool song_entry_less_by_difficulty_asc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_difficulty_desc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_title_asc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_title_desc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_artist_asc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_artist_desc(const SongEntry& lhs, const SongEntry& rhs);
std::string key_filter_label(int key_filter);
std::string browser_summary_label(std::string_view query, int key_filter, int level_min, int level_max);
std::string filename_only(const std::string& path);
SongMembershipSet build_song_membership_set(const std::vector<std::string>& values);
SongCollectionMembershipLookup build_song_collection_membership_lookup(
    const std::unordered_map<std::string, std::vector<std::string>>& collections);
bool song_membership_contains(const SongMembershipSet& values, std::string_view target);
bool song_collection_membership_contains(const SongCollectionMembershipLookup& lookup,
                                         std::string_view collection_name,
                                         std::string_view target);
int count_song_membership_matches(const std::vector<std::string>& song_keys, const SongMembershipSet& membership);

}  // namespace tenriff::app::menu_song_select
