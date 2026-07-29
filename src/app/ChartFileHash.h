#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace tenriff::app {

// Standard BMS difficulty tables identify charts by the digest of the exact
// chart-file bytes. Both digests are calculated in one streaming file pass.
struct ChartFileHashes {
    std::string md5;
    std::string sha256;
    std::uint64_t size = 0;

    [[nodiscard]] bool valid() const noexcept {
        return md5.size() == 32 && sha256.size() == 64;
    }
};

[[nodiscard]] ChartFileHashes hash_chart_file(const std::filesystem::path& path,
                                              std::string* error = nullptr);
[[nodiscard]] ChartFileHashes hash_chart_file_utf8(std::string_view path,
                                                   std::string* error = nullptr);

}  // namespace tenriff::app
