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

void to_upper_ascii(std::string& value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
}

bool parse_int(std::string_view view, int& out) {
    std::string trimmed = trim(view);  // Store in stable string
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
    out = value;
    return true;
}

bool parse_double(std::string_view view, double& out) {
    std::string trimmed = trim(view);  // Store in stable string
    if (trimmed.empty()) {
        return false;
    }
    double value = 0.0;
    std::istringstream ss(trimmed);
    ss.imbue(std::locale::classic());  // Locale-independent parsing
    ss >> value;
    // Check for complete consumption: no failure AND no remaining characters.
    if (ss.fail() || ss.peek() != std::char_traits<char>::eof()) {
        return false;
    }
    out = value;
    return true;
}

std::vector<std::string> split_csv_fields(std::string_view line) {
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;
    for (char ch : line) {
        if (ch == '"') {
            in_quotes = !in_quotes;
            current.push_back(ch);
            continue;
        }
        if (ch == ',' && !in_quotes) {
            fields.push_back(trim(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(trim(current));
    return fields;
}

std::string normalize_event_asset_field(std::string_view view) {
    std::string value = trim(view);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = trim(std::string_view(value).substr(1, value.size() - 2));
    }
    return value;
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
            to_upper_ascii(current_section);  // Case-insensitive section matching
            continue;
        }

        if (current_section == "GENERAL") {
            auto sep = trimmed.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            std::string key = trim(trimmed.substr(0, sep));
            to_upper_ascii(key);  // Case-insensitive key matching
            std::string value = trim(trimmed.substr(sep + 1));
            if (key == "MODE") {
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
            } else if (key == "AUDIOFILENAME") {
                result.chart.audio_filename = value;
            }
            continue;
        }

        if (current_section == "DIFFICULTY") {
            auto sep = trimmed.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            std::string key = trim(trimmed.substr(0, sep));
            to_upper_ascii(key);  // Case-insensitive key matching
            std::string value = trim(trimmed.substr(sep + 1));
            if (key == "KEYCOUNT" || key == "CIRCLESIZE") {
                int keys = 0;
                if (!parse_int(value, keys) || keys <= 0) {
                    add_message(result.messages, OsuParseSeverity::Error, line_number,
                                "KeyCount/CircleSize must be a positive integer.");
                    continue;
                }
                result.chart.key_count = keys;
            } else if (key == "OVERALLDIFFICULTY") {
                double od = 0.0;
                if (!parse_double(value, od)) {
                    add_message(result.messages, OsuParseSeverity::Warning, line_number,
                                "Failed to parse OverallDifficulty value.");
                } else {
                    result.chart.overall_difficulty = od;
                }
            }
            continue;
        }

        if (current_section == "METADATA") {
            auto sep = trimmed.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            std::string key = trim(trimmed.substr(0, sep));
            to_upper_ascii(key);  // Case-insensitive key matching
            std::string value = trim(trimmed.substr(sep + 1));
            if (key == "TITLE") {
                result.chart.title = value;
            } else if (key == "TITLEUNICODE") {
                result.chart.title_unicode = value;
            } else if (key == "ARTIST") {
                result.chart.artist = value;
            } else if (key == "ARTISTUNICODE") {
                result.chart.artist_unicode = value;
            } else if (key == "VERSION") {
                result.chart.version = value;
            }
            continue;
        }

        if (current_section == "EVENTS") {
            const std::vector<std::string> tokens = split_csv_fields(trimmed);
            if (tokens.size() >= 3) {
                std::string event_type = trim(tokens[0]);
                to_upper_ascii(event_type);
                if ((event_type == "0" || event_type == "BACKGROUND") &&
                    result.chart.background_filename.empty()) {
                    result.chart.background_filename = normalize_event_asset_field(tokens[2]);
                }
            }
            continue;
        }

        if (current_section == "TIMINGPOINTS") {
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

        if (current_section == "HITOBJECTS") {
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
        add_message(result.messages, OsuParseSeverity::Error, 0, "KeyCount/CircleSize was not specified.");
    }

    return result;
}

}  // namespace tenriff::chart
