#pragma once

#include <optional>
#include <string>

namespace keyweaver_e {

struct TimingPoint {
    int time = 0;
    double beatLength = 0.0;
    std::optional<int> meter;
    std::optional<int> sampleSet;
    std::optional<int> sampleIndex;
    std::optional<int> volume;
    std::optional<bool> uninherited;
    std::optional<int> effects;
    std::string rawLine;
};

}  // namespace keyweaver_e

