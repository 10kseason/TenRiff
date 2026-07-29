#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "app/ChartFileHash.h"

namespace tenriff::app {

struct DifficultyTableMatch {
    std::string name;
    std::string symbol;
    std::string level;
    int order = -1;

    [[nodiscard]] std::string label() const { return symbol + level; }
};

struct DifficultyTableLoadOptions {
    // A standard table-data JSON file is only an array and carries no table
    // identity. These fields are therefore required when loading an array
    // directly. A standard local header object supplies them itself.
    std::string name;
    std::string symbol;
    std::vector<std::string> level_order;
};

struct DifficultyTableLoadResult;

class DifficultyTable {
public:
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }
    [[nodiscard]] std::size_t entry_count() const noexcept { return entries_.size(); }

    // SHA-256 is preferred when both supplied digests point at table entries.
    [[nodiscard]] std::optional<DifficultyTableMatch> lookup(const ChartFileHashes& hashes) const;
    [[nodiscard]] std::optional<DifficultyTableMatch> lookup(std::string_view md5,
                                                              std::string_view sha256) const;

private:
    struct Entry {
        std::string level;
        int order = -1;
    };

    std::string name_;
    std::string symbol_;
    std::vector<Entry> entries_;
    std::unordered_map<std::string, std::size_t> by_md5_;
    std::unordered_map<std::string, std::size_t> by_sha256_;

    friend struct DifficultyTableLoadResult;
    friend DifficultyTableLoadResult load_difficulty_table(
        const std::filesystem::path&, const DifficultyTableLoadOptions&);
    friend DifficultyTableLoadResult load_difficulty_table_utf8(
        std::string_view, const DifficultyTableLoadOptions&);
};

struct DifficultyTableLoadResult {
    DifficultyTable table;
    std::vector<std::string> warnings;
    std::string error;

    [[nodiscard]] bool success() const noexcept { return error.empty(); }
};

// The selected file may be either:
// - a standard table data array, using identity supplied in options; or
// - a local standard header object whose data_url is a relative filesystem path.
// Network URLs and absolute data_url values are intentionally not fetched.
[[nodiscard]] DifficultyTableLoadResult load_difficulty_table(
    const std::filesystem::path& path,
    const DifficultyTableLoadOptions& options = {});
[[nodiscard]] DifficultyTableLoadResult load_difficulty_table_utf8(
    std::string_view path,
    const DifficultyTableLoadOptions& options = {});

}  // namespace tenriff::app
