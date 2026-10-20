#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gameplay/GameplayChart.h"

namespace tenriff::app {

enum class ChartFormat {
    Unknown,
    Bms,
};

// Existing replay lane counts distinguish historical native 8K from corrected
// native 7K. Explicit conversions and peer v7 retain their original source
// mapping, so this correction needs no new replay field or protocol version.
[[nodiscard]] bool preserve_legacy_empty_sp_scratch(std::string_view key_mode,
                                                   int replay_lane_count = 0,
                                                   bool peer_battle = false);

struct ChartLoadResult {
    gameplay::GameplayChart chart;
    ChartFormat format = ChartFormat::Unknown;
    double base_bpm = 0.0;
    // Display/Hi-Speed reference only; base_bpm preserves replay/conversion input.
    double reference_bpm = 0.0;
    bool has_bpm_stop_timing_difference = false;
    std::vector<std::string> messages;
    std::string error;

    [[nodiscard]] bool success() const { return error.empty(); }
};

class ChartLoader {
public:
    [[nodiscard]] ChartLoadResult load(const std::string& path,
                                       int sample_rate,
                                       double rate,
                                       std::string_view bms_keysound_policy = "ignore",
                                       uint32_t bms_random_seed = 0,
                                       int bms_timing_version = 2,
                                       int bms_control_flow_version = 2,
                                       bool preserve_empty_sp_scratch = false) const;
};

}  // namespace tenriff::app
