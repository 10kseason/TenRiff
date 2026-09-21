#pragma once

#include <array>
#include <string_view>

namespace tenriff::config {

struct BuiltinDifficultyTable {
    std::string_view name_en;
    std::string_view name_ko;
    std::string_view url;
    int function_key;
};

// Keep publisher page URLs where provided: the importer resolves their BMSTable
// header marker, so the publisher can relocate the header without a client patch.
// F4 remains Native LV; append presets without changing existing indices/keys.
inline constexpr std::array<BuiltinDifficultyTable, 8> kBuiltinDifficultyTables{{
    {"5K Aery", u8"5키 에리", "https://asumatoki.kr/table/aery/header.json", 1},
    {"7K Aery", u8"7키 에리", "https://asumatoki.kr/table/aery7/header.json", 2},
    {"10K Revive", u8"10키 리바이브", "https://calc.10k-revive.cloud/table.html", 3},
    {"Stella", u8"스텔라", "https://stellabms.xyz/st/table.html", 5},
    {"Satellite", u8"새틀라이트", "https://stellabms.xyz/sl/table.html", 6},
    {"4K U_E Pack", u8"4키 U_E 팩", "https://classmaterma.github.io/4UE/table.html", 7},
    {"6K U_E Pack", u8"6키 U_E 팩", "https://classmaterma.github.io/UE/table.html", 8},
    {"8K U_E Pack", u8"8키 U_E 팩", "https://classmaterma.github.io/8UE/table.html", 9},
}};

inline const BuiltinDifficultyTable* builtin_difficulty_table(std::string_view url) {
    for (const auto& table : kBuiltinDifficultyTables) {
        if (table.url == url) return &table;
    }
    return nullptr;
}

} // namespace tenriff::config
