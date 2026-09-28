#include "app/SongSourceAggregate.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <limits>
#include <unordered_set>
#include <utility>

#include "app/MenuSongUtils.h"
#include "util/Utf8Compat.h"

namespace tenriff::app {
namespace {
namespace fs = std::filesystem;

bool is_cancelled(const SongIndexCancelCallback& cancel) {
    return cancel && cancel();
}

int count_as_int(std::size_t count) {
    return static_cast<int>((std::min)(count, static_cast<std::size_t>((std::numeric_limits<int>::max)())));
}

fs::path canonical_absolute(fs::path path) {
    std::error_code ec;
    if (!path.is_absolute()) {
        path = fs::absolute(path, ec);
        if (ec) return {};
    }
    const auto canonical = fs::weakly_canonical(path, ec);
    return !ec && !canonical.empty() ? canonical : path.lexically_normal();
}

std::string chart_key(const fs::path& canonical) {
    std::string key = canonical.generic_u8string();
#ifdef _WIN32
    // Use the same ASCII case folding as menu path lookup on Windows, while
    // retaining distinct case-sensitive files on platforms that support them.
    for (char& ch : key) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
#endif
    return key;
}

std::string absolute_preview(const std::string& value, const fs::path& source_root) {
    if (value.empty()) return {};
    const auto path = util::path_from_utf8_lossy(value);
    // Current caches already store absolute previews. Older relative values
    // belong to their source root; they must not be resolved against ALL SONG.
    return util::path_to_utf8_lossy(canonical_absolute(path.is_absolute() ? path : source_root / path));
}

void append_warnings(std::vector<std::string>& target,
                     const std::vector<std::string>& warnings,
                     const std::string& source) {
    for (const auto& warning : warnings) target.push_back(source + ": " + warning);
}

SongIndexLoadResult load_source_cache(const std::string& primary,
                                     const std::string& legacy,
                                     const SongIndexOptions& options,
                                     std::vector<std::string>& warnings,
                                     const std::string& source) {
    auto loaded = load_song_index(primary, options);
    if (loaded.success() && loaded.loaded_from_file) {
        append_warnings(warnings, loaded.warnings, source);
        return loaded;
    }
    if (!loaded.error.empty()) warnings.push_back(source + ": " + loaded.error);
    // An absent/incompatible cache is an expected scan trigger, not a warning.
    // Legacy reads remain read-only; completed scans write profile-local caches.
    if (!legacy.empty() && legacy != primary) {
        auto fallback = load_song_index(legacy, options);
        if (fallback.success() && fallback.loaded_from_file) {
            append_warnings(warnings, fallback.warnings, source);
            return fallback;
        }
        if (!fallback.error.empty()) warnings.push_back(source + ": " + fallback.error);
    }
    return {};
}

}  // namespace

bool save_song_source_cache_atomically(const std::string& cache_path,
                                      const SongIndex& index,
                                      const SongIndexOptions& options,
                                      std::string* error,
                                      SongIndexProgressCallback progress,
                                      SongIndexCancelCallback cancel) {
    if (cache_path.empty()) {
        if (error) *error = "Failed to resolve song index cache path.";
        return false;
    }
    if (is_cancelled(cancel)) {
        if (error) *error = "Song index save canceled.";
        return false;
    }
    static std::atomic<unsigned long long> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto target = util::path_from_utf8_lossy(cache_path);
    auto temporary = target;
    temporary += ".aggregate-" + std::to_string(stamp) + "-" +
                 std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp";
    struct RemoveTemporary {
        fs::path path;
        ~RemoveTemporary() { std::error_code ignored; fs::remove(path, ignored); }
    } cleanup{temporary};
    if (!save_song_index(util::path_to_utf8_lossy(temporary), index, options, error, std::move(progress), cancel)) {
        return false;
    }
    if (is_cancelled(cancel)) {
        if (error) *error = "Song index save canceled.";
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        if (error) *error = "Failed to replace song index cache: " + std::to_string(GetLastError());
        return false;
    }
#else
    std::error_code ec;
    fs::rename(temporary, target, ec);
    if (ec) {
        if (error) *error = "Failed to replace song index cache: " + ec.message();
        return false;
    }
#endif
    return true;
}

SongSourceAggregateResult aggregate_song_sources(const std::vector<std::string>& source_roots,
                                                const std::string& profile_root,
                                                const SongIndexOptions& options,
                                                SongSourceAggregateProgressCallback progress,
                                                SongIndexCancelCallback cancel,
                                                bool force_rescan) {
    SongSourceAggregateResult result;
    std::vector<fs::path> roots;
    roots.reserve(source_roots.size());
    std::unordered_set<std::string> seen_roots;
    const auto finish_cancelled = [&]() {
        result.cancelled = true;
        result.index.entries.clear();
        result.source_counts.clear();
        return std::move(result);
    };
    for (const auto& raw : source_roots) {
        if (is_cancelled(cancel)) return finish_cancelled();
        if (raw.empty()) continue;
        try {
            auto root = canonical_absolute(util::path_from_utf8_lossy(raw));
            if (root.empty()) { result.warnings.push_back("Failed to resolve song source: " + raw); continue; }
            const auto key = menu_songs::normalize_path_key(root);
            if (seen_roots.insert(key).second) roots.push_back(std::move(root));
        } catch (const std::exception& e) {
            result.warnings.push_back("Failed to resolve song source: " + raw + ": " + e.what());
        }
    }

    std::unordered_set<std::string> seen_charts;
    for (std::size_t root_index = 0; root_index < roots.size(); ++root_index) {
        if (is_cancelled(cancel)) return finish_cancelled();
        const auto& root = roots[root_index];
        const auto source = util::path_to_utf8_lossy(root);
        const auto source_key = menu_songs::normalize_path_key(root);
        result.source_counts[source_key] = 0;
        const auto report = [&](const SongIndexProgress& update) {
            if (progress) progress({update, count_as_int(root_index + 1), count_as_int(roots.size())});
        };
        report({SongIndexProgressStage::ScanningFiles, 0, -1});
        std::error_code ec;
        if (!fs::is_directory(root, ec)) {
            ++result.missing_sources;
            result.warnings.push_back("Songs path not found: " + source);
            continue;
        }
        try {
            const auto cache_path = song_index_cache_path_for_source(profile_root, source);
            auto loaded = load_source_cache(cache_path, legacy_song_index_cache_path_for_source(source),
                                            options, result.warnings, source);
            if (is_cancelled(cancel)) return finish_cancelled();
            SongIndex index;
            if (!force_rescan && loaded.success() && loaded.loaded_from_file) {
                index = std::move(loaded.index);
                ++result.cached_sources;
            } else {
                std::vector<std::string> warnings;
                auto scan_options = options;
                if (force_rescan) scan_options.reuse_cached_metadata = false;
                index = scan_songs(source, loaded.loaded_from_file ? &loaded.index : nullptr,
                                   warnings, report, scan_options, cancel);
                append_warnings(result.warnings, warnings, source);
                if (is_cancelled(cancel)) return finish_cancelled();
                ec.clear();
                if (!fs::is_directory(root, ec)) {
                    ++result.missing_sources;
                    result.warnings.push_back("Songs path disappeared: " + source);
                    continue;
                }
                ++result.scanned_sources;
                // Release the old metadata before serializing or accumulating
                // another root. At most one source scan is live at a time.
                loaded.index = {};
                std::string save_error;
                if (!save_song_source_cache_atomically(cache_path, index, options, &save_error, report, cancel)) {
                    if (is_cancelled(cancel)) return finish_cancelled();
                    result.warnings.push_back(source + ": " + save_error);
                }
            }
            if (is_cancelled(cancel)) return finish_cancelled();
            // A folder can disappear while its cache loads or while scanning.
            // Do not resurrect its cached charts as a successful source.
            ec.clear();
            if (!fs::is_directory(root, ec)) {
                ++result.missing_sources;
                result.warnings.push_back("Songs path disappeared: " + source);
                continue;
            }
            std::unordered_set<std::string> source_charts;
            source_charts.reserve(index.entries.size());
            for (auto& entry : index.entries) {
                if (is_cancelled(cancel)) return finish_cancelled();
                if (entry.path.empty()) continue;
                const auto path = util::path_from_utf8_lossy(entry.path);
                const auto absolute = canonical_absolute(path.is_absolute() ? path : root / path);
                if (absolute.empty()) continue;
                const auto key = chart_key(absolute);
                if (!source_charts.insert(key).second) continue;
                if (!seen_charts.insert(key).second) continue;
                entry.path = util::path_to_utf8_lossy(absolute);
                entry.background_preview_path = absolute_preview(entry.background_preview_path, root);
                entry.audio_preview_path = absolute_preview(entry.audio_preview_path, root);
                result.index.entries.push_back(std::move(entry));
            }
            result.source_counts[source_key] = count_as_int(source_charts.size());
            report({SongIndexProgressStage::BuildingMetadata, count_as_int(index.entries.size()),
                    count_as_int(index.entries.size())});
        } catch (const std::exception& e) {
            result.warnings.push_back("Song source aggregation failed: " + source + ": " + e.what());
        }
    }
    if (is_cancelled(cancel)) return finish_cancelled();
    // Each per-source index is already sorted. MenuApp owns final list sorting;
    // preserve source order here so duplicate charts have a stable first winner.
    return result;
}

}  // namespace tenriff::app
