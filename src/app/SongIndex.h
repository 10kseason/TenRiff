#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tenriff::app {

struct SongEntry {
    std::string path;
    std::string title;
    std::string artist;
    std::string format;
    std::string layout_label;
    int key_count = 0;
    int level = 0;
    double rating = 0.0;
    double bpm = 0.0;
    int64_t mtime = 0;
};

struct SongIndex {
    std::vector<SongEntry> entries;
};

struct SongIndexOptions {
    bool include_osu = false;
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

SongIndexLoadResult load_song_index(const std::string& path, const SongIndexOptions& options = {});
bool save_song_index(const std::string& path,
                     const SongIndex& index,
                     const SongIndexOptions& options = {},
                     std::string* error = nullptr,
                     SongIndexProgressCallback progress = {});

SongIndex scan_songs(const std::string& root_path,
                     const SongIndex* cache,
                     std::vector<std::string>& warnings,
                     SongIndexProgressCallback progress = {},
                     const SongIndexOptions& options = {});

}  // namespace tenriff::app
