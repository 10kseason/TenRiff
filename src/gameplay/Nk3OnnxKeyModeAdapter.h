#pragma once

#include "gameplay/KeyModeConverter.h"

namespace tenriff::gameplay {

[[nodiscard]] constexpr bool nk3_pattern_mlp_route_enabled(int source_lane_count,
                                                           int target_lane_count) {
    return source_lane_count > 0 && source_lane_count != 10 && target_lane_count == 10;
}

[[nodiscard]] KeyModeConverterResult
convert_key_mode_chart_nk3_onnx(const GameplayChart& chart,
                                const KeyModeConverterOptions& options);

}  // namespace tenriff::gameplay
