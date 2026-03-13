#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "chart/NoteLaneMapping.h"

namespace tenriff::chart {

struct BmsParserOptions {
    bool tolerant = false;
};

enum class BmsParseSeverity {
    Info,
    Warning,
    Error,
};

struct BmsParseMessage {
    BmsParseSeverity severity;
    std::size_t line;  // 1-based line number
    std::string text;
};

struct BmsMeasureCommand {
    int measure = 0;
    std::string channel;
    std::string data;
};

struct BmsChart {
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> wav;
    std::unordered_map<std::string, std::string> bmp;
    std::unordered_map<std::string, double> bpm;
    std::unordered_map<std::string, double> stop;
    double base_bpm = 0.0;
    std::vector<BmsMeasureCommand> commands;
    NoteLaneMapping lane_mapping = NoteLaneMapping::TenKeyDualPlayerDefault();
};

struct BmsParseResult {
    BmsChart chart;
    std::vector<BmsParseMessage> messages;

    bool success() const;
};

class BmsParser {
public:
    BmsParseResult parse(std::string_view content, const BmsParserOptions& options = {}) const;
    BmsParseResult parseFile(const std::string& path, const BmsParserOptions& options = {}) const;
};

}  // namespace tenriff::chart
