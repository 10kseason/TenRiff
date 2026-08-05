#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "chart/BmsChartNorm.h"

namespace tenriff::chart {

struct BmsScheduledEvent {
    BmsNormalizedEvent event;
    int64_t time_samples = 0;
};

struct BmsScrollSegment {
    int64_t start_sample = 0;
    int64_t end_sample = 0;
    double start_position = 0.0;
    double end_position = 0.0;
};


struct BmsTimeline {
    int64_t duration_samples = 0;
    std::vector<BmsScheduledEvent> events;
    std::vector<BmsScrollSegment> scroll_segments;
};

struct BmsTimelineMessage {
    BmsParseSeverity severity;
    int measure;
    std::string text;
};

struct BmsTimelineResult {
    BmsTimeline timeline;
    std::vector<BmsTimelineMessage> messages;

    [[nodiscard]] bool success() const;
};

class BmsTimelineBuilder {
public:
    BmsTimelineResult build(const BmsNormalizedChart& chart, int sample_rate_hz) const;
};

}  // namespace tenriff::chart

