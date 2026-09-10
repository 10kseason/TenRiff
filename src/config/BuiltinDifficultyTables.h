#pragma once

#include <array>
#include <string_view>

namespace tenriff::config {

struct BuiltinDifficultyTable {
    std::string_view name_en;
    std::string_view name_ko;
    std::string_view url;
};

// Keep the publisher's page URL for Revive: the importer resolves its BMSTable
// header marker, so the publisher can relocate the header without a client patch.
inline constexpr std::array<BuiltinDifficultyTable, 3> kBuiltinDifficultyTables{{
    {"5K Aery", u8"5키 에리", "https://asumatoki.kr/table/aery/header.json"},
    {"7K Aery", u8"7키 에리", "https://asumatoki.kr/table/aery7/header.json"},
    {"10K Revive", u8"10키 리바이브", "https://calc.10k-revive.cloud/table.html"},
}};

inline const BuiltinDifficultyTable* builtin_difficulty_table(std::string_view url) {
    for (const auto& table : kBuiltinDifficultyTables) {
        if (table.url == url) return &table;
    }
    return nullptr;
}

} // namespace tenriff::config
