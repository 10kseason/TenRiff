#include "chart/BmsTimeline.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tenriff::chart {

namespace {

constexpr double kBeatsPerMeasure = 4.0;
constexpr double kStopTicksPerBeat = 48.0;
constexpr double kPositionEpsilon = 1e-9;

bool nearly_equal(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kPositionEpsilon;
}

double seconds_from_position_delta(double delta, double bpm) {
    if (delta <= 0.0) {
        return 0.0;
    }
    double beats = delta * kBeatsPerMeasure;
    return (beats * 60.0) / bpm;
}

double stop_ticks_to_seconds(double stop_value, double bpm) {
    if (stop_value <= 0.0) {
        return 0.0;
    }
    double beats = stop_value / kStopTicksPerBeat;
    return (beats * 60.0) / bpm;
}

void add_message(std::vector<BmsTimelineMessage>& messages, BmsParseSeverity severity, int measure,
                 std::string text) {
    messages.push_back(BmsTimelineMessage{severity, measure, std::move(text)});
}

}  // namespace

bool BmsTimelineResult::success() const {
    return std::none_of(messages.begin(), messages.end(), [](const BmsTimelineMessage& message) {
        return message.severity == BmsParseSeverity::Error;
    });
}

BmsTimelineResult BmsTimelineBuilder::build(const BmsNormalizedChart& chart, int sample_rate_hz) const {
    BmsTimelineResult result;

    if (sample_rate_hz <= 0) {
        add_message(result.messages, BmsParseSeverity::Error, 0,
                    "Sample rate must be a positive integer.");
        return result;
    }

    double sample_rate = static_cast<double>(sample_rate_hz);

    double current_bpm = chart.base_bpm;
    double current_position = 0.0;
    double current_time_samples = 0.0;
    double current_scroll = 1.0;
    double current_visual_position = 0.0;
    const double reference_bpm = chart.base_bpm;

    auto append_scroll_segment = [&](double start_sample,
                                     double end_sample,
                                     double start_position,
                                     double end_position) {
        const int64_t rounded_start = static_cast<int64_t>(std::llround(start_sample));
        const int64_t rounded_end = static_cast<int64_t>(std::llround(end_sample));
        if (rounded_end <= rounded_start) {
            return;
        }
        result.timeline.scroll_segments.push_back(BmsScrollSegment{
            rounded_start,
            rounded_end,
            start_position,
            end_position,
        });
    };


    const auto& events = chart.events;
    std::size_t index = 0;

    while (index < events.size()) {
        double group_position = events[index].position;
        if (group_position + kPositionEpsilon < current_position) {
            add_message(result.messages, BmsParseSeverity::Error, events[index].measure,
                        "Normalized events are not sorted by position.");
            group_position = current_position;
        }

        double delta_position = group_position - current_position;
        if (delta_position > kPositionEpsilon) {
            if (!std::isfinite(current_bpm) || current_bpm <= 0.0) {
                add_message(result.messages, BmsParseSeverity::Error, events[index].measure,
                            "Encountered non-positive BPM while advancing the timeline.");
                break;
            }
            double delta_time_seconds = seconds_from_position_delta(delta_position, current_bpm);
            const double segment_start_sample = current_time_samples;
            const double segment_start_position = current_visual_position;
            current_time_samples += delta_time_seconds * sample_rate;
            // Keep the selected Hi-Speed anchored to the chart's starting BPM.
            // BPM changes still alter event timing, but no longer change visual
            // pixels-per-second. Explicit #SCROLL factors, stops, and reverse
            // motion remain represented by current_scroll.
            current_visual_position +=
                delta_time_seconds * reference_bpm /
                (kBeatsPerMeasure * 60.0) * current_scroll;
            append_scroll_segment(segment_start_sample, current_time_samples,
                                  segment_start_position, current_visual_position);
            current_position = group_position;
        }

        std::size_t group_end = index;
        while (group_end < events.size() && nearly_equal(events[group_end].position, group_position)) {
            ++group_end;
        }

        for (std::size_t i = index; i < group_end; ++i) {
            result.timeline.events.push_back(
                BmsScheduledEvent{events[i], static_cast<int64_t>(std::llround(current_time_samples))});
        }

        double stop_accumulated = 0.0;
        for (std::size_t i = index; i < group_end; ++i) {
            const auto& event = events[i];
            if (event.type == BmsNormalizedEventType::BpmChange) {
                if (!event.value.has_value()) {
                    add_message(result.messages, BmsParseSeverity::Error, event.measure,
                                "BPM change event is missing a BPM value.");
                    continue;
                }
                double bpm = event.value.value();
                if (!std::isfinite(bpm) || bpm <= 0.0) {
                    add_message(result.messages, BmsParseSeverity::Error, event.measure,
                                "BPM change event must specify a positive finite BPM value.");
                    continue;
                }
                current_bpm = bpm;
            } else if (event.type == BmsNormalizedEventType::Scroll) {
                if (!event.value.has_value() || !std::isfinite(event.value.value())) {
                    add_message(result.messages, BmsParseSeverity::Error, event.measure,
                                "SCROLL event is missing a finite factor.");
                    continue;
                }
                current_scroll = event.value.value();
            } else if (event.type == BmsNormalizedEventType::Stop) {
                if (!event.value.has_value()) {
                    add_message(result.messages, BmsParseSeverity::Error, event.measure,
                                "STOP event is missing a duration value.");
                    continue;
                }
                double stop_value = event.value.value();
                if (!std::isfinite(stop_value) || stop_value < 0.0) {
                    add_message(result.messages, BmsParseSeverity::Error, event.measure,
                                "STOP event must specify a non-negative duration value.");
                    continue;
                }
                if (!std::isfinite(current_bpm) || current_bpm <= 0.0) {
                    add_message(result.messages, BmsParseSeverity::Error, event.measure,
                                "Cannot apply STOP because BPM is not positive.");
                    continue;
                }
                stop_accumulated += stop_ticks_to_seconds(stop_value, current_bpm);
            }
        }

        const double stop_samples = stop_accumulated * sample_rate;
        if (stop_samples > 0.0) {
            append_scroll_segment(current_time_samples, current_time_samples + stop_samples,
                                  current_visual_position, current_visual_position);
            current_time_samples += stop_samples;
        }
        index = group_end;
    }

    double chart_end_position = current_position;
    if (!chart.measures.empty()) {
        chart_end_position = std::max(chart_end_position, chart.measures.back().end());
    }

    double remaining = chart_end_position - current_position;
    if (remaining > kPositionEpsilon) {
        if (!std::isfinite(current_bpm) || current_bpm <= 0.0) {
            add_message(result.messages, BmsParseSeverity::Error, static_cast<int>(chart.measures.size()) - 1,
                        "Cannot determine chart duration because BPM is not positive.");
        } else {
            double delta_time_seconds = seconds_from_position_delta(remaining, current_bpm);
            const double segment_start_sample = current_time_samples;
            const double segment_start_position = current_visual_position;
            current_time_samples += delta_time_seconds * sample_rate;
            current_visual_position +=
                delta_time_seconds * reference_bpm /
                (kBeatsPerMeasure * 60.0) * current_scroll;
            append_scroll_segment(segment_start_sample, current_time_samples,
                                  segment_start_position, current_visual_position);
            current_position = chart_end_position;
        }
    }

    result.timeline.duration_samples = static_cast<int64_t>(std::llround(current_time_samples));
    return result;
}

}  // namespace tenriff::chart

