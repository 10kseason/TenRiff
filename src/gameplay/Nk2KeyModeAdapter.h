#pragma once

#include "gameplay/KeyModeConverter.h"

namespace tenriff::gameplay {

[[nodiscard]] KeyModeConverterResult
convert_key_mode_chart_nk2(const GameplayChart &chart,
                           const KeyModeConverterOptions &options);

} // namespace tenriff::gameplay
