#pragma once

#include <string>
#include <vector>

#include "chart/BmsParser.h"
#include "chart/BmsTimeline.h"
#include "gameplay/GameplayChart.h"

namespace tenriff::app {

struct BmsGameplayBuildResult {
    gameplay::GameplayChart chart;
    std::vector<std::string> note_object_ids;
    std::vector<std::string> messages;
};

[[nodiscard]] BmsGameplayBuildResult build_bms_gameplay_chart(const chart::BmsTimeline& timeline,
                                                              const chart::BmsChart& parsed_chart,
                                                              double rate);

}  // namespace tenriff::app
