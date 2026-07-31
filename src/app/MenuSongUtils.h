#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::chart {
struct BmsChart;
}

namespace tenriff::app::menu_songs {

bool is_bms_chart_extension(std::string_view ext);
bool is_supported_chart_extension(std::string_view ext);
std::optional<std::string> normalize_dropped_song_source(const std::string& raw_path);
std::string normalize_song_source_path(const std::string& raw_path);
std::string song_source_display_name(const std::string& raw_path);
std::string normalize_path_key(const std::filesystem::path& raw_path);
std::vector<std::string> build_chart_path_keys(const std::string& chart_path, const std::string& songs_root);
std::string resolve_bms_background_preview_path(const std::filesystem::path& chart_path,
                                                const chart::BmsChart& chart);
std::string resolve_song_background_preview_path(const std::string& chart_path);
std::string resolve_bms_audio_preview_path(const std::filesystem::path& chart_path,
                                           const chart::BmsChart& chart);
std::string resolve_song_audio_preview_path(const std::string& chart_path);

}  // namespace tenriff::app::menu_songs
