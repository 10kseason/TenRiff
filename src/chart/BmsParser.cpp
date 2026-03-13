#include "chart/BmsParser.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tenriff::chart {

namespace {

std::string trim(std::string_view view) {
    std::size_t begin = 0;
    std::size_t end = view.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(view[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(view[end - 1]))) {
        --end;
    }
    return std::string(view.substr(begin, end - begin));
}

void to_upper_ascii(std::string& value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
}

double parse_double(std::string_view token, bool& ok) {
    std::string trimmed = trim(token);  // Store trimmed result in stable string
    if (trimmed.empty()) {
        ok = false;
        return 0.0;
    }
    double result = 0.0;
    std::istringstream ss(trimmed);
    ss.imbue(std::locale::classic());  // Locale-independent parsing
    ss >> result;
    // Check for complete consumption: no failure AND no remaining characters.
    ok = !ss.fail() && (ss.peek() == std::char_traits<char>::eof());
    return result;
}

bool parse_int(std::string_view token, int& out_value) {
    std::string trimmed = trim(token);  // Store in stable string
    if (trimmed.empty()) {
        return false;
    }
    int value = 0;
    auto first = trimmed.data();
    auto last = trimmed.data() + trimmed.size();
    auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    out_value = value;
    return true;
}

std::string strip_comment(std::string_view view) {
    auto pos = view.find(';');
    if (pos != std::string_view::npos) {
        view = view.substr(0, pos);
    }
    return trim(view);
}

void add_message(std::vector<BmsParseMessage>& messages, BmsParseSeverity severity, std::size_t line, std::string text) {
    messages.push_back(BmsParseMessage{severity, line, std::move(text)});
}

std::string remove_bom(std::string_view content) {
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.remove_prefix(3);
    }
    return std::string(content);
}

