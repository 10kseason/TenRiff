#pragma once

#include <array>
#include <string>
#include <string_view>

namespace tenriff::ui {

// One pass over authored UI strings avoids cascading replacements (F1 -> F2 -> Q).
// Do not apply this to song titles, paths, user text or already formatted labels.
inline std::string function_key_labels(std::string_view text,
                                      const std::array<std::string, 11>& keys) {
    const auto word = [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') || ch == '_';
    };
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == 'F' && (i == 0 || !word(text[i - 1]))) {
            std::size_t end = i + 1;
            int number = 0;
            while (end < text.size() && text[end] >= '0' && text[end] <= '9' && end - i < 3)
                number = number * 10 + text[end++] - '0';
            if (number >= 1 && number <= 11 && (end == text.size() || !word(text[end]))) {
                result += keys[number - 1].empty() ? "--" : keys[number - 1];
                i = end;
                continue;
            }
        }
        result += text[i++];
    }
    return result;
}

} // namespace tenriff::ui
