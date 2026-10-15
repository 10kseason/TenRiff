#pragma once

#include <cstdint>
#include <string_view>

namespace tenriff::render {

struct DifficultyLabelPrefix {
    int key_count = 0;
    std::uint32_t length = 0;
};

// The caller supplies a mode only after checking the registered table identity.
// Numeric difficulty values are never interpreted as a key-count marker.
[[nodiscard]] inline DifficultyLabelPrefix difficulty_label_prefix(
    std::wstring_view label, bool aery_five_key, int table_prefix_key_count) {
    if (aery_five_key && label.substr(0, 1) == L"\u2464") return {5, 1};
    switch (table_prefix_key_count) {
    case 4: if (label.substr(0, 3) == L"4UE") return {4, 3}; break;
    case 6: if (label.substr(0, 3) == L"6UE") return {6, 3}; break;
    case 8: if (label.substr(0, 3) == L"8UE") return {8, 3}; break;
    case 10: if (label.substr(0, 1) == L"\u24c7") return {10, 1}; break;
    default: break;
    }
    return {};
}

} // namespace tenriff::render
