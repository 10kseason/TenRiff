#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace tenriff::app {

[[nodiscard]] inline std::string_view trim_chat_text(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] inline bool is_now_playing_chat_command(std::string_view text) {
    text = trim_chat_text(text);
    if (text.size() != 3 || text.front() != '/') return false;
    return std::tolower(static_cast<unsigned char>(text[1])) == 'n' &&
           std::tolower(static_cast<unsigned char>(text[2])) == 'p';
}

[[nodiscard]] inline std::optional<std::string> first_chat_web_url(
    std::string_view text) {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char byte) {
        return static_cast<char>(std::tolower(byte));
    });
    const auto http = lower.find("http://");
    const auto https = lower.find("https://");
    std::size_t begin = std::string::npos;
    if (http != std::string::npos) begin = http;
    if (https != std::string::npos) begin = std::min(begin, https);
    if (begin == std::string::npos) return std::nullopt;

    std::size_t end = begin;
    while (end < text.size() && end - begin < 2048) {
        const unsigned char byte = static_cast<unsigned char>(text[end]);
        if (byte <= 0x20u || byte == 0x7fu) break;
        ++end;
    }
    while (end > begin) {
        const char tail = text[end - 1];
        if (tail != '.' && tail != ',' && tail != '!' && tail != '?' &&
            tail != ';' && tail != ':' && tail != ')' && tail != ']' &&
            tail != '}' && tail != '>') {
            break;
        }
        --end;
    }
    const std::size_t scheme_size = lower.compare(begin, 8, "https://") == 0 ? 8u : 7u;
    if (end <= begin + scheme_size) return std::nullopt;
    return std::string(text.substr(begin, end - begin));
}

}  // namespace tenriff::app
