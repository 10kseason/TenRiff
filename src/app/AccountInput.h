#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "util/Utf8Compat.h"

namespace tenriff::app {

inline constexpr std::size_t kAccountPasswordMaxBytes = 128;

[[nodiscard]] inline std::string sanitize_pasted_account_password(
    std::string_view value) {
    const std::string utf8 = util::ensure_utf8_text(value);
    std::string output;
    output.reserve((std::min)(utf8.size(), kAccountPasswordMaxBytes));
    std::size_t cursor = 0;
    while (cursor < utf8.size()) {
        const unsigned char lead = static_cast<unsigned char>(utf8[cursor]);
        std::size_t length = 1;
        if ((lead & 0x80u) == 0u) length = 1;
        else if ((lead & 0xE0u) == 0xC0u) length = 2;
        else if ((lead & 0xF0u) == 0xE0u) length = 3;
        else if ((lead & 0xF8u) == 0xF0u) length = 4;
        if (cursor + length > utf8.size() ||
            output.size() + length > kAccountPasswordMaxBytes) {
            break;
        }
        if (length == 1 && (lead < 0x20u || lead == 0x7fu)) {
            ++cursor;
            continue;
        }
        output.append(utf8, cursor, length);
        cursor += length;
    }
    return output;
}

}  // namespace tenriff::app
