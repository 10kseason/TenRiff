#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app {

struct SongSourceAggregateProgress {
    SongIndexProgress source;
    int source_index = 0;  // One-based while a root is being processed.
    int source_total = 0;
};

using SongSourceAggregateProgressCallback =
    std::function<void(const SongSourceAggregateProgress&)>;

struct SongSourceAggregateResult {
    SongIndex index;
    std::vector<std::string> warnings;
    // Keys match MenuSongUtils::normalize_path_key on the canonical source root.
    // Counts belong to each source, before cross-source chart deduplication.
    std::unordered_map<std::string, int> source_counts;
    int cached_sources = 0;
    int scanned_sources = 0;
    int missing_sources = 0;
    bool cancelled = false;
};

// Reads/scans one source at a time. A compatible source cache is reused without
// scanning unless force_rescan is true. Only the returned aggregate uses absolute
// paths; source caches retain their existing source-relative chart paths.
// Unavailable roots and cached chart files are skipped without changing caches.
// A cancelled result contains no index, so callers can keep the previous list.
SongSourceAggregateResult aggregate_song_sources(
    const std::vector<std::string>& source_roots,
    const std::string& profile_root,
    const SongIndexOptions& options = {},
    SongSourceAggregateProgressCallback progress = {},
    SongIndexCancelCallback cancel = {},
    bool force_rescan = false);

// Writes beside the cache and replaces it only after a complete, uncancelled
// save. A cancelled rescan must never truncate an existing usable cache.
bool save_song_source_cache_atomically(
    const std::string& cache_path,
    const SongIndex& index,
    const SongIndexOptions& options = {},
    std::string* error = nullptr,
    SongIndexProgressCallback progress = {},
    SongIndexCancelCallback cancel = {});

}  // namespace tenriff::app
