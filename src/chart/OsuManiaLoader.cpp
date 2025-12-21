#include "chart/OsuManiaLoader.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

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

bool parse_int(std::string_view view, int& out) {
    view = trim(view);
    if (view.empty()) {
        return false;
    }
    int value = 0;
    auto first = view.data();
    auto last = view.data() + view.size();
    auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    out = value;
    return true;
}

bool parse_double(std::string_view view, double& out) {
    view = trim(view);
    if (view.empty()) {
        return false;
    }
    double value = 0.0;
    auto first = view.data();
    auto last = view.data() + view.size();
    std::string buffer(first, last);
    try {
        size_t consumed = 0;
        value = std::stod(buffer, &consumed);
        if (consumed != buffer.size()) {
            return false;
        }
    } catch (...) {
        return false;
    }
    out = value;
    return true;
}

void add_message(std::vector<OsuParseMessage>& messages, OsuParseSeverity severity, std::size_t line,
                 std::string text) {
    messages.push_back(OsuParseMessage{severity, line, std::move(text)});
}

int column_from_x(int x, int key_count) {
    if (key_count <= 0) {
        return 0;
    }
    double position = static_cast<double>(x) * static_cast<double>(key_count) / 512.0;
    int column = static_cast<int>(std::floor(position));
    if (column < 0) {
        column = 0;
    }
    if (column >= key_count) {
        column = key_count - 1;
    }
    return column;
}

}  // namespace

bool OsuManiaParseResult::success() const {
    return std::none_of(messages.begin(), messages.end(), [](const OsuParseMessage& message) {
        return message.severity == OsuParseSeverity::Error;
    });
}

OsuManiaParseResult OsuManiaLoader::parse(std::string_view content) const {
    OsuManiaParseResult result;

    std::istringstream stream{std::string(content)};
    std::string line;
    std::string current_section;

    bool mode_found = false;
    bool mode_valid = false;

    std::size_t line_number = 0;

    auto maybe_set_bpm = [&](double beat_length) {
        if (beat_length <= 0.0) {
            return;
        }
        if (result.chart.base_bpm <= 0.0) {
            result.chart.base_bpm = 60000.0 / beat_length;
        }
    };

    while (std::getline(stream, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.rfind("//", 0) == 0) {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        if (current_section == "General") {
            auto sep = trimmed.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            std::string key = trim(trimmed.substr(0, sep));
            std::string value = trim(trimmed.substr(sep + 1));
            if (key == "Mode") {
                mode_found = true;
                int mode = 0;
                if (!parse_int(value, mode)) {
                    add_message(result.messages, OsuParseSeverity::Error, line_number,
                                "Failed to parse Mode value as integer.");
                    continue;
                }
                if (mode != 3) {
                    add_message(result.messages, OsuParseSeverity::Error, line_number,
                                "Beatmap mode is not osu!mania (Mode=3).");
                } else {
                    mode_valid = true;
                }
            }
            continue;
        }

        if (current_section == "Difficulty") {
            auto sep = trimmed.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            std::string key = trim(trimmed.substr(0, sep));
            std::string value = trim(trimmed.substr(sep + 1));
            if (key == "KeyCount") {
                int keys = 0;
                if (!parse_int(value, keys) || keys <= 0) {
                    add_message(result.messages, OsuParseSeverity::Error, line_number,
                                "KeyCount must be a positive integer.");
                    continue;
                }
                result.chart.key_count = keys;
            }
            continue;
        }

        if (current_section == "Metadata") {
            auto sep = trimmed.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            std::string key = trim(trimmed.substr(0, sep));
            std::string value = trim(trimmed.substr(sep + 1));
            if (key == "Title") {
                result.chart.title = value;
            } else if (key == "Artist") {
                result.chart.artist = value;
            }
            continue;
        }

        if (current_section == "TimingPoints") {
            std::stringstream timing_line(trimmed);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(timing_line, token, ',')) {
                tokens.push_back(trim(token));
            }
            if (tokens.size() < 2) {
                add_message(result.messages, OsuParseSeverity::Error, line_number,
                            "Timing point is missing required fields.");
                continue;
            }

            double time_ms = 0.0;
            double beat_length = 0.0;
            int uninherited = 1;

            if (!parse_double(tokens[0], time_ms)) {
                add_message(result.messages, OsuParseSeverity::Error, line_number,
                            "Failed to parse timing point time.");
                continue;
            }
            if (!parse_double(tokens[1], beat_length)) {
                add_message(result.messages, OsuParseSeverity::Error, line_number,
                            "Failed to parse timing point beat length.");
                continue;
            }
            if (tokens.size() >= 7) {
                parse_int(tokens[6], uninherited);
            }

            OsuManiaTimingPoint point;
            point.time_ms = time_ms;
            point.beat_length = beat_length;
            point.inherited = uninherited == 0;

            if (!point.inherited) {
                maybe_set_bpm(point.beat_length);
            }

            result.chart.timing_points.push_back(point);
            continue;
        }

        if (current_section == "HitObjects") {
            std::stringstream hit_line(trimmed);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(hit_line, token, ',')) {
                tokens.push_back(trim(token));
            }
            if (tokens.size() < 5) {
                add_message(result.messages, OsuParseSeverity::Error, line_number,
                            "Hit object is missing required fields.");
                continue;
            }

            int x = 0;
            int y_unused = 0;
            int time = 0;
            int type = 0;
            int hit_sound = 0;

            if (!parse_int(tokens[0], x) || !parse_int(tokens[1], y_unused) || !parse_int(tokens[2], time) ||
                !parse_int(tokens[3], type) || !parse_int(tokens[4], hit_sound)) {
                add_message(result.messages, OsuParseSeverity::Error, line_number,
                            "Failed to parse hit object numeric fields.");
                continue;
            }

            if (result.chart.key_count <= 0) {
                add_message(result.messages, OsuParseSeverity::Error, line_number,
                            "Encountered hit object before KeyCount was defined.");
                continue;
            }

            OsuManiaNote note;
            note.column = column_from_x(x, result.chart.key_count);
            note.start_time_ms = time;
            note.hit_sound = hit_sound;

            bool is_hold = (type & 128) != 0;
            if (is_hold) {
                if (tokens.size() < 6) {
                    add_message(result.messages, OsuParseSeverity::Error, line_number,
                                "Hold note is missing end time field.");
                } else {
                    auto colon = tokens[5].find(':');
                    std::string_view hold_segment(tokens[5]);
                    if (colon != std::string::npos) {
                        hold_segment = hold_segment.substr(0, colon);
                    }
                    int end_time = 0;
                    if (!parse_int(hold_segment, end_time)) {
                        add_message(result.messages, OsuParseSeverity::Error, line_number,
                                    "Failed to parse hold note end time.");
                    } else if (end_time < time) {
                        add_message(result.messages, OsuParseSeverity::Warning, line_number,
                                    "Hold note end time is earlier than start time.");
                        note.end_time_ms = time;
                    } else {
                        note.end_time_ms = end_time;
                    }
                }
            }

            result.chart.notes.push_back(note);
            continue;
        }
    }

    if (!mode_found) {
        add_message(result.messages, OsuParseSeverity::Warning, 0, "Mode not specified; assuming osu!mania.");
        mode_valid = true;
    }

    if (!mode_valid) {
        add_message(result.messages, OsuParseSeverity::Error, 0, "Beatmap mode validation failed.");
    }

    if (result.chart.key_count <= 0) {
        add_message(result.messages, OsuParseSeverity::Error, 0, "KeyCount was not specified.");
    }

    return result;
}

}  // namespace tenriff::chart
