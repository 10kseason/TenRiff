#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::app::osu_assets {

// osu! beatmap resources are package-local. Reject path forms that could make a
// chart name a file outside its own beatmap directory before applying any
// filename or extension fallback.
inline bool is_safe_relative_reference(std::string_view reference) {
    if (reference.empty()) {
        return false;
    }

    std::string portable(reference);
    std::replace(portable.begin(), portable.end(), '\\', '/');
    if (portable.empty() || portable.front() == '/') {
        return false;
    }
    if (portable.size() >= 2u &&
        std::isalpha(static_cast<unsigned char>(portable[0])) != 0 &&
        portable[1] == ':') {
        return false;
    }

    std::size_t component_begin = 0;
    while (component_begin <= portable.size()) {
        const std::size_t slash = portable.find('/', component_begin);
        const std::size_t component_end =
            slash == std::string::npos ? portable.size() : slash;
        const std::string_view component(portable.data() + component_begin,
                                         component_end - component_begin);
        if (component == "..") {
            return false;
        }
        if (slash == std::string::npos) {
            break;
        }
        component_begin = slash + 1u;
    }

    try {
        const std::filesystem::path path = std::filesystem::u8path(reference);
        return !path.is_absolute() && !path.has_root_name() && !path.has_root_directory();
    } catch (...) {
        return false;
    }
}

inline std::optional<std::filesystem::path> canonical_chart_root(
    const std::filesystem::path& chart_path) {
    namespace fs = std::filesystem;
    try {
        std::error_code ec;
        fs::path absolute_chart = chart_path;
        if (!absolute_chart.is_absolute()) {
            absolute_chart = fs::absolute(absolute_chart, ec);
            if (ec || absolute_chart.empty()) {
                return std::nullopt;
            }
        }

        const fs::path root = fs::canonical(absolute_chart.parent_path(), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec) {
            return std::nullopt;
        }
        return root;
    } catch (...) {
        return std::nullopt;
    }
}

inline bool path_is_within_root(const std::filesystem::path& canonical_path,
                                const std::filesystem::path& canonical_root) {
    try {
        const std::filesystem::path relative = canonical_path.lexically_relative(canonical_root);
        if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
            relative.has_root_directory()) {
            return false;
        }
        for (const auto& component : relative) {
            if (component == "..") {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

inline std::optional<std::filesystem::path> canonical_existing_file_in_chart_root(
    const std::filesystem::path& chart_path,
    const std::filesystem::path& candidate_path) {
    namespace fs = std::filesystem;
    try {
        const auto root = canonical_chart_root(chart_path);
        if (!root.has_value()) {
            return std::nullopt;
        }

        std::error_code ec;
        const fs::path canonical_candidate = fs::canonical(candidate_path, ec);
        if (ec || canonical_candidate.empty() || !fs::is_regular_file(canonical_candidate, ec) || ec) {
            return std::nullopt;
        }
        if (!path_is_within_root(canonical_candidate, *root)) {
            return std::nullopt;
        }
        return canonical_candidate;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace tenriff::app::osu_assets
