#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

enum class SongIndexProfile {
    Safe,
    Fast,
};

struct SongEntry {
    std::string path;
    std::string title;
    std::string artist;
    std::string chart_name;
    std::string format;
    std::string layout_label;
    std::string background_preview_path;
    int key_count = 0;
    int level = 0;
    int native_level = 0;
    double rating = 0.0;
    double bpm = 0.0;
    int64_t mtime = 0;
    std::uint64_t file_size = 0;
    std::string md5;
    std::string sha256;
    std::string difficulty_table_name;
    std::string difficulty_table_symbol;
    std::string difficulty_table_level;
    int difficulty_table_order = -1;
};

struct SongIndex {
    std::vector<SongEntry> entries;
};

struct SongIndexOptions {
    SongIndexProfile profile = SongIndexProfile::Safe;
    std::string difficulty_table_path;
    // F5/manual full rescans disable this so timestamp-preserving replacements
    // cannot keep stale metadata or difficulty-table hashes.
    bool reuse_cached_metadata = true;
};

enum class SongIndexProgressStage {
    ScanningFiles,
    BuildingMetadata,
    SavingCache,
};

struct SongIndexProgress {
    SongIndexProgressStage stage = SongIndexProgressStage::ScanningFiles;
    int processed = 0;
    int total = -1;
};

struct SongIndexLoadResult {
    SongIndex index;
    std::vector<std::string> warnings;
    std::string error;
    bool loaded_from_file = false;

    [[nodiscard]] bool success() const { return error.empty(); }
};

using SongIndexProgressCallback = std::function<void(const SongIndexProgress&)>;
using SongIndexCancelCallback = std::function<bool()>;

[[nodiscard]] std::string song_index_cache_path_for_source(std::string_view profile_root,
                                                           std::string_view source_root);
[[nodiscard]] std::string legacy_song_index_cache_path_for_source(std::string_view source_root);
SongIndexLoadResult load_song_index(const std::string& path, const SongIndexOptions& options = {});
bool save_song_index(const std::string& path,
                     const SongIndex& index,
                     const SongIndexOptions& options = {},
                     std::string* error = nullptr,
                     SongIndexProgressCallback progress = {},
                     SongIndexCancelCallback cancel = {});

SongIndex scan_songs(const std::string& root_path,
                     const SongIndex* cache,
                     std::vector<std::string>& warnings,
                     SongIndexProgressCallback progress = {},
                     const SongIndexOptions& options = {},
                     SongIndexCancelCallback cancel = {});

}  // namespace tenriff::app
