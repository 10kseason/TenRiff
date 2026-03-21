#include "chart/NoteLaneMapping.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace tenriff::chart {

namespace {
std::string normalize_channel(std::string_view channel) {
    std::string normalized(channel);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return normalized;
}
}

NoteLaneMapping::NoteLaneMapping(std::unordered_map<std::string, std::size_t> channel_to_lane) {
    setMapping(std::move(channel_to_lane));
}

NoteLaneMapping NoteLaneMapping::TenKeyDualPlayerDefault() {
    return NoteLaneMapping({
        {"11", 1},
        {"12", 2},
        {"13", 3},
        {"14", 4},
        {"15", 5},
        {"21", 6},
        {"22", 7},
        {"23", 8},
        {"24", 9},
        {"25", 10},
        {"51", 1},
        {"52", 2},
        {"53", 3},
        {"54", 4},
        {"55", 5},
        {"61", 6},
        {"62", 7},
        {"63", 8},
        {"64", 9},
        {"65", 10},
    });
}

void NoteLaneMapping::setMapping(std::unordered_map<std::string, std::size_t> channel_to_lane) {
    channel_to_lane_.clear();
    for (auto& entry : channel_to_lane) {
        auto normalized_channel = normalize_channel(entry.first);
        if (normalized_channel.empty()) {
            continue;
        }
        channel_to_lane_.emplace(std::move(normalized_channel), entry.second);
    }
}

std::optional<std::size_t> NoteLaneMapping::laneForChannel(std::string_view channel) const {
    auto normalized_channel = normalize_channel(channel);
    auto it = channel_to_lane_.find(normalized_channel);
    if (it == channel_to_lane_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace tenriff::chart
