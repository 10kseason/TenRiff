#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "app/SongIndex.h"

namespace tenriff::app::menu_song_select {

std::filesystem::path path_from_utf8(std::string_view value);
std::string safe_ui_text(std::string_view value, std::string_view fallback = {});
std::string safe_ui_text_or_placeholder(std::string_view value, std::string_view placeholder);
std::string song_title_for_ui(const SongEntry& entry);
std::string song_artist_for_ui(const SongEntry& entry);
std::string song_detail_label(const SongEntry& entry);
std::string song_index_stage_label(SongIndexProgressStage stage);
std::string format_eta_seconds(int64_t seconds);
std::string format_int_with_commas(int64_t value);
bool song_entry_matches_chart_filter(const SongEntry& entry, std::string_view filter);
bool song_entry_matches_search(const SongEntry& entry, std::string_view query);
bool song_entry_matches_key_filter(const SongEntry& entry, int key_filter);
bool song_entry_matches_level_filter(const SongEntry& entry, int level_min, int level_max);
bool song_entry_less_by_difficulty_asc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_difficulty_desc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_title_asc(const SongEntry& lhs, const SongEntry& rhs);
bool song_entry_less_by_title_desc(const SongEntry& lhs, const SongEntry& rhs);
std::string key_filter_label(int key_filter);
std::string browser_summary_label(std::string_view query, int key_filter, int level_min, int level_max);
std::string filename_only(const std::string& path);

}  // namespace tenriff::app::menu_song_select
