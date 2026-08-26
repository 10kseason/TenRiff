#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace tenriff::app {

inline constexpr int kLanePresentationMaximum = 16;

struct LanePresentationLayout {
    bool seven_plus_one = false;
    int lane_count = 0;
    int source_scratch_lane = 0;
    int visual_scratch_lane = 0;
    // Both arrays use 1-based lane indices. Index zero is intentionally unused.
    std::array<int, kLanePresentationMaximum + 1> source_to_visual{};
    std::array<int, kLanePresentationMaximum + 1> visual_to_source{};

    [[nodiscard]] constexpr int visual_lane_for_source(int lane) const {
        return lane > 0 && lane <= lane_count ? source_to_visual[static_cast<std::size_t>(lane)]
                                              : lane;
    }

    [[nodiscard]] constexpr int source_lane_for_visual(int lane) const {
        return lane > 0 && lane <= lane_count ? visual_to_source[static_cast<std::size_t>(lane)]
                                              : lane;
    }
};

[[nodiscard]] inline LanePresentationLayout resolve_lane_presentation_layout(
    int lane_count,
    const int* scratch_lanes,
    std::size_t scratch_lane_count,
    std::string_view scratch_position) {
    LanePresentationLayout layout;
    layout.lane_count = std::clamp(lane_count, 0, kLanePresentationMaximum);
    for (int lane = 1; lane <= layout.lane_count; ++lane) {
        layout.source_to_visual[static_cast<std::size_t>(lane)] = lane;
        layout.visual_to_source[static_cast<std::size_t>(lane)] = lane;
    }

    if (layout.lane_count != 8 || scratch_lane_count != 1 || scratch_lanes == nullptr ||
        scratch_lanes[0] <= 0 || scratch_lanes[0] > layout.lane_count) {
        return layout;
    }

    layout.seven_plus_one = true;
    layout.source_scratch_lane = scratch_lanes[0];
    layout.visual_scratch_lane = scratch_position == "right" ? layout.lane_count : 1;
    layout.source_to_visual.fill(0);
    layout.visual_to_source.fill(0);
    layout.source_to_visual[static_cast<std::size_t>(layout.source_scratch_lane)] =
        layout.visual_scratch_lane;
    layout.visual_to_source[static_cast<std::size_t>(layout.visual_scratch_lane)] =
        layout.source_scratch_lane;

    int next_visual_lane = 1;
    for (int source_lane = 1; source_lane <= layout.lane_count; ++source_lane) {
        if (source_lane == layout.source_scratch_lane) continue;
        while (next_visual_lane == layout.visual_scratch_lane) ++next_visual_lane;
        layout.source_to_visual[static_cast<std::size_t>(source_lane)] = next_visual_lane;
        layout.visual_to_source[static_cast<std::size_t>(next_visual_lane)] = source_lane;
        ++next_visual_lane;
    }
    return layout;
}

template <typename T>
[[nodiscard]] inline std::vector<T> lane_values_in_visual_order(
    const std::vector<T>& source_values,
    const LanePresentationLayout& layout) {
    if (!layout.seven_plus_one ||
        source_values.size() < static_cast<std::size_t>(layout.lane_count)) {
        return source_values;
    }
    std::vector<T> visual_values = source_values;
    for (int source_lane = 1; source_lane <= layout.lane_count; ++source_lane) {
        const int visual_lane = layout.visual_lane_for_source(source_lane);
        visual_values[static_cast<std::size_t>(visual_lane - 1)] =
            source_values[static_cast<std::size_t>(source_lane - 1)];
    }
    return visual_values;
}

template <typename T>
[[nodiscard]] inline std::vector<T> lane_gap_values_in_visual_order(
    const std::vector<T>& source_values,
    const LanePresentationLayout& layout) {
    if (!layout.seven_plus_one || layout.source_scratch_lane != 1 ||
        layout.visual_scratch_lane != layout.lane_count ||
        source_values.size() < static_cast<std::size_t>(layout.lane_count - 1)) {
        return source_values;
    }
    std::vector<T> visual_values = source_values;
    for (int visual_gap = 0; visual_gap < layout.lane_count - 2; ++visual_gap) {
        visual_values[static_cast<std::size_t>(visual_gap)] =
            source_values[static_cast<std::size_t>(visual_gap + 1)];
    }
    visual_values[static_cast<std::size_t>(layout.lane_count - 2)] = source_values.front();
    return visual_values;
}

}  // namespace tenriff::app
