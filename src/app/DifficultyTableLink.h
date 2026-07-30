#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tenriff::app {

struct DifficultyTableHttpResponse {
    int status_code = 0;
    std::string final_url;
    std::string body;
    std::string error;

    [[nodiscard]] bool success() const noexcept {
        return error.empty() && status_code >= 200 && status_code < 300;
    }
};

using DifficultyTableFetchFunction =
    std::function<DifficultyTableHttpResponse(std::string_view url, std::size_t max_bytes)>;

struct DifficultyTableLinkImportResult {
    std::string cached_header_path;
    std::string source_url;
    std::string table_name;
    std::vector<std::string> warnings;
    std::string error;

    [[nodiscard]] bool success() const noexcept { return error.empty(); }
};

// Reads the standard BMSTable page marker:
// <meta name="bmstable" content="header.json">
[[nodiscard]] std::optional<std::string> extract_bmstable_header_url(std::string_view html);

// Resolves an HTTP(S) reference against an HTTP(S) base URL. Fragments are
// discarded because they do not participate in document fetching.
[[nodiscard]] std::optional<std::string> resolve_difficulty_table_url(
    std::string_view base_url,
    std::string_view reference);

// Imports either a standard BMSTable HTML page URL or a direct header JSON URL.
// Remote documents are copied into an app-owned local cache, then validated by
// the existing local-only difficulty-table loader.
[[nodiscard]] DifficultyTableLinkImportResult import_difficulty_table_link(
    std::string_view source_url,
    const std::filesystem::path& cache_root,
    DifficultyTableFetchFunction fetch = {});

}  // namespace tenriff::app
