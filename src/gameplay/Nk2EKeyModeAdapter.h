#pragma once

#include "gameplay/KeyModeConverter.h"

namespace tenriff::gameplay {
// The E engine is separate from the frozen historical nK2 replay path.
[[nodiscard]] KeyModeConverterResult convert_key_mode_chart_nk2_e(
    const GameplayChart& chart, const KeyModeConverterOptions& options);
}
