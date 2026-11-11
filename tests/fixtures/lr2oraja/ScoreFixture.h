#pragma once

#include <charconv>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace tenriff::test {

inline std::string read_lr2oraja_score_fixture() {
    std::ifstream stream(std::filesystem::u8path(TENRIFF_SOURCE_DIR) /
                         "tests/fixtures/lr2oraja/score_fixture.hex");
    std::string bytes;
    std::string line;
    while (stream >> line) {
        if (line.size() % 2 != 0) return {};
        for (std::size_t i = 0; i < line.size(); i += 2) {
            unsigned value = 0;
            const auto end = line.data() + i + 2;
            const auto parsed = std::from_chars(end - 2, end, value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != end) return {};
            bytes.push_back(static_cast<char>(value));
        }
    }
    return bytes;
}

}  // namespace tenriff::test
