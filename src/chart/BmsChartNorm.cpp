#include "chart/BmsChartNorm.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <string_view>

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
        // As a fallback, interpret as base-36 encoded integer BPM.
        auto parsed_base36 = parse_int_base36(token);
        if (parsed_base36.has_value()) {
            return static_cast<double>(parsed_base36.value());
        }
        return std::nullopt;
    };

    for (const auto& command : chart.commands) {
        if (command.channel == "02") {
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
                event.type = BmsNormalizedEventType::Bga;
            } else if (command.channel == "04") {
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
