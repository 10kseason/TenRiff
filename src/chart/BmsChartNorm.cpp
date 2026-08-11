#include "chart/BmsChartNorm.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <string_view>
#include <unordered_map>

namespace tenriff::chart {

namespace {

void add_message(std::vector<BmsNormalizationMessage>& messages, BmsParseSeverity severity, int measure,
                 std::string text) {
    messages.push_back(BmsNormalizationMessage{severity, measure, std::move(text)});
}

std::string to_upper_ascii(std::string_view token) {
    std::string upper(token);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<char>(ch - ('a' - 'A'));
        }
        return static_cast<char>(ch);
    });
    return upper;
}

std::optional<double> parse_double(std::string_view view) {
    std::string trimmed(view);
    // Trim whitespace manually to avoid locale dependence on stod ignoring trailing spaces.
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    auto begin = std::find_if_not(trimmed.begin(), trimmed.end(), is_space);
    auto end = std::find_if_not(trimmed.rbegin(), trimmed.rend(), is_space).base();
    if (begin >= end) {
        return std::nullopt;
    }
    trimmed.assign(begin, end);
    try {
        size_t consumed = 0;
        double value = std::stod(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            return std::nullopt;
        }
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> parse_measure_length(std::string_view token, std::string& error) {
    auto slash_pos = token.find('/');
    if (slash_pos != std::string_view::npos) {
        std::string_view numerator_view = token.substr(0, slash_pos);
        std::string_view denominator_view = token.substr(slash_pos + 1);
        if (numerator_view.empty() || denominator_view.empty()) {
            error = "Measure length fraction must include both numerator and denominator (#nnn02).";
            return std::nullopt;
        }

        auto numerator = parse_double(numerator_view);
        auto denominator = parse_double(denominator_view);
        if (!numerator.has_value() || !denominator.has_value()) {
            error = "Failed to parse measure length fraction (#nnn02) as numbers.";
            return std::nullopt;
        }

        if (std::abs(denominator.value()) < 1e-12) {
            error = "Measure length fraction denominator cannot be zero (#nnn02).";
            return std::nullopt;
        }

        return numerator.value() / denominator.value();
    }

    auto value = parse_double(token);
    if (!value.has_value()) {
        error = "Failed to parse measure length (#nnn02) value as floating point.";
    }
    return value;
}

std::optional<int> parse_int_base16(std::string_view token) {
    if (token.empty()) {
        return std::nullopt;
    }
    int value = 0;
    for (char ch : token) {
        value *= 16;
        if (ch >= '0' && ch <= '9') {
            value += ch - '0';
        } else if (ch >= 'A' && ch <= 'F') {
            value += 10 + (ch - 'A');
        } else if (ch >= 'a' && ch <= 'f') {
            value += 10 + (ch - 'a');
        } else {
            return std::nullopt;
        }
    }
    return value;
}
std::optional<int> parse_int_base36(std::string_view token) {
    if (token.empty()) {
        return std::nullopt;
    }
    int value = 0;
    for (char ch : token) {
        value *= 36;
        if (ch >= '0' && ch <= '9') {
            value += ch - '0';
        } else if (ch >= 'A' && ch <= 'Z') {
            value += 10 + (ch - 'A');
        } else if (ch >= 'a' && ch <= 'z') {
            value += 10 + (ch - 'a');
        } else {
            return std::nullopt;
        }
    }
    return value;
}


bool is_potential_note_channel(std::string_view channel) {
    if (channel.size() != 2) {
        return false;
    }
    char first = channel[0];
    return first == '1' || first == '2' || first == '5' || first == '6';
}

bool is_long_note_channel(std::string_view channel) {
    return channel.size() == 2 && (channel[0] == '5' || channel[0] == '6');
}
std::optional<std::size_t> mine_lane_for_channel(const NoteLaneMapping& mapping,
                                                 std::string_view channel) {
    if (channel.size() != 2 ||
        (channel[0] != 'D' && channel[0] != 'E') ||
        channel[1] < '1' || channel[1] > '9') {
        return std::nullopt;
    }
    std::string visible_channel;
    visible_channel.push_back(channel[0] == 'D' ? '1' : '2');
    visible_channel.push_back(channel[1]);
    return mapping.laneForChannel(visible_channel);
}


}  // namespace

bool BmsNormalizationResult::success() const {
    return std::none_of(messages.begin(), messages.end(), [](const BmsNormalizationMessage& message) {
        return message.severity == BmsParseSeverity::Error;
    });
}

BmsNormalizationResult BmsChartNormalizer::normalize(const BmsChart& chart) const {
    BmsNormalizationResult result;
    result.chart.lane_mapping = chart.lane_mapping;
    result.chart.base_bpm = chart.base_bpm;

    int max_measure = 0;
    for (const auto& command : chart.commands) {
        max_measure = std::max(max_measure, command.measure);
    }

    if (max_measure < 0) {
        max_measure = 0;
    }

    result.chart.measures.resize(static_cast<std::size_t>(max_measure) + 1);
    for (auto& measure : result.chart.measures) {
        measure.length = 1.0;
    }

    for (const auto& command : chart.commands) {
        if (command.channel != "02") {
            continue;
        }
        std::string length_error;
        auto maybe_length = parse_measure_length(command.data, length_error);
        if (!maybe_length.has_value()) {
            if (length_error.empty()) {
                length_error = "Failed to parse measure length (#nnn02) value as positive number.";
            }
            add_message(result.messages, BmsParseSeverity::Error, command.measure, std::move(length_error));
            continue;
        }
        if (!std::isfinite(maybe_length.value()) || maybe_length.value() <= 0.0) {
            add_message(result.messages, BmsParseSeverity::Error, command.measure,
                        "Failed to parse measure length (#nnn02) value as positive number.");
            continue;
        }
        if (static_cast<std::size_t>(command.measure) >= result.chart.measures.size()) {
            result.chart.measures.resize(static_cast<std::size_t>(command.measure) + 1, BmsMeasureTiming{});
            result.chart.measures.back().length = 1.0;
        }
        result.chart.measures[static_cast<std::size_t>(command.measure)].length = maybe_length.value();
    }

    double cursor = 0.0;
    for (auto& measure : result.chart.measures) {
        measure.start = cursor;
        cursor += measure.length;
    }

    auto handle_bpm_reference = [&](const std::string& token) -> std::optional<double> {
        auto lookup = chart.bpm.find(token);
        if (lookup != chart.bpm.end()) {
            return lookup->second;
        }
        return std::nullopt;
    };
    auto handle_scroll_reference = [&](const std::string& token) -> std::optional<double> {
        const auto lookup = chart.scroll.find(token);
        if (lookup != chart.scroll.end()) {
            return lookup->second;
        }
        return std::nullopt;
    };

    const bool lntype2 = [&chart]() {
        const auto it = chart.headers.find("LNTYPE");
        if (it == chart.headers.end()) {
            return false;
        }
        const auto value = parse_double(it->second);
        return value.has_value() && std::abs(value.value() - 2.0) < 1e-9;
    }();

    if (lntype2) {
        std::unordered_map<std::string, std::vector<const BmsMeasureCommand*>> commands_by_channel;
        for (const auto& command : chart.commands) {
            if (is_long_note_channel(command.channel)) {
                commands_by_channel[command.channel].push_back(&command);
            }
        }

        for (auto& [channel, commands] : commands_by_channel) {
            std::stable_sort(commands.begin(), commands.end(), [](const auto* lhs, const auto* rhs) {
                return lhs->measure < rhs->measure;
            });

            const auto lane = chart.lane_mapping.laneForChannel(channel);
            if (!lane.has_value()) {
                continue;
            }

            bool active = false;
            std::string active_object;
            const BmsMeasureCommand* last_command = nullptr;
            std::size_t last_slice_count = 0;

            const auto append_endpoint = [&](const BmsMeasureCommand& command,
                                             std::size_t slice_index,
                                             std::size_t slice_count,
                                             double intra_measure,
                                             std::string_view object_id) {
                const auto& measure = result.chart.measures[static_cast<std::size_t>(command.measure)];
                BmsNormalizedEvent event;
                event.type = BmsNormalizedEventType::Note;
                event.measure = command.measure;
                event.slice_index = slice_index;
                event.slice_count = slice_count;
                event.intra_measure = intra_measure;
                event.position = measure.start + measure.length * intra_measure;
                event.channel = command.channel;
                event.object_id = std::string(object_id);
                event.lane = lane;
                result.chart.events.push_back(std::move(event));
            };

            for (const BmsMeasureCommand* command : commands) {
                if (!command || command->data.empty()) {
                    continue;
                }
                if (command->data.size() % 2 != 0) {
                    add_message(result.messages, BmsParseSeverity::Error, command->measure,
                                "LNTYPE 2 command data does not consist of pairs of characters.");
                    continue;
                }

                const std::size_t slice_count = command->data.size() / 2;
                if (slice_count == 0) {
                    continue;
                }

                if (active && last_command && command->measure > last_command->measure + 1) {
                    append_endpoint(*last_command, last_slice_count, last_slice_count, 1.0, active_object);
                    active = false;
                    active_object.clear();
                }

                for (std::size_t index = 0; index < slice_count; ++index) {
                    const std::string token = to_upper_ascii(command->data.substr(index * 2, 2));
                    const double intra = static_cast<double>(index) / static_cast<double>(slice_count);
                    if (token != "00") {
                        if (!active) {
                            active = true;
                            active_object = token;
                            append_endpoint(*command, index, slice_count, intra, active_object);
                        }
                    } else if (active) {
                        append_endpoint(*command, index, slice_count, intra, active_object);
                        active = false;
                        active_object.clear();
                    }
                }

                last_command = command;
                last_slice_count = slice_count;
            }

            if (active && last_command) {
                append_endpoint(*last_command, last_slice_count, last_slice_count, 1.0, active_object);
                add_message(result.messages, BmsParseSeverity::Warning, last_command->measure,
                            "LNTYPE 2 long note reached the end of its last command without an explicit 00; closed at the measure boundary.");
            }
        }
    }


    for (const auto& command : chart.commands) {
        if (command.channel == "02") {
            continue;
        }

        if (lntype2 && is_long_note_channel(command.channel)) {
            continue;
        }

        if (command.data.empty()) {
            continue;
        }
        if (command.data.size() % 2 != 0) {
            add_message(result.messages, BmsParseSeverity::Error, command.measure,
                        "Measure command data does not consist of pairs of characters.");
            continue;
        }

        std::size_t slice_count = command.data.size() / 2;
        if (slice_count == 0) {
            continue;
        }

        if (static_cast<std::size_t>(command.measure) >= result.chart.measures.size()) {
            result.chart.measures.resize(static_cast<std::size_t>(command.measure) + 1);
            for (std::size_t i = 0; i < result.chart.measures.size(); ++i) {
                if (result.chart.measures[i].length <= 0.0) {
                    result.chart.measures[i].length = 1.0;
                }
            }
            cursor = 0.0;
            for (auto& measure : result.chart.measures) {
                measure.start = cursor;
                cursor += measure.length;
            }
        }

        const auto& measure_info = result.chart.measures[static_cast<std::size_t>(command.measure)];

        for (std::size_t index = 0; index < slice_count; ++index) {
            std::string token = command.data.substr(index * 2, 2);
            std::string normalized_token = to_upper_ascii(token);
            if (normalized_token == "00") {
                continue;
            }

            BmsNormalizedEvent event;
            event.measure = command.measure;
            event.slice_index = index;
            event.slice_count = slice_count;
            event.intra_measure = slice_count > 0 ? static_cast<double>(index) / static_cast<double>(slice_count) : 0.0;
            event.position = measure_info.start + measure_info.length * event.intra_measure;
            event.channel = command.channel;
            event.object_id = normalized_token;

            if (command.channel == "01") {
                event.type = BmsNormalizedEventType::Bgm;
            } else if (command.channel == "03") {
                event.type = BmsNormalizedEventType::BpmChange;
                auto bpm_value = parse_int_base16(normalized_token);
                if (!bpm_value.has_value()) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "BPM channel 03 token must be valid hexadecimal: '" + normalized_token + "'.");
                    continue;
                }
                if (bpm_value.value() <= 0) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "BPM channel 03 value must be positive: '" + normalized_token + "'.");
                    continue;
                }
                event.value = static_cast<double>(bpm_value.value());
            } else if (command.channel == "04" || command.channel == "07") {
                event.type = BmsNormalizedEventType::Bga;
            } else if (command.channel == "06") {
                event.type = BmsNormalizedEventType::Poor;
            } else if (command.channel == "08") {
                event.type = BmsNormalizedEventType::BpmChange;
                auto bpm_value = handle_bpm_reference(normalized_token);
                if (!bpm_value.has_value()) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "BPM change references undefined value '" + normalized_token + "'.");
                    continue;
                }
                event.value = bpm_value;
            } else if (command.channel == "09") {
                event.type = BmsNormalizedEventType::Stop;
                auto stop_it = chart.stop.find(normalized_token);
                if (stop_it == chart.stop.end()) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "STOP references undefined value '" + normalized_token + "'.");
                    continue;
                }
                event.value = stop_it->second;
            } else if (command.channel == "SC") {
                event.type = BmsNormalizedEventType::Scroll;
                auto scroll_value = handle_scroll_reference(normalized_token);
                if (!scroll_value.has_value()) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "SCROLL references undefined value '" + normalized_token + "'.");
                    continue;
                }
                if (!std::isfinite(scroll_value.value())) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "SCROLL value must be finite: '" + normalized_token + "'.");
                    continue;
                }
                event.value = scroll_value.value();
            } else if (auto mine_lane = mine_lane_for_channel(chart.lane_mapping, command.channel);
                       mine_lane.has_value()) {
                auto damage_value = parse_int_base36(normalized_token);
                if (!damage_value.has_value() || damage_value.value() <= 0) {
                    add_message(result.messages, BmsParseSeverity::Error, command.measure,
                                "Landmine damage token must be base-36 01-ZZ: '" + normalized_token + "'.");
                    continue;
                }
                event.type = BmsNormalizedEventType::Mine;
                event.lane = mine_lane;
                event.value = static_cast<double>(damage_value.value()) / 2.0;
            } else {
                auto lane = chart.lane_mapping.laneForChannel(command.channel);
                if (lane.has_value()) {
                    event.type = BmsNormalizedEventType::Note;
                    event.lane = lane;
                } else {
                    event.type = BmsNormalizedEventType::Unknown;
                    if (is_potential_note_channel(command.channel)) {
                        add_message(result.messages, BmsParseSeverity::Warning, command.measure,
                                    "Encountered note data on unmapped channel '" + command.channel + "'.");
                    }
                }
            }

            result.chart.events.push_back(std::move(event));
        }
    }

    std::stable_sort(result.chart.events.begin(), result.chart.events.end(),
                     [](const BmsNormalizedEvent& lhs, const BmsNormalizedEvent& rhs) {
                         if (std::abs(lhs.position - rhs.position) > 1e-9) {
                             return lhs.position < rhs.position;
                         }
                         if (lhs.measure != rhs.measure) {
                             return lhs.measure < rhs.measure;
                         }
                         if (lhs.slice_count != rhs.slice_count) {
                             return lhs.slice_count < rhs.slice_count;
                         }
                         if (lhs.slice_index != rhs.slice_index) {
                             return lhs.slice_index < rhs.slice_index;
                         }
                         return lhs.channel < rhs.channel;
                     });

    return result;
}

}  // namespace tenriff::chart
