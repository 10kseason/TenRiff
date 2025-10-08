#pragma once

#include <string>
#include <vector>

#include "chart/BmsChartNorm.h"

namespace tenriff::chart {

struct BmsScheduledEvent {
    BmsNormalizedEvent event;
    double time = 0.0;
};

struct BmsTimeline {
    double duration = 0.0;
    std::vector<BmsScheduledEvent> events;
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
    BmsTimelineResult build(const BmsNormalizedChart& chart) const;
};

}  // namespace tenriff::chart

