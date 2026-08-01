#include "app/MenuSongUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chart/BmsParser.h"
#include "chart/OsuManiaLoader.h"
#include "util/Utf8Compat.h"

namespace tenriff::app::menu_songs {

namespace {

std::filesystem::path path_from_utf8(std::string_view value) {
    try {
        return util::path_from_utf8_lossy(value);
    } catch (...) {
        return {};
    }
}

std::string safe_ui_text(std::string_view value, std::string_view fallback = {}) {
    std::string cleaned = util::sanitize_ui_text(value);
    if (!cleaned.empty()) {
        return cleaned;
    }
    return std::string(fallback);
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string to_upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= static_cast<unsigned char>('a') && ch <= static_cast<unsigned char>('z')) {
            return static_cast<char>(ch - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string trim_copy(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string normalize_asset_reference(std::string value) {
    value = trim_copy(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = trim_copy(std::string_view(value).substr(1, value.size() - 2));
        }
    }
    return value;
}

bool is_preview_image_extension(std::string_view ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
           ext == ".gif" || ext == ".tif" || ext == ".tiff" || ext == ".webp";
}

std::string normalize_asset_lookup_key(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return to_lower_ascii(std::move(value));
}

std::filesystem::path normalize_resolved_preview_path(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path normalized = path;
    if (!normalized.is_absolute()) {
        const fs::path absolute = fs::absolute(normalized, ec);
        if (!ec && !absolute.empty()) {
            normalized = absolute;
        } else {
            ec.clear();
        }
    }

    const fs::path canonical = fs::weakly_canonical(normalized, ec);
    if (!ec && !canonical.empty()) {
        return canonical;
    }
    return normalized.lexically_normal();
}

struct PreviewAssetLookupIndex {
    bool built = false;
    std::unordered_map<std::string, std::filesystem::path> by_relative;
    std::unordered_map<std::string, std::filesystem::path> by_filename;
};

void build_preview_asset_lookup(const std::filesystem::path& chart_path,
                                 PreviewAssetLookupIndex& lookup) {
    if (lookup.built) {
        return;
    }
    lookup.built = true;
    lookup.by_relative.clear();
    lookup.by_filename.clear();

    namespace fs = std::filesystem;
    const fs::path root = chart_path.parent_path();
    if (root.empty()) {
        return;
    }

    std::error_code ec;
    fs::directory_options options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root, options, ec);
    fs::recursive_directory_iterator end;
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        const std::string ext = to_lower_ascii(entry.path().extension().u8string());
        if (!is_preview_image_extension(ext)) {
            it.increment(ec);
            continue;
        }

        const fs::path full = normalize_resolved_preview_path(entry.path());
        fs::path relative = fs::relative(full, root, ec);
        if (ec || relative.empty()) {
            ec.clear();
            relative = full.lexically_relative(root);
        }
        if (!relative.empty()) {
            lookup.by_relative.emplace(normalize_asset_lookup_key(relative.generic_u8string()), full);
        }
        lookup.by_filename.emplace(normalize_asset_lookup_key(full.filename().u8string()), full);
        it.increment(ec);
    }
}

std::optional<std::filesystem::path> lookup_preview_asset_candidate(const std::filesystem::path& chart_path,
                                                                     const std::filesystem::path& ref_path,
                                                                     PreviewAssetLookupIndex& lookup) {
    namespace fs = std::filesystem;
    const fs::path direct = ref_path.is_absolute()
                                ? ref_path.lexically_normal()
                                : (chart_path.parent_path() / ref_path).lexically_normal();
    std::error_code ec;
    if (!direct.empty() && fs::exists(direct, ec) && !ec &&
        is_preview_image_extension(to_lower_ascii(direct.extension().u8string()))) {
        return normalize_resolved_preview_path(direct);
    }

    build_preview_asset_lookup(chart_path, lookup);

    const std::string relative_key = normalize_asset_lookup_key(ref_path.generic_u8string());
    auto relative_it = lookup.by_relative.find(relative_key);
    if (relative_it != lookup.by_relative.end()) {
        return relative_it->second;
    }

    const std::string file_key = normalize_asset_lookup_key(ref_path.filename().u8string());
    auto file_it = lookup.by_filename.find(file_key);
    if (file_it != lookup.by_filename.end()) {
        return file_it->second;
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> build_preview_reference_candidates(const std::string& reference) {
    namespace fs = std::filesystem;
    static constexpr std::string_view kPreviewExts[] = {
        ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff", ".webp",
    };

    const std::string normalized = normalize_asset_reference(reference);
#ifdef _WIN32
    fs::path ref_path = fs::u8path(normalized);
#else
    fs::path ref_path(normalized);
#endif

    std::vector<fs::path> candidates;
    std::unordered_set<std::string> seen;
    auto push_candidate = [&](const fs::path& candidate) {
        const std::string key = normalize_asset_lookup_key(candidate.generic_u8string());
        if (seen.emplace(key).second) {
            candidates.push_back(candidate);
        }
    };

    push_candidate(ref_path);

    const std::string ext = to_lower_ascii(ref_path.extension().u8string());
    if (ext.empty()) {
        for (std::string_view preview_ext : kPreviewExts) {
            fs::path candidate = ref_path;
            candidate += preview_ext;
            push_candidate(candidate);
        }
    } else if (is_preview_image_extension(ext)) {
        for (std::string_view preview_ext : kPreviewExts) {
            if (preview_ext == ext) {
                continue;
            }
            fs::path candidate = ref_path;
            candidate.replace_extension(preview_ext);
            push_candidate(candidate);
        }
    }

    return candidates;
}

std::optional<std::filesystem::path> resolve_preview_asset_path(const std::filesystem::path& chart_path,
                                                                 const std::string& reference) {
    const std::string normalized = normalize_asset_reference(reference);
    PreviewAssetLookupIndex lookup;
    for (const auto& candidate : build_preview_reference_candidates(normalized)) {
        if (auto resolved = lookup_preview_asset_candidate(chart_path, candidate, lookup);
            resolved.has_value()) {
            return resolved;
        }
    }
    return std::nullopt;
}

std::vector<std::string> collect_bms_preview_references(const chart::BmsChart& chart) {
    std::vector<std::string> references;
    std::unordered_set<std::string> seen;
    auto append_reference = [&](std::string value) {
        value = normalize_asset_reference(std::move(value));
        if (value.empty()) {
            return;
        }
        const std::string key = normalize_asset_lookup_key(value);
        if (seen.emplace(key).second) {
            references.push_back(std::move(value));
        }
    };

    for (std::string_view header_key : {"STAGEFILE", "BACKBMP"}) {
        auto header_it = chart.headers.find(std::string(header_key));
        if (header_it != chart.headers.end()) {
            append_reference(header_it->second);
        }
    }

    auto append_bga_references = [&](std::string_view channel) {
        for (const auto& command : chart.commands) {
            if (command.channel != channel) {
                continue;
            }
            for (std::size_t i = 0; i + 1 < command.data.size(); i += 2) {
                std::string slot = command.data.substr(i, 2);
                if (slot == "00") {
                    continue;
                }
                slot = to_upper_ascii(std::move(slot));
                auto bmp_it = chart.bmp.find(slot);
                if (bmp_it == chart.bmp.end()) {
                    continue;
                }
                append_reference(bmp_it->second);
            }
        }
    };

    for (std::string_view channel : {"04", "07", "06"}) {
        append_bga_references(channel);
    }
    return references;
}

bool is_preview_audio_extension(std::string_view ext) {
    return ext == ".ogg" || ext == ".wav" || ext == ".wave" || ext == ".mp3";
}

std::vector<std::filesystem::path> build_audio_reference_candidates(const std::string& reference) {
    namespace fs = std::filesystem;
    static constexpr std::string_view kAudioExtensions[] = {".ogg", ".wav", ".wave", ".mp3"};

    const std::string normalized = normalize_asset_reference(reference);
    if (normalized.empty()) {
        return {};
    }
#ifdef _WIN32
    fs::path reference_path = fs::u8path(normalized);
#else
    fs::path reference_path(normalized);
#endif

    std::vector<fs::path> candidates;
    std::unordered_set<std::string> seen;
    const auto append = [&](const fs::path& candidate, auto& self) -> void {
        const std::string key = normalize_asset_lookup_key(candidate.generic_u8string());
        if (seen.emplace(key).second) {
            candidates.push_back(candidate);
        }
        (void)self;
    };

    append(reference_path, append);
    const std::string extension = to_lower_ascii(reference_path.extension().u8string());
    if (extension.empty()) {
        for (const std::string_view audio_extension : kAudioExtensions) {
            fs::path candidate = reference_path;
            candidate += audio_extension;
            append(candidate, append);
        }
    } else if (is_preview_audio_extension(extension)) {
        for (const std::string_view audio_extension : kAudioExtensions) {
            if (audio_extension == extension) {
                continue;
            }
            fs::path candidate = reference_path;
            candidate.replace_extension(audio_extension);
            append(candidate, append);
        }
    }
    return candidates;
}

std::optional<std::filesystem::path> resolve_audio_reference_path(
    const std::filesystem::path& chart_path,
    const std::string& reference) {
    namespace fs = std::filesystem;
    const fs::path root = chart_path.parent_path();
    if (root.empty()) {
        return std::nullopt;
    }

    const auto candidates = build_audio_reference_candidates(reference);
    for (const auto& candidate : candidates) {
        const fs::path direct = candidate.is_absolute()
                                    ? candidate.lexically_normal()
                                    : (root / candidate).lexically_normal();
        std::error_code ec;
        if (fs::is_regular_file(direct, ec) && !ec &&
            is_preview_audio_extension(to_lower_ascii(direct.extension().u8string()))) {
            return normalize_resolved_preview_path(direct);
        }
    }

    std::unordered_set<std::string> relative_keys;
    std::unordered_set<std::string> filename_keys;
    for (const auto& candidate : candidates) {
        relative_keys.emplace(normalize_asset_lookup_key(candidate.generic_u8string()));
        filename_keys.emplace(normalize_asset_lookup_key(candidate.filename().u8string()));
    }

    std::error_code ec;
    fs::recursive_directory_iterator iterator(
        root, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    while (iterator != end) {
        if (ec) {
            ec.clear();
            iterator.increment(ec);
            continue;
        }

        const fs::directory_entry entry = *iterator;
        iterator.increment(ec);
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        ec.clear();

        const fs::path candidate_path = entry.path();
        if (!is_preview_audio_extension(
                to_lower_ascii(candidate_path.extension().u8string()))) {
            continue;
        }

        fs::path relative = fs::relative(candidate_path, root, ec);
        if (ec || relative.empty()) {
            ec.clear();
            relative = candidate_path.lexically_relative(root);
        }
        const std::string relative_key =
            normalize_asset_lookup_key(relative.generic_u8string());
        const std::string filename_key =
            normalize_asset_lookup_key(candidate_path.filename().u8string());
        if (relative_keys.find(relative_key) != relative_keys.end() ||
            filename_keys.find(filename_key) != filename_keys.end()) {
            return normalize_resolved_preview_path(candidate_path);
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> largest_local_audio_file(
    const std::filesystem::path& chart_path) {
    namespace fs = std::filesystem;
    const fs::path root = chart_path.parent_path();
    if (root.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    fs::directory_iterator iterator(
        root, fs::directory_options::skip_permission_denied, ec);
    const fs::directory_iterator end;
    fs::path best_path;
    std::uintmax_t best_size = 0;

    while (iterator != end) {
        if (ec) {
            ec.clear();
            iterator.increment(ec);
            continue;
        }

        const fs::directory_entry entry = *iterator;
        iterator.increment(ec);
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        ec.clear();
        if (!is_preview_audio_extension(
                to_lower_ascii(entry.path().extension().u8string()))) {
            continue;
        }

        const std::uintmax_t size = entry.file_size(ec);
        if (ec) {
            ec.clear();
            continue;
        }
        const std::string candidate_key =
            normalize_asset_lookup_key(entry.path().generic_u8string());
        const std::string best_key =
            normalize_asset_lookup_key(best_path.generic_u8string());
        if (best_path.empty() || size > best_size ||
            (size == best_size && candidate_key < best_key)) {
            best_path = entry.path();
            best_size = size;
        }
    }

    if (best_path.empty()) {
        return std::nullopt;
    }
    return normalize_resolved_preview_path(best_path);
}

void append_unique_key(std::vector<std::string>& keys, const std::string& key) {
    if (key.empty()) {
        return;
    }
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

bool relative_points_outside_root(const std::filesystem::path& relative) {
    if (relative.empty()) {
        return true;
    }
    const auto first = relative.begin();
    if (first == relative.end()) {
        return true;
    }
    return *first == "..";
}

}  // namespace

bool is_bms_chart_extension(std::string_view ext) {
    return ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".pms";
}

bool is_supported_chart_extension(std::string_view ext) {
    return is_bms_chart_extension(ext) || ext == ".osu";
}

std::optional<std::string> normalize_dropped_song_source(const std::string& raw_path) {
    namespace fs = std::filesystem;
    if (raw_path.empty()) {
        return std::nullopt;
    }

    fs::path candidate = path_from_utf8(raw_path);
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(candidate, ec);
    if (!ec && !canonical.empty()) {
        candidate = canonical;
    } else {
        ec.clear();
        candidate = candidate.lexically_normal();
    }

    if (fs::is_directory(candidate, ec)) {
        return candidate.u8string();
    }
    ec.clear();

    if (fs::is_regular_file(candidate, ec) &&
        is_supported_chart_extension(to_lower_ascii(candidate.extension().u8string()))) {
        const fs::path parent = candidate.parent_path();
        if (!parent.empty()) {
            return parent.u8string();
        }
    }

    return std::nullopt;
}

std::string normalize_song_source_path(const std::string& raw_path) {
    namespace fs = std::filesystem;
    if (raw_path.empty()) {
        return {};
    }

    fs::path candidate = path_from_utf8(raw_path);
    std::error_code ec;
    if (!candidate.is_absolute()) {
        const fs::path absolute = fs::absolute(candidate, ec);
        if (!ec && !absolute.empty()) {
            candidate = absolute;
        } else {
            ec.clear();
        }
    }

    const fs::path canonical = fs::weakly_canonical(candidate, ec);
    if (!ec && !canonical.empty()) {
        candidate = canonical;
    } else {
        ec.clear();
        candidate = candidate.lexically_normal();
    }
    return candidate.u8string();
}

std::string song_source_display_name(const std::string& raw_path) {
    namespace fs = std::filesystem;
    if (raw_path.empty()) {
        return "Songs";
    }

    fs::path source_path = path_from_utf8(raw_path);
    try {
        source_path = source_path.lexically_normal();
        if (!source_path.filename().empty()) {
            return safe_ui_text(source_path.filename().u8string(), "Songs");
        }
        if (!source_path.root_name().empty()) {
            return safe_ui_text(source_path.root_name().u8string(), "Songs");
        }
        return safe_ui_text(source_path.u8string(), "Songs");
    } catch (...) {
        return "Songs";
    }
}

std::string normalize_path_key(const std::filesystem::path& raw_path) {
    if (raw_path.empty()) {
        return {};
    }
    try {
        std::error_code ec;
        std::filesystem::path normalized = raw_path;
        if (normalized.is_absolute()) {
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(normalized, ec);
            if (!ec && !canonical.empty()) {
                normalized = canonical;
            } else {
                ec.clear();
                normalized = normalized.lexically_normal();
            }
        } else {
            normalized = normalized.lexically_normal();
        }
        return to_lower_ascii(normalized.generic_u8string());
    } catch (...) {
        return {};
    }
}

std::vector<std::string> build_chart_path_keys(const std::string& chart_path, const std::string& songs_root) {
    namespace fs = std::filesystem;
    std::vector<std::string> keys;
    if (chart_path.empty()) {
        return keys;
    }

    const fs::path raw = path_from_utf8(chart_path);
    if (raw.empty()) {
        return keys;
    }
    append_unique_key(keys, normalize_path_key(raw));

    if (songs_root.empty()) {
        return keys;
    }

    try {
        std::error_code ec;
        fs::path root = path_from_utf8(songs_root);
        if (root.empty()) {
            return keys;
        }
        const fs::path root_canonical = fs::weakly_canonical(root, ec);
        if (!ec && !root_canonical.empty()) {
            root = root_canonical;
        } else {
            ec.clear();
            root = root.lexically_normal();
        }

        if (raw.is_absolute()) {
            fs::path absolute = raw;
            const fs::path absolute_canonical = fs::weakly_canonical(absolute, ec);
            if (!ec && !absolute_canonical.empty()) {
                absolute = absolute_canonical;
            } else {
                ec.clear();
                absolute = absolute.lexically_normal();
            }
            const fs::path relative = absolute.lexically_relative(root);
            if (!relative_points_outside_root(relative)) {
                append_unique_key(keys, normalize_path_key(relative));
            }
        } else {
            append_unique_key(keys, normalize_path_key(root / raw));
        }
    } catch (...) {
    }

    return keys;
}

std::string resolve_osu_background_preview_path(const std::filesystem::path& chart_path,
                                                std::string_view background_filename) {
    if (chart_path.empty() || background_filename.empty()) {
        return {};
    }
    if (auto preview = resolve_preview_asset_path(chart_path, std::string(background_filename));
        preview.has_value()) {
        return preview->u8string();
    }
    return {};
}
std::string resolve_bms_background_preview_path(const std::filesystem::path& chart_path,
                                                const chart::BmsChart& chart) {
    if (chart_path.empty()) {
        return {};
    }
    for (const auto& preview_reference : collect_bms_preview_references(chart)) {
        if (auto preview = resolve_preview_asset_path(chart_path, preview_reference);
            preview.has_value()) {
            return preview->u8string();
        }
    }
    return {};
}

std::string resolve_song_background_preview_path(const std::string& chart_path) {
    if (chart_path.empty()) {
        return {};
    }

    namespace fs = std::filesystem;
    const fs::path chart_fs_path = path_from_utf8(chart_path);
    const std::string chart_ext = to_lower_ascii(chart_fs_path.extension().u8string());

    if (chart_ext == ".osu") {
        std::ifstream file(chart_fs_path, std::ios::binary);
        if (!file) {
            return {};
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        chart::OsuManiaLoader loader;
        const auto parsed = loader.parse(buffer.str());
        return resolve_osu_background_preview_path(chart_fs_path, parsed.chart.background_filename);
    }
    if (!is_bms_chart_extension(chart_ext)) {
        return {};
    }

    chart::BmsParser parser;
    chart::BmsParserOptions options;
    options.tolerant = true;
    const auto parsed = parser.parseFile(chart_path, options);
    return resolve_bms_background_preview_path(chart_fs_path, parsed.chart);
}

std::string resolve_osu_audio_preview_path(const std::filesystem::path& chart_path,
                                           std::string_view audio_filename) {
    if (chart_path.empty()) {
        return {};
    }
    if (auto resolved = resolve_audio_reference_path(chart_path, std::string(audio_filename));
        resolved.has_value()) {
        return resolved->u8string();
    }
    if (auto fallback = largest_local_audio_file(chart_path); fallback.has_value()) {
        return fallback->u8string();
    }
    return {};
}
std::string resolve_bms_audio_preview_path(const std::filesystem::path& chart_path,
                                           const chart::BmsChart& chart) {
    if (chart_path.empty()) {
        return {};
    }

    for (const std::string_view header_key : {"PREVIEW", "PREVIEWFILE"}) {
        const auto header = chart.headers.find(std::string(header_key));
        if (header == chart.headers.end()) {
            continue;
        }
        if (auto resolved = resolve_audio_reference_path(chart_path, header->second);
            resolved.has_value()) {
            return resolved->u8string();
        }
    }

    if (auto fallback = largest_local_audio_file(chart_path); fallback.has_value()) {
        return fallback->u8string();
    }
    return {};
}

std::string resolve_song_audio_preview_path(const std::string& chart_path) {
    if (chart_path.empty()) {
        return {};
    }

    const std::filesystem::path chart_fs_path = path_from_utf8(chart_path);
    const std::string chart_ext = to_lower_ascii(chart_fs_path.extension().u8string());
    if (chart_ext == ".osu") {
        std::ifstream file(chart_fs_path, std::ios::binary);
        if (!file) {
            return {};
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        chart::OsuManiaLoader loader;
        const auto parsed = loader.parse(buffer.str());
        return resolve_osu_audio_preview_path(chart_fs_path, parsed.chart.audio_filename);
    }
    if (!is_bms_chart_extension(chart_ext)) {
        return {};
    }

    chart::BmsParser parser;
    chart::BmsParserOptions options;
    options.tolerant = true;
    options.retain_unknown_headers = false;
    options.retain_nonessential_commands = false;
    const auto parsed = parser.parseFile(chart_path, options);
    return resolve_bms_audio_preview_path(chart_fs_path, parsed.chart);
}

}  // namespace tenriff::app::menu_songs
