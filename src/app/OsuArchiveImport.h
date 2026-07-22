#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace tenriff::app {

enum class OsuArchiveKind {
    Osk,
    Osz,
};

struct OsuArchiveImportLimits {
    std::uint64_t max_archive_bytes = 2ull * 1024ull * 1024ull * 1024ull;
    std::uint64_t max_file_bytes = 512ull * 1024ull * 1024ull;
    std::uint64_t max_total_uncompressed_bytes = 4ull * 1024ull * 1024ull * 1024ull;
    std::size_t max_entries = 16384;
    std::size_t max_path_bytes = 1024;
    std::size_t max_path_components = 64;
    double max_compression_ratio = 500.0;
};

struct OsuArchiveImportResult {
    bool success = false;
    std::string installed_path;
    std::string display_name;
    std::string error;
    std::size_t archive_entry_count = 0;
    std::size_t extracted_file_count = 0;
    std::uint64_t extracted_bytes = 0;
};

// Imports one osu! archive without modifying the source archive. All archive
// entries are validated before extraction begins, and extraction is committed
// by renaming a private staging directory inside destination_root.
[[nodiscard]] OsuArchiveImportResult import_osu_archive(
    std::string_view archive_path_utf8,
    std::string_view destination_root_utf8,
    OsuArchiveKind kind,
    const OsuArchiveImportLimits& limits = {});

}  // namespace tenriff::app
