#pragma once

#include "config/SimpleJson.h"
#include "gameplay/TimingAnalysis.h"

namespace tenriff::gameplay {

// Optional observational metadata under stats.analysis_v1. This schema does not
// change the official score/replay versions or participate in replay validation.
// Serialize with max_digits10 precision to preserve counters and timing moments.
[[nodiscard]] config::JsonValue build_timing_analysis_json(const TimingAnalysis& analysis);

// Missing, unsupported, or inconsistent metadata yields unavailable analysis;
// old aggregate timing fields are never expanded into invented measurements.
[[nodiscard]] TimingAnalysis parse_timing_analysis_json(const config::JsonValue* value);

}  // namespace tenriff::gameplay
