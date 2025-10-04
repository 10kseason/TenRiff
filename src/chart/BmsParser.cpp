#include "chart/BmsParser.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>

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
    token = trim(token);
    if (token.empty()) {
        ok = false;
        return 0.0;
    }
    double result = 0.0;
    auto* begin = token.data();
    auto* end = token.data() + token.size();
    std::string temp(begin, end);
    try {
        size_t consumed = 0;
        result = std::stod(temp, &consumed);
        ok = consumed == temp.size();
    } catch (...) {
        ok = false;
    }
    return result;
}

bool parse_int(std::string_view token, int& out_value) {
    token = trim(token);
    if (token.empty()) {
        return false;
    }
    int value = 0;
    auto first = token.data();
    auto last = token.data() + token.size();
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

bool validate_measure_token_length(const std::string& data) {
    return (data.size() % 2) == 0;
}

}  // namespace

bool BmsParseResult::success() const {
    return std::none_of(messages.begin(), messages.end(), [](const BmsParseMessage& msg) {
        return msg.severity == BmsParseSeverity::Error;
    });
}

BmsParseResult BmsParser::parse(std::string_view content, const BmsParserOptions& options) const {
    BmsParseResult result;
    std::string cleaned = remove_bom(content);

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
        if (trimmed.front() != '#') {
            add_message(result.messages, BmsParseSeverity::Warning, line_number,
                        "Line ignored because it does not start with '#'.");
            continue;
        }

        std::string_view command_view(trimmed.c_str() + 1, trimmed.size() - 1);
        auto colon_pos = command_view.find(':');
        if (colon_pos == std::string_view::npos) {
            // Header style: #KEY VALUE
            std::string command = trim(command_view);
            auto space_pos = command.find_first_of(" \t");
            std::string key = command.substr(0, space_pos);
            to_upper_ascii(key);
            std::string value;
            if (space_pos != std::string::npos) {
                value = trim(command.substr(space_pos + 1));
            }
            if (key.empty()) {
                add_message(result.messages, BmsParseSeverity::Error, line_number,
                            "Empty command key.");
                continue;
            }

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
            continue;
        }

        std::string_view head_view = command_view.substr(0, colon_pos);
        std::string left = trim(head_view);
        std::string data = trim(command_view.substr(colon_pos + 1));
        // Remove all spaces and tabs from data to ensure even-length token parsing.
        data.erase(std::remove_if(data.begin(), data.end(), [](unsigned char ch) {
                      return ch == ' ' || ch == '\t';
                  }),
                  data.end());

        if (left.size() < 5) {
            add_message(result.messages, BmsParseSeverity::Error, line_number,
                        "Measure command is too short.");
            continue;
        }
        std::string measure_token = left.substr(0, 3);
        if (!std::all_of(measure_token.begin(), measure_token.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            add_message(result.messages, BmsParseSeverity::Error, line_number,
                        "Measure number must be three digits.");
            continue;
        }
        int measure_index = 0;
        if (!parse_int(measure_token, measure_index)) {
            add_message(result.messages, BmsParseSeverity::Error, line_number,
                        "Failed to parse measure index.");
            continue;
        }

        std::string channel_token = left.substr(3);
        channel_token = trim(channel_token);
        to_upper_ascii(channel_token);
        if (channel_token.size() != 2) {
            add_message(result.messages, BmsParseSeverity::Error, line_number,
                        "Channel token must have length 2.");
            continue;
        }

        if (channel_token != "02" && !validate_measure_token_length(data)) {
            auto severity = options.tolerant ? BmsParseSeverity::Warning : BmsParseSeverity::Error;
            add_message(result.messages, severity, line_number,
                        "Measure data must consist of pairs of characters.");
            if (!options.tolerant) {
                continue;
            }
        }

        result.chart.commands.push_back(BmsMeasureCommand{measure_index, channel_token, data});
    }

    return result;
}

BmsParseResult BmsParser::parseFile(const std::string& path, const BmsParserOptions& options) const {
    std::ifstream file(path, std::ios::binary);
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