bool is_valid_utf8(std::string_view content) {
    std::size_t index = 0;
    while (index < content.size()) {
        const auto lead = static_cast<unsigned char>(content[index]);
        if (lead <= 0x7F) {
            ++index;
            continue;
        }

        std::size_t width = 0;
        uint32_t code_point = 0;
        if ((lead & 0xE0) == 0xC0) {
            width = 2;
            code_point = static_cast<uint32_t>(lead & 0x1F);
        } else if ((lead & 0xF0) == 0xE0) {
            width = 3;
            code_point = static_cast<uint32_t>(lead & 0x0F);
        } else if ((lead & 0xF8) == 0xF0) {
            width = 4;
            code_point = static_cast<uint32_t>(lead & 0x07);
        } else {
            return false;
        }

        if (index + width > content.size()) {
            return false;
        }

        for (std::size_t offset = 1; offset < width; ++offset) {
            const auto next = static_cast<unsigned char>(content[index + offset]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | static_cast<uint32_t>(next & 0x3F);
        }

        if ((width == 2 && code_point < 0x80) ||
            (width == 3 && code_point < 0x800) ||
            (width == 4 && (code_point < 0x10000 || code_point > 0x10FFFF)) ||
            (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            return false;
        }

        index += width;
    }
    return true;
}

#ifdef _WIN32
std::string wide_to_utf8(std::wstring_view content) {
    if (content.empty()) {
        return {};
    }
    if (content.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }

    const int wide_size = static_cast<int>(content.size());
    const int utf8_size =
        WideCharToMultiByte(CP_UTF8, 0, content.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(utf8_size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, content.data(), wide_size, utf8.data(), utf8_size, nullptr, nullptr) <= 0) {
        return {};
    }
    return utf8;
}

std::optional<std::wstring> multibyte_to_wide(UINT code_page, std::string_view content) {
    if (content.empty()) {
        return std::wstring{};
    }
    if (content.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }

    const int narrow_size = static_cast<int>(content.size());
    const int wide_size = MultiByteToWideChar(code_page, 0, content.data(), narrow_size, nullptr, 0);
    if (wide_size <= 0) {
        return std::nullopt;
    }

    std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(code_page, 0, content.data(), narrow_size, wide.data(), wide_size) <= 0) {
        return std::nullopt;
    }
    return wide;
}

std::optional<std::string> decode_utf16_bom(std::string_view content) {
    if (content.size() < 2) {
        return std::nullopt;
    }

    const auto first = static_cast<unsigned char>(content[0]);
    const auto second = static_cast<unsigned char>(content[1]);
    const bool little_endian = (first == 0xFF && second == 0xFE);
    const bool big_endian = (first == 0xFE && second == 0xFF);
    if (!little_endian && !big_endian) {
        return std::nullopt;
    }

    std::wstring wide;
    wide.reserve((content.size() - 2) / 2);
    for (std::size_t index = 2; index + 1 < content.size(); index += 2) {
        const auto high = static_cast<unsigned char>(content[index]);
        const auto low = static_cast<unsigned char>(content[index + 1]);
        const uint16_t code_unit = little_endian
                                       ? static_cast<uint16_t>(high | (low << 8))
                                       : static_cast<uint16_t>((high << 8) | low);
        wide.push_back(static_cast<wchar_t>(code_unit));
    }

    return wide_to_utf8(wide);
}
#endif

std::string normalize_bms_text(std::string_view content) {
    if (content.empty()) {
        return {};
    }

#ifdef _WIN32
    if (auto utf16 = decode_utf16_bom(content); utf16.has_value()) {
        return *utf16;
    }
#endif

    if (is_valid_utf8(content)) {
        return std::string(content);
    }

#ifdef _WIN32
    if (auto wide = multibyte_to_wide(932, content); wide.has_value()) {
        std::string utf8 = wide_to_utf8(*wide);
        if (!utf8.empty()) {
            return utf8;
        }
    }
#endif

    return std::string(content);
}

bool validate_measure_token_length(const std::string& data) {
    return (data.size() % 2) == 0;
}

bool is_measure_command_key(std::string_view key) {
    if (key.size() != 5) {
        return false;
    }
    return std::all_of(key.begin(), key.begin() + 3, [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

std::string parse_header_value(std::string_view tail) {
    std::string value = trim(tail);
    if (!value.empty() && value.front() == ':') {
        value = trim(std::string_view(value).substr(1));
    }
    return value;
}

}  // namespace

bool BmsParseResult::success() const {
    return std::none_of(messages.begin(), messages.end(), [](const BmsParseMessage& msg) {
        return msg.severity == BmsParseSeverity::Error;
    });
}

BmsParseResult BmsParser::parse(std::string_view content, const BmsParserOptions& options) const {
    BmsParseResult result;
    std::string cleaned = remove_bom(normalize_bms_text(content));

    std::istringstream stream(cleaned);
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(stream, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = strip_comment(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.front() == '*') {
            continue;
        }
        if (trimmed.front() != '#') {
            add_message(result.messages, BmsParseSeverity::Warning, line_number,
                        "Line ignored because it does not start with '#'.");
            continue;
        }

        std::string command = trim(std::string_view(trimmed).substr(1));
        auto separator_pos = command.find_first_of(" \t:");
        std::string key = separator_pos == std::string::npos ? command : command.substr(0, separator_pos);
        if (key.empty()) {
            add_message(result.messages, BmsParseSeverity::Error, line_number,
                        "Empty command key.");
            continue;
        }

        const std::string_view tail = separator_pos == std::string::npos
                                          ? std::string_view{}
                                          : std::string_view(command).substr(separator_pos);
        std::string_view measure_tail = tail;
        while (!measure_tail.empty() &&
               std::isspace(static_cast<unsigned char>(measure_tail.front())) != 0) {
            measure_tail.remove_prefix(1);
        }

        if (is_measure_command_key(key) && !measure_tail.empty() && measure_tail.front() == ':') {
            std::string measure_token = key.substr(0, 3);
            int measure_index = 0;
            if (!parse_int(measure_token, measure_index)) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Failed to parse measure index.");
                continue;
            }

            std::string channel_token = key.substr(3);
            channel_token = trim(channel_token);
            to_upper_ascii(channel_token);
            if (channel_token.size() != 2) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Channel token must have length 2.");
                continue;
            }

            measure_tail.remove_prefix(1);
            std::string data = trim(measure_tail);
            // Remove all spaces and tabs from data to ensure even-length token parsing.
            data.erase(std::remove_if(data.begin(), data.end(), [](unsigned char ch) {
                          return ch == ' ' || ch == '\t';
                      }),
                      data.end());

            if (channel_token != "02" && !validate_measure_token_length(data)) {
                auto severity = options.tolerant ? BmsParseSeverity::Warning : BmsParseSeverity::Error;
                add_message(result.messages, severity, line_number,
                            "Measure data must consist of pairs of characters.");
                if (!options.tolerant) {
                    continue;
                }
            }

            result.chart.commands.push_back(BmsMeasureCommand{measure_index, channel_token, data});
            continue;
        }

        std::string value = parse_header_value(tail);
        to_upper_ascii(key);

        if (key == "BPM" || key == "PLAYER" || key == "GENRE" || key == "TITLE" ||
            key == "ARTIST" || key == "PLAYLEVEL" || key == "RANK" || key == "TOTAL" ||
            key == "VOLWAV") {
            result.chart.headers[key] = value;
            if (key == "BPM") {
                bool ok = false;
                double bpm_value = parse_double(value, ok);
                if (!ok) {
                    add_message(result.messages, BmsParseSeverity::Error, line_number,
                                "Failed to parse #BPM value as floating point.");
                } else {
                    result.chart.base_bpm = bpm_value;
                }
            }
            continue;
        }

        if (key.rfind("WAV", 0) == 0) {
            std::string slot = key.substr(3);
            if (slot.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "#WAV entry missing identifier.");
                continue;
            }
            result.chart.wav[slot] = value;
            continue;
        }

        if (key.rfind("BMP", 0) == 0) {
            std::string slot = key.substr(3);
            if (slot.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "#BMP entry missing identifier.");
                continue;
            }
            result.chart.bmp[slot] = value;
            continue;
        }

        if (key.rfind("BPM", 0) == 0 && key.size() > 3) {
            std::string slot = key.substr(3);
            bool ok = false;
            double bpm_value = parse_double(value, ok);
            if (!ok) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Failed to parse #BPMxx value as floating point.");
                continue;
            }
            result.chart.bpm[slot] = bpm_value;
            continue;
        }

        if (key.rfind("STOP", 0) == 0) {
            std::string slot = key.substr(4);
            if (slot.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "#STOP entry missing identifier.");
                continue;
            }
            bool ok = false;
            double value_ms = parse_double(value, ok);
            if (!ok) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Failed to parse #STOPxx value as floating point.");
                continue;
            }
            result.chart.stop[slot] = value_ms;
            continue;
        }

        // Unknown header; store it for completeness.
        result.chart.headers[key] = value;
    }

    return result;
}

BmsParseResult BmsParser::parseFile(const std::string& path, const BmsParserOptions& options) const {
    std::ifstream file;
#ifdef _WIN32
    try {
        file.open(std::filesystem::u8path(path), std::ios::binary);
    } catch (...) {
        file.open(path, std::ios::binary);
    }
#else
    file.open(path, std::ios::binary);
#endif
    if (!file) {
        BmsParseResult result;
        add_message(result.messages, BmsParseSeverity::Error, 0,
                    "Failed to open BMS file: " + path);
        return result;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str(), options);
}

}  // namespace tenriff::chart
