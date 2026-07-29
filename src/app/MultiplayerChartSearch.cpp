#include "app/MultiplayerChartSearch.h"

#include <filesystem>
#include <unordered_set>

#include "app/MenuSongUtils.h"
#include "app/MultiplayerMenuState.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {

namespace {

std::string normalized_source_key(std::string_view source) {
    return menu_songs::normalize_path_key(util::path_from_utf8_lossy(source));
}

}  // namespace

std::vector<std::string> multiplayer_loaded_song_sources(
    std::string_view active_source,
    const std::vector<std::string>& recent_sources) {
    std::vector<std::string> sources;
    sources.reserve(recent_sources.size() + 1);
    std::unordered_set<std::string> seen;
    seen.reserve(recent_sources.size() + 1);

    const auto append = [&](std::string_view raw_source) {
        if (raw_source.empty() || sources.size() >= kMultiplayerLoadedSongSourceLimit) {
            return;
        }
        const std::string normalized =
            menu_songs::normalize_song_source_path(std::string(raw_source));
        if (normalized.empty()) {
            return;
        }
        std::string key = normalized_source_key(normalized);
        if (key.empty()) {
            key = normalized;
        }
        if (seen.insert(std::move(key)).second) {
            sources.push_back(normalized);
        }
    };

    append(active_source);
    for (const auto& source : recent_sources) {
        append(source);
    }
    return sources;
}

std::string multiplayer_chart_path_for_source(const SongEntry& entry,
                                               std::string_view source_root) {
    return multiplayer_chart_path_for_source(entry.path, source_root);
}

std::string multiplayer_chart_path_for_source(std::string_view indexed_path,
                                               std::string_view source_root) {
    namespace fs = std::filesystem;
    try {
        fs::path root = util::path_from_utf8_lossy(source_root);
        fs::path candidate = util::path_from_utf8_lossy(indexed_path);
        if (root.empty() || candidate.empty()) {
            return {};
        }
        std::error_code ec;
        if (!root.is_absolute()) {
            const fs::path absolute_root = fs::absolute(root, ec);
            if (ec || absolute_root.empty()) {
                return {};
            }
            root = absolute_root;
        }
        root = root.lexically_normal();
        if (!candidate.is_absolute()) {
            candidate = root / candidate;
        }

        ec.clear();
        const fs::path canonical = fs::weakly_canonical(candidate, ec);
        if (!ec && !canonical.empty()) {
            candidate = canonical;
        } else {
            candidate = candidate.lexically_normal();
        }

        const fs::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute()) {
            return {};
        }
        for (const auto& component : relative) {
            if (component == "..") {
                return {};
            }
        }
        return candidate.u8string();
    } catch (...) {
        return {};
    }
}

std::vector<MultiplayerChartSearchCandidate> build_multiplayer_chart_candidates(
    const std::vector<SongEntry>& entries,
    std::string_view source_root,
    std::string_view remote_name) {
    std::vector<MultiplayerChartSearchCandidate> preferred;
    std::vector<MultiplayerChartSearchCandidate> remaining;
    preferred.reserve(entries.size());
    remaining.reserve(entries.size());
    const std::string normalized_root =
        menu_songs::normalize_song_source_path(std::string(source_root));
    if (normalized_root.empty()) {
        return preferred;
    }

    for (const auto& entry : entries) {
        if (entry.path.empty()) {
            continue;
        }
        MultiplayerChartSearchCandidate candidate;
        candidate.source_root = normalized_root;
        candidate.indexed_path = entry.path;
        candidate.title = entry.title.empty() ? entry.path : entry.title;
        if (multiplayer_chart_candidate_name_matches(entry.title, entry.path, remote_name)) {
            preferred.push_back(std::move(candidate));
        } else {
            remaining.push_back(std::move(candidate));
        }
    }

    preferred.reserve(preferred.size() + remaining.size());
    for (auto& candidate : remaining) {
        preferred.push_back(std::move(candidate));
    }
    return preferred;
}

MultiplayerChartCandidateLoadResult
load_multiplayer_chart_candidates_from_profile_cache(
    std::string_view profile_root,
    std::string_view source_root,
    const SongIndexOptions& options,
    std::string_view remote_name) {
    MultiplayerChartCandidateLoadResult result;
    const std::string cache_path = song_index_cache_path_for_source(profile_root, source_root);
    if (cache_path.empty()) {
        return result;
    }

    SongIndexLoadResult loaded = load_song_index(cache_path, options);
    if (!loaded.success()) {
        result.error = std::move(loaded.error);
        return result;
    }
    if (!loaded.loaded_from_file) {
        return result;
    }

    result.loaded_from_cache = true;
    result.candidates = build_multiplayer_chart_candidates(
        loaded.index.entries, source_root, remote_name);
    return result;
}

}  // namespace tenriff::app
