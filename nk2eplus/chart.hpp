#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "note.hpp"
#include "timing.hpp"

namespace keyweaver_eplus {

struct RawChartData {
    std::map<std::string, std::vector<std::string>> sections;
    std::vector<std::string> sectionOrder;
    std::vector<std::string> preamble;
    std::vector<std::string> originalLines;
};

struct ChartMeta {
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> creator;
    std::optional<std::string> version;
    int sourceKeyCount = 0;
    std::optional<int> targetKeyCount;
    std::string format = "osu";
    std::optional<int> mode;
};

struct Chart {
    ChartMeta meta;
    std::vector<TimingPoint> timingPoints;
    std::vector<Note> notes;
    RawChartData raw;
    std::vector<std::string> warnings;
};

}  // namespace keyweaver_eplus

