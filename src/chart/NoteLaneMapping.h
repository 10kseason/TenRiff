#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tenriff::chart {

class NoteLaneMapping {
public:
    NoteLaneMapping() = default;
    explicit NoteLaneMapping(std::unordered_map<std::string, std::size_t> channel_to_lane);

    static NoteLaneMapping TenKeyDualPlayerDefault();

    void setMapping(std::unordered_map<std::string, std::size_t> channel_to_lane);

    [[nodiscard]] std::optional<std::size_t> laneForChannel(std::string_view channel) const;

    [[nodiscard]] const std::unordered_map<std::string, std::size_t>& mapping() const noexcept {
        return channel_to_lane_;
    }

private:
    std::unordered_map<std::string, std::size_t> channel_to_lane_;
};

}  // namespace tenriff::chart
