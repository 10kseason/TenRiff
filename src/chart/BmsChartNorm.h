#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "chart/BmsParser.h"

namespace tenriff::chart {

enum class BmsNormalizedEventType {
    Unknown,
    Bgm,
    Bga,
    Poor,
    Note,
    BpmChange,
    Stop,
};

struct BmsMeasureTiming {
    double start = 0.0;
    double length = 1.0;

    [[nodiscard]] double end() const noexcept { return start + length; }
};

struct BmsNormalizedEvent {
    BmsNormalizedEventType type = BmsNormalizedEventType::Unknown;
    int measure = 0;
    std::size_t slice_index = 0;
    std::size_t slice_count = 0;
    double intra_measure = 0.0;
    double position = 0.0;
    std::string channel;
    std::string object_id;
    std::optional<std::size_t> lane;
    std::optional<double> value;
};

struct BmsNormalizedChart {
    std::vector<BmsMeasureTiming> measures;
    std::vector<BmsNormalizedEvent> events;
    NoteLaneMapping lane_mapping;
};

struct BmsNormalizationMessage {
    BmsParseSeverity severity;
    int measure;
    std::string text;
};

struct BmsNormalizationResult {
    BmsNormalizedChart chart;
    std::vector<BmsNormalizationMessage> messages;

    [[nodiscard]] bool success() const;
};

class BmsChartNormalizer {
public:
    BmsNormalizationResult normalize(const BmsChart& chart) const;
};

}  // namespace tenriff::chart
