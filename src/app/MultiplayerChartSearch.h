#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "app/SongIndex.h"

namespace tenriff::app {

struct MultiplayerChartSearchCandidate {
    std::string source_root;
    std::string indexed_path;
    std::string title;
};

inline constexpr std::size_t kMultiplayerLoadedSongSourceLimit = 12;

using MultiplayerChartHashSet = std::unordered_set<std::string>;

/// Normalizes an exact SHA-256 chart identity to lowercase hexadecimal.
/// Invalid or incomplete values return an empty string.
[[nodiscard]] std::string normalize_multiplayer_chart_sha256(std::string_view sha256);

/// Builds the compact, sorted inventory announced to the connected peer.
[[nodiscard]] std::vector<std::string> build_multiplayer_chart_sha256_inventory(
    const std::vector<SongEntry>& entries);

[[nodiscard]] bool multiplayer_chart_is_shared(
    const SongEntry& entry,
    const MultiplayerChartHashSet& remote_sha256);

[[nodiscard]] std::size_t count_shared_multiplayer_charts(
    const std::vector<SongEntry>& entries,
    const MultiplayerChartHashSet& remote_sha256);

struct MultiplayerChartCandidateLoadResult {
    std::vector<MultiplayerChartSearchCandidate> candidates;
    std::string error;
    bool loaded_from_cache = false;
};

/// Returns only the active and profile-recent song sources, normalized and
/// deduplicated with the active source first. This intentionally never scans
/// parent folders or the rest of the machine.
[[nodiscard]] std::vector<std::string> multiplayer_loaded_song_sources(
    std::string_view active_source,
    const std::vector<std::string>& recent_sources);

/// Resolves an indexed relative chart path against the source that produced
/// that index, rather than against whichever source happens to be active now.
[[nodiscard]] std::string multiplayer_chart_path_for_source(
    const SongEntry& entry,
    std::string_view source_root);
[[nodiscard]] std::string multiplayer_chart_path_for_source(
    std::string_view indexed_path,
    std::string_view source_root);

/// Builds a title-prioritized candidate list. Fingerprint and file-size checks
/// remain mandatory at the call site before a candidate can be accepted.
[[nodiscard]] std::vector<MultiplayerChartSearchCandidate>
build_multiplayer_chart_candidates(const std::vector<SongEntry>& entries,
                                   std::string_view source_root,
                                   std::string_view remote_name);

/// Loads only the existing profile-local cache for one previously loaded song
/// source. It never starts a rescan and never searches an unlisted directory.
[[nodiscard]] MultiplayerChartCandidateLoadResult
load_multiplayer_chart_candidates_from_profile_cache(
    std::string_view profile_root,
    std::string_view source_root,
    const SongIndexOptions& options,
    std::string_view remote_name);

}  // namespace tenriff::app
